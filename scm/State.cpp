#include <State.h>
#include <StateMachine.h>

#include <cassert>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstdlib>

using namespace std;

namespace SCM {

const string done_state_prefix = "done.state.";
extern size_t splitStringToVector (std::string const &valstr, std::vector<std::string> &val, size_t max_s=0xffffffff, std::string const&separators=", \t");

struct State::PRIVATE
{
    State       *self_;
    Transition  *leaving_target_transition_;
    float        leaving_delay_;
    float        leaving_elapsed_seconds_;

    PRIVATE (State *state)
        : self_(state)
        , leaving_delay_(0)
        , leaving_elapsed_seconds_(0)
        , leaving_target_transition_(0)
    {
    }

    ~PRIVATE ()
    {
    }

    void connect_transitions_signal (std::vector<boost::shared_ptr<Transition> > &transitions);
    void connect_transitions_conds (std::vector<boost::shared_ptr<Transition> > &transitions);
};


State::State (std::string const& state_id, State* parent, StateMachine *machine)
    : parent_(parent), state_id_(state_id), current_state_(0), machine_(machine), depth_(0)
    , is_a_final_(false), done_(false), active_(false), is_unique_id_(true), slots_ready_(false)
{
    private_ = new PRIVATE(this);

    if (state_id_.empty()) { // anonymous state
        ostringstream stream;
        stream << "_st" << machine_->num_of_states();;
        state_id_ = stream.str();
    } else {
        is_unique_id_ = machine_->is_unique_id(state_id_);
    }
    
    if (parent_) {
        this->depth_ = parent_->depth_ + 1;
    }

    if (this->is_unique_id_) {
        state_uid_ = this->state_id_;
    } else {
        state_uid_ = parent_->state_uid() + "." + this->state_id_;
    }

    if (machine_ != this) machine_->addState (this);
}

State::~State ()
{
    machine_->removeState (this);
    this->transitions_.clear ();
    this->no_event_transitions_.clear ();
    clear_substates ();

    delete private_;
}

State *State::clone (State *parent, StateMachine *m)
{
    State *state = new State (state_id_, parent, m);

    state->clone_data (this);

    state->autorelease ();

    return state;
}

void State::clone_data (State *rhs)
{
    for (size_t i=0; i < rhs->substates_.size (); ++i) {
        substates_.push_back (rhs->substates_[i]->clone (this, machine_));
        substates_.back ()->retain ();
    }
    depth_ = rhs->depth_;
    is_a_final_ = rhs->is_a_final_;
    private_->leaving_delay_ = rhs->private_->leaving_delay_;
    is_unique_id_ = rhs->is_unique_id_;
}

void State::clear_substates ()
{
    for (size_t i=0; i < substates_.size (); ++i) {
        substates_[i]->clear_substates ();
        substates_[i]->release();
    }

    substates_.clear ();
}

void  State::reset_history ()
{
    if (!machine_->with_history_) return;

    if (!machine_->history_type(this->state_uid()).empty()) {
        this->clearHistory ();
    }

    for (size_t i=0; i < substates_.size (); ++i) {
        substates_[i]->reset_history ();
    }
}

bool State::trig_cond (Transition const &tran) const
{
    if (tran.cond_functor_) {
        bool change = tran.cond_functor_();
        if (tran.attr_->not_) change = !change;
        return change;
    } else if (!tran.attr_->in_state_.empty ()) {
        bool change = machine_->inState (tran.attr_->in_state_);
        if (tran.attr_->not_) change = !change;
        return change;
    } else {
        return true;
    }
}

void State::enterState (bool enter_substate)
{
    if (active_) return;
    
    if (parent_ && !machine_->history_type (parent_->state_uid()).empty ()) {
        parent_->history_state_id_ = this->state_uid();
    }

    this->reset_time ();

    this->done_ = false;
    active_ = true;

    machine_->current_leaf_state_ = this;
    signal_onentry ();

    if (!this->active_) { // in case state changed immediately at last signal_onentry.
        return;
    }

    if (enter_substate) {
        doEnterSubState();
    }

    if (is_a_final_ && parent_) {
        parent_->done_ = true;
        parent_->signal_done ();
        machine_->enqueEvent (done_state_prefix + parent_->state_uid());
    }
}

void State::exitState ()
{
    if (!active_) return;
    
    if (this->current_state_) {
        this->current_state_->exitState ();
    }

    active_ = false;
    machine_->current_leaf_state_ = this;
    this->current_state_ = 0;

    signal_onexit ();
}

void State::onEvent (string const &e)
{
    if (this->done_) {
        return;
    }

    if (this->isLeavingState()) {
        return;
    }

    for (size_t i=0; i < transitions_.size (); ++i) {
        Transition const &tran = *transitions_[i];
        if (tran.attr_->event_ == e) {
            bool change = trig_cond (tran);
            if (change) {
                this->changeState (tran);
                return;
            }
        }
    }

    if (this->current_state_) {
        this->current_state_->onEvent (e);
    }
}

string State::initial_state() const
{
    string const&inits = machine_->initial_state_of_state(state_uid());
    if (!inits.empty()) {
        return inits;
    } else if (!substates_.empty()) {
        return substates_[0]->state_uid();
    } else {
        return ""; // a leaf state has not initial.
    }
}


State* State::findState(const string& state_id, const State *exclude, bool check_parent) const
{
    for (size_t i=0; i < substates_.size(); ++i) {
        if (substates_[i]->state_id () == state_id) {
            return substates_[i];
        }
    }
    
    for (size_t i=0; i < substates_.size(); ++i) {
        if (substates_[i] != exclude) {
            State *st = substates_[i]->findState(state_id, exclude, false);
            if (st) return st;
        }
    }
    
    if (check_parent && parent_) {
        State *st = parent_->findState(state_id, this, true);
        if (st) return st;
    }
    
    return 0;
}

float State::leavingDelay () const
{
    return private_->leaving_delay_;
}

bool State::isLeavingState () const
{
    return private_->leaving_target_transition_;
}

void State::setLeavingDelay (float delay)
{
    private_->leaving_delay_ = delay;
}

void State::clearHistory ()
{
    this->history_state_id_ = "";
}

void State::clearDeepHistory ()
{
    this->history_state_id_ = "";
    for (size_t i=0; i < this->substates_.size (); ++i) {
        this->substates_[i]->clearDeepHistory ();
    }
}

void State::changeState (Transition const &transition)
{
    std::string target;
    if (!transition.attr_->random_target_.empty ()) {
        int index = rand()%transition.attr_->random_target_.size ();
        target = transition.attr_->random_target_[index];
    } else {
        target = transition.attr_->transition_target_;
    }

    std::string const& corresponding_state = machine_->state_id_of_history(target);
    if (!corresponding_state.empty()) {
        State *st = machine_->getState (corresponding_state);
        std::string const& h = st->history_state_id_;
        if (!h.empty()) {
            target = h;
        } else  {
            target = st->initial_state ();
        }
    }

    machine_->transition_source_state_ = this->state_uid();
    machine_->transition_target_state_ = target;

    if (!private_->leaving_target_transition_) {
        if (private_->leaving_delay_ != 0) {
            private_->leaving_target_transition_ = new Transition;
            TransitionAttr *attr = new TransitionAttr (transition.attr_->event_, target);
            private_->leaving_target_transition_->setAttr (attr);
            attr->release ();
            if (!transition.attr_->ontransit_.empty ()) {
                boost::function<void()> s;
                if (this->machine_->GetActionSlot (transition.attr_->ontransit_, s)) {
                    private_->leaving_target_transition_->signal_transit.connect (s);
                }
            }
            return;
        }
    }
    
    vector<State *> newState;
    
    if (target.find(',') == string::npos) {
        State *st = machine_->getState (target);
        if (st) {
            newState.push_back(st);
        } else {
            assert (0 && "can't find transition target state!");
        }
    } else {
        vector<string> targets;
        splitStringToVector(target, targets);
        for (size_t i=0; i < targets.size(); ++i) {
            State *st = machine_->getState (targets[i]);
            if (st) {
                newState.push_back(st);
            } else {
                assert (0 && "can't find transition target state!");
            }
        }
    }
    
    if (newState.empty()) {
        assert (0 && "can't find transition target state!");
        return;
    }

    State *lcaState = this->findLCA (newState[0]);

    if (!lcaState) {
        assert (lcaState && "can't find common ancestor state.");
        return;
    }
    
    // make sure they are in the same parallel state?
    if (newState.size() > 1) {
        for (size_t i=1; i < newState.size(); ++i) {
            State *lca = this->findLCA (newState[i]);
            if (lca != lcaState) {
                assert (lca && "multiple targets but can't find common ancestor.");
                return;
            }
        }
    }

    // exit old states
    if (newState.size() == 1 && lcaState == newState[0]) { // for reentering
        machine_->transition_source_state_ = lcaState->state_uid();
        lcaState->exitState ();
        transition.signal_transit ();
        lcaState->enterState ();
        return;
    } else if (lcaState->current_state_) {
        machine_->transition_source_state_ = lcaState->current_state_->state_uid();
        lcaState->current_state_->exitState ();
    }

    // transition
    transition.signal_transit ();

    // enter new state
    vector<State *> vps;
    for (size_t i=0; i < newState.size(); ++i) {
        vps.push_back (newState[i]);
        State *parentstate = newState[i]->parent_;
        while (parentstate && (parentstate != lcaState)) {
            vps.push_back (parentstate);
            parentstate = parentstate->parent_;
        }
    }
    while (!vps.empty()) {
        lcaState->doEnterState (vps);
    }
        
    lcaState->makeSureEnterStates();
}

void State::doEnterState (std::vector<State *> &vps)
{
    State *state = vps.back ();
    if (state->depth_ <= this->depth_) return;
    
    vps.pop_back ();
    this->current_state_ = state;
    this->current_state_->enterState (false);
    if (!vps.empty ()) {
        this->current_state_->doEnterState (vps);
    }
}

void State::doEnterSubState()
{
    if (!this->history_state_id_.empty() && !machine_->history_type(this->state_uid()).empty()) {
        this->current_state_ = machine_->getState (this->history_state_id_);
        this->current_state_->enterState ();
    } else {
        if (!substates_.empty ()) {
            const string &initial_state = machine_->initial_state_of_state(this->state_uid());
            if (initial_state.empty ()) {
                this->current_state_ = this->substates_.front ();
                this->current_state_->enterState ();
            } else {
                Transition tran;
                TransitionAttr *attr = new TransitionAttr ("", initial_state);
                tran.setAttr (attr);
                attr->release ();
                this->changeState (tran);
            }
        }
    }
}


State *State::findLCA (State const* ots)
{
    assert (ots && "State::findLCA on invalid state object");
    if (this == ots) {
        return this;
    } else if (this->depth_ > ots->depth_) {
        return this->parent_->findLCA (ots);
    } else if (this->depth_ < ots->depth_) {
        return this->findLCA (ots->parent_);
    } else {
        if (this->parent_ == ots->parent_) {
            return this->parent_;
        } else {
            return this->parent_->findLCA (ots->parent_);
        }
    }

    return 0;
}

bool State::inState (std::string const& state_id, bool recursive) const
{
    if (!current_state_) return false;
    if (current_state_->state_id_ == state_id) {
        return true;
    } else if (recursive) {
        return current_state_->inState (state_id, recursive);
    } else {
        return false;
    }
}

bool State::inState (State const*state, bool recursive) const
{
    if (!current_state_) return false;
    if (current_state_ == state) {
        return true;
    } else if (recursive) {
        return current_state_->inState (state, recursive);
    } else {
        return false;
    }
}

void State::makeSureEnterStates()
{
    this->enterState(true);
    if (!current_state_ && !substates_.empty()) {
        this->doEnterSubState();
    }
    if (current_state_) {
        current_state_->makeSureEnterStates();
    }
}


void State::prepareActionCondSlots ()
{
    if (this->slots_ready_) {
        return;
    }

    this->slots_ready_ = true;

    std::vector<TransitionAttr *> const&tran_attrs = machine_->transition_attr(this->state_uid());
    size_t size = tran_attrs.size ();
    transitions_.reserve(size);
    no_event_transitions_.reserve(size);
    for (size_t i=0; i < tran_attrs.size(); ++i) {
        boost::shared_ptr<Transition> ptr (new Transition);
        if (tran_attrs[i]->event_.empty()) {
            no_event_transitions_.push_back (ptr);
            no_event_transitions_.back()->setAttr (tran_attrs[i]);
        } else {
            transitions_.push_back (ptr);
            transitions_.back()->setAttr (tran_attrs[i]);
        }
    }
    std::vector<boost::shared_ptr<Transition> > (transitions_).swap(transitions_);
    std::vector<boost::shared_ptr<Transition> > (no_event_transitions_).swap(no_event_transitions_);

    // add clear history action
    this->machine_->setActionSlot ("clh(" + state_uid() + "*)", boost::bind(&State::clearDeepHistory, this));
    this->machine_->setActionSlot ("clh(" + state_uid() + ")", boost::bind(&State::clearHistory, this));

    for (size_t i=0; i < this->substates_.size (); ++i) {
        this->substates_[i]->prepareActionCondSlots ();
    }

}

void State::connectCondSlots ()
{
    private_->connect_transitions_conds(transitions_);
    private_->connect_transitions_conds(no_event_transitions_);

    for (size_t i=0; i < this->substates_.size (); ++i) {
        this->substates_[i]->connectCondSlots ();
    }
}

void State::PRIVATE::connect_transitions_conds(vector< boost::shared_ptr< Transition > >& transitions)
{
    for (size_t i=0; i < transitions.size (); ++i) {
        if (!transitions[i]->attr_->cond_.empty ()) {
            std::string cond = transitions[i]->attr_->cond_;
            std::string instate_check = cond.substr (0, 3);
            if (instate_check == "In(" || instate_check == "in(") {
                string::size_type endmark = cond.find_first_of (')', 4);
                string st = cond.substr (3, endmark - 3);
                if (!self_->machine_->is_unique_id(st)) {
                    State *s = self_->findState(st);
                    if (!s) {
                        assert (0 && "can't find state for In() check.");
                        continue;
                    }
                    st = s->state_uid();
                }
                transitions[i]->attr_->in_state_ = st;
            } else {
                boost::function<bool()> s;
                if (self_->machine_->GetCondSlot (cond, s)) {
                    transitions[i]->cond_functor_ = s;
                } else {
                    assert (0 && "can't connect cond slot");
                }
            }
        }
    }

}


void State::connectActionSlots ()
{
    boost::function<void()> s;
    string const&onentry = machine_->onentry_action(this->state_uid());
    if (!onentry.empty ()) {
        if (this->machine_->GetActionSlot (onentry, s)) {
            this->signal_onentry.connect (s);
        } else if (onentry != "onentry_"+state_uid()) {
            assert (0 && "can't connect onentry slot");
        }
    }

    string const&onexit = machine_->onexit_action(this->state_uid());
    if (!onexit.empty ()) {
        if (this->machine_->GetActionSlot (onexit, s)) {
            this->signal_onexit.connect (s);
        } else if (onexit != "onexit_"+state_uid())  {
            assert (0 && "can't connect onexit slot");
        }
    }

    string const &frame_move = machine_->frame_move_action(this->state_uid());
    if (!frame_move.empty ()) {
        boost::function<void(float)> sf;
        if (this->machine_->GetFrameMoveSlot (frame_move, sf)) {
            frame_move_slots_.push_back(sf);
        } else if (frame_move != state_uid()) { // specified frame_move slot but not found
            assert (0 && "can't connect frame_move slot");
        }
    }

    private_->connect_transitions_signal (transitions_);
    private_->connect_transitions_signal (no_event_transitions_);

    for (size_t i=0; i < this->substates_.size (); ++i) {
        this->substates_[i]->connectActionSlots ();
    }
}

void State::PRIVATE::connect_transitions_signal(vector< boost::shared_ptr< Transition > >& transitions)
{
    for (size_t i=0; i < transitions.size (); ++i) {
        if (!transitions[i]->attr_->ontransit_.empty ()) {
            string const&tr = transitions[i]->attr_->ontransit_;
            if (!tr.empty ()) {
                boost::function<void()> s;
                if (self_->machine_->GetActionSlot (tr, s)) {
                    transitions[i]->signal_transit.connect (s);
                } else {
                    assert (0 && "can't connect on_transit slot");
                }
            }
        }
    }
}

void State::doLeaveAferDelay ()
{
    if (private_->leaving_target_transition_) {
        // change state
        this->changeState (*private_->leaving_target_transition_);
        delete private_->leaving_target_transition_;
        private_->leaving_target_transition_ = 0;
        private_->leaving_elapsed_seconds_ = 0;
    }
}

bool State::check_leaving_state_finished(float t)
{
    if (private_->leaving_delay_ < 0) return false;
    private_->leaving_elapsed_seconds_ += t;
    if (private_->leaving_elapsed_seconds_ >= private_->leaving_delay_) {
        doLeaveAferDelay ();
        return true;
    }
    return false;
}

void State::onFrameMove (float t)
{
    if (this->isLeavingState()) {
        if (this->check_leaving_state_finished (t)) return;
    }

    if (this->current_state_) {
        this->current_state_->frame_move (t);
        if (!this->active_) return;
    }

    this->pumpNoEvents ();

    if (!this->active_) return;

    for (size_t i=0; i < frame_move_slots_.size(); ++i) {
        frame_move_slots_[i] (t);
    }
}

void State::pumpNoEvents()
{
    for (size_t i=0; i < no_event_transitions_.size (); ++i) {
        Transition const &tran = *no_event_transitions_[i];
        if (trig_cond (tran)) {
            this->changeState (tran);
            return;
        }
    }
}

}


#include "StateMachine.h"
#include "StateMachineManager.h"
#include <sstream>
#include <cassert>
#include <iostream>
using namespace std;

namespace scm {

namespace {
    struct StateTimedEventTypeCmpP
    {
        bool operator () (TimedEventType *lhs, TimedEventType *rhs) const
        {
            return lhs->time_ < rhs->time_;
        }
    };
}


struct StateMachine::PRIVATE
{
    StateMachine                 *mach_;
    std::list <TimedEventType *> timed_events_;
    std::vector<std::string>     queued_events_;
    
    PRIVATE(StateMachine *mach)
    : mach_(mach)
    {
    }
    
    ~PRIVATE ()
    {
    }

    void loadSCXMLString (std::string const&xmlstr);

};

StateMachine::StateMachine (StateMachineManager *manager)
    : super ("", 0, this)
    , manager_(manager)
    , slots_prepared_(false)
    , slots_connected_(false)
    , scxml_loaded_(false)
    , engine_started_(false)
    , on_event_(false)
    , with_history_(false)
    , current_leaf_state_(0)
    , frame_move_slots_(0)
    , cond_slots_(0)
    , action_slots_(0)
    , allow_nop_entry_exit_slot_(false)
    , do_exit_state_on_destroy_(false)
{
    private_ = new PRIVATE(this);
    this->addState (this);
}

StateMachine::~StateMachine ()
{
    this->clearTimedEvents ();    
    destroy_machine (do_exit_state_on_destroy_);
    delete private_;
}

StateMachine* StateMachine::clone ()
{
    StateMachine *mach = new StateMachine (manager_);
    mach->autorelease ();

    mach->state_id_ = this->state_id_;
    mach->scxml_id_ = this->scxml_id_;
    mach->scxml_loaded_ = this->scxml_loaded_;

    mach->machine_ = mach;
    mach->clone_data (this);

    return mach;
}

bool StateMachine::is_unique_id(std::string const&state_id) const
{
    return !manager_ || manager_->is_unique_id(scxml_id_, state_id);

}

State* StateMachine::getState(std::string const& state_uid) const
{
    map<std::string, State*>::const_iterator it = states_map_.find(state_uid);
    if (it == states_map_.end()) return 0;
    return it->second;
}

void StateMachine::addState (State *state)
{
    assert (state && "add state error!");
    if (!state) return;
    string uid = state->state_uid();    
    states_map_[uid] = state;
}

void StateMachine::removeState (State *state)
{
    assert (state && "remove state error!");
    if (!state || state == this) return;
    if (states_map_[state->state_uid()] == state) states_map_.erase(state->state_uid());
}

bool StateMachine::inState (std::string const &state_uid) const
{
    map<std::string, State*>::const_iterator it = states_map_.find(state_uid);
    if (it == states_map_.end()) return false;
    return it->second->active();
}

void StateMachine::onFrameMove (float t)
{
    if (!slots_connected_) return;
    State::onFrameMove (t);
    pumpTimedEvents();
    while (!private_->queued_events_.empty()) {
        pumpQueuedEvents ();
    }
}

void StateMachine::pumpQueuedEvents ()
{
    if (private_->queued_events_.empty ()) {
        return;
    }

    std::vector<string> events;
    events.swap(private_->queued_events_);
    for (size_t i=0; i < events.size (); ++i) {
        this->onEvent (events[i]);
    }
}

void StateMachine::enqueEvent(string const&e)
{
    private_->queued_events_.push_back (e);
    manager_->addToActiveMach (this);
}


void StateMachine::onEvent(string const&e)
{
    if (!this->slots_connected_) {
        assert (0 && " slots not connected");
        return;
    }

    if (on_event_) {
        this->enqueEvent (e);
        return;
    }

    on_event_ = true;
    State::onEvent (e);
    on_event_ = false;
}

void StateMachine::prepareEngine ()
{
    assert (scxml_loaded_ && "no scxml loaded");
    if (!scxml_loaded_) {
        assert (0 && "scxml not loaded!");
        return;
    }
    this->prepare_slots ();
    this->connect_slots ();
}

void StateMachine::StartEngine ()
{
    assert (scxml_loaded_ && "no scxml loaded");
    if (engine_started_) return;
    prepareEngine ();
    engine_started_ = true;
    this->enterState ();
}

void StateMachine::ReStartEngine ()
{
    assert (scxml_loaded_ && "no scxml loaded");
    if (engine_started_) {
        ShutDownEngine(true);
    }
    StartEngine();
}

bool StateMachine::engineStarted() const
{
    return engine_started_;
}


void StateMachine::ShutDownEngine (bool do_exit_state)
{
    if (do_exit_state) this->exitState();
    engine_started_ = false;
}

bool StateMachine::engineReady () const
{
    return scxml_loaded_;
}

bool StateMachine::isLeavingState () const
{
    return this->getEnterState ()->isLeavingState ();
}

void StateMachine::prepare_slots ()
{
    if (slots_prepared_) return;

    slots_prepared_ = true;
    prepareActionCondSlots ();
    onPrepareActionCondSlots ();
    signal_prepare_slots_ ();
}

void StateMachine::connect_slots ()
{
    if (slots_connected_) {
        return;
    }
    slots_connected_ = true;

    connectCondSlots ();
    onConnectCondSlots ();
    signal_connect_cond_slots_ ();

    connectActionSlots ();
    onConnectActionSlots ();
    signal_connect_action_slots_ ();
}

void StateMachine::clear_slots ()
{
    slots_connected_ = false;

    if (action_slots_) {
        this->action_slots_->clear ();
    }

    if (cond_slots_) this->cond_slots_->clear ();

    if (frame_move_slots_) {
        this->frame_move_slots_->clear ();
    }

    this->transitions_.clear ();
}

void StateMachine::destroy_machine (bool do_exit_state)
{
    if (slots_connected_) {
        if (do_exit_state) this->exitState ();
        scxml_loaded_ = false;
    }
    private_->queued_events_.clear ();
    states_map_.clear();
    machine_clear_substates ();
    clear_slots ();
    reset_history ();

// clean machine
    delete frame_move_slots_;
    delete cond_slots_;
    delete action_slots_;

    frame_move_slots_ = 0;
    cond_slots_ = 0;
    action_slots_ = 0;

    engine_started_ = false;
}

void StateMachine::PRIVATE::loadSCXMLString (std::string const&xmlstr)
{
    if (mach_->scxml_loaded_) mach_->destroy_machine ();
    mach_->scxml_loaded_ = mach_->manager_->loadMachFromString (mach_, xmlstr);
    if (!mach_->scxml_loaded_)
        mach_->destroy_machine ();
    mach_->onLoadScxmlFailed ();
    assert ("load scxml string failed." && 0);
}


bool StateMachine::GetCondSlot (string const&name, boost::function<bool ()> &s)
{
    if (!cond_slots_) return false;

    StateMachine::cond_slot_map::iterator it = this->cond_slots_->find (name);
    if (it != this->cond_slots_->end ()) {
        s = it->second;
        return true;
    }

    return false;
}


bool StateMachine::GetActionSlot (string const&name, boost::function<void ()> &s)
{
    if (!action_slots_) return false;

    StateMachine::action_slot_map::iterator it = this->action_slots_->find (name);
    if (it != this->action_slots_->end ()) {
        s = it->second;
        return true;
    }

    return false;
}

bool StateMachine::GetFrameMoveSlot (std::string const&name, boost::function<void (float)> &s)
{
    if (!frame_move_slots_) return false;

    StateMachine::frame_move_map::iterator it = this->frame_move_slots_->find (name);
    if (it != this->frame_move_slots_->end ()) {
        s = it->second;
        return true;
    }

    return false;
}

void StateMachine::setCondSlot (std::string const&name, boost::function<bool ()> const &s)
{
    if (!cond_slots_) {
        cond_slots_ = new cond_slot_map;
    }

    (*cond_slots_)[name] = s;
}

void StateMachine::setActionSlot (std::string const&name, boost::function<void ()> const &s)
{
    if (!action_slots_) {
        action_slots_ = new action_slot_map;
    }

    (*action_slots_)[name] = s;
}

void StateMachine::setFrameMoveSlot (std::string const&name, boost::function<void (float)> const &s)
{
    if (!frame_move_slots_) {
        frame_move_slots_ = new frame_move_map;
    }

    (*frame_move_slots_)[name] = s;
}

TimedEventType * StateMachine::registerTimedEvent (float after_t, string const&event_e, bool extern_manage)
{
    TimedEventType * p = new TimedEventType(after_t + total_elapsed_time_, event_e, extern_manage);
    list <TimedEventType *>::iterator it = std::upper_bound (private_->timed_events_.begin (), private_->timed_events_.end (), p, StateTimedEventTypeCmpP());
    private_->timed_events_.insert (it, p);
    return p;
}

void StateMachine::clearTimedEvents ()
{
    list <TimedEventType *>::iterator it = private_->timed_events_.begin ();
    list <TimedEventType *>::iterator it_end = private_->timed_events_.end ();
    for (; it != it_end ; ++it) {
        (*it)->release ();
    }
    private_->timed_events_.clear ();
}

void StateMachine::pumpTimedEvents ()
{
    if (!private_->timed_events_.empty ()) {
        list <TimedEventType *>::iterator it = private_->timed_events_.begin ();
        for (; it != private_->timed_events_.end () ;) {
            if ((*it)->time_ <= this->total_elapsed_time_) {
                if (!(*it)->extern_manage_ || !(*it)->unique_ref ()) {
                    machine_->enqueEvent ((*it)->event_);
                }
                (*it)->release ();
                private_->timed_events_.erase (it++);
            } else {
                break;
            }
        }
    }
}

std::string const& StateMachine::state_id_of_history(const string& history_id) const
{
    return manager_->history_id_resided_state(scxml_id_, history_id);
}

std::string const& StateMachine::history_type(std::string const& state_uid) const
{
    return manager_->history_type(scxml_id_, state_uid);
}

const string& StateMachine::initial_state_of_state(std::string const& state_uid) const
{
    return manager_->initial_state_of_state(scxml_id_, state_uid);
}

const string& StateMachine::onentry_action(std::string const& state_uid) const
{
    return manager_->onentry_action(scxml_id_, state_uid);
}

const string& StateMachine::onexit_action(std::string const& state_uid) const
{
    return manager_->onexit_action(scxml_id_, state_uid);
}

const string& StateMachine::frame_move_action(std::string const& state_uid) const
{
    return manager_->frame_move_action(scxml_id_, state_uid);
}

std::vector< TransitionAttr* > StateMachine::transition_attr(std::string const& state_uid) const
{
    return manager_->transition_attr(scxml_id_, state_uid);
}

size_t StateMachine::num_of_states() const
{
    return this->states_map_.size();
}

const vector< string > & StateMachine::get_all_states() const
{
    return manager_->get_all_states (scxml_id_);
}

}


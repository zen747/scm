#include "Parallel.h"
#include "StateMachine.h"

#include <cassert>

using std::string;
using std::vector;

namespace SCM {

const string done_state_prefix = "done.state.";

/////////
Parallel::Parallel (std::string const& state_id, State* parent, StateMachine *machine)
    :State (state_id, parent, machine)
{
}

Parallel *Parallel::clone (State *parent, StateMachine *m)
{
    Parallel *pa = new Parallel (state_id_, parent, m);

    pa->clone_data (this);

    pa->autorelease ();

    return pa;
}

void Parallel::makeSureEnterStates()
{
    this->enterState(true);
    for (size_t i=0; i < this->substates_.size(); ++i) {
        substates_[i]->makeSureEnterStates();
    }   
}


void Parallel::onEvent (string const &e)
{
    if (this->isLeavingState ()) return;

    if (this->done_) {
        return;
    }

    if (e.substr(0, done_state_prefix.size()) == done_state_prefix) {
        string state_uid = e.substr (11);
        if (!state_uid.empty()) {
            this->finished_substates_.insert (state_uid);
        }
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

    for (size_t i=0; i < this->substates_.size (); ++i) {
        this->substates_[i]->onEvent (e);
    }

    // this state come to an end
    if (this->finished_substates_.size () == this->substates_.size ()) {
        done_ = true;
        this->signal_done ();
        machine_->enqueEvent (done_state_prefix + state_uid_);
    }
}

void Parallel::enterState (bool enter_substate)
{
    if (active_) return;
    
    assert (!substates_.empty ());

    if (parent_ && !machine_->history_type (parent_->state_uid()).empty ()) {
        parent_->history_state_id_ = this->state_uid();
    }

    finished_substates_.clear ();

    done_ = false;
    active_ = true;
    this->reset_time ();

    machine_->current_leaf_state_ = this;
    signal_onentry ();
    if (this->parent_ && !this->parent_->inState (this, false)) {
        return;
    }

    if (enter_substate) {
        for (size_t i=0; i < this->substates_.size (); ++i) {
            this->substates_[i]->enterState (enter_substate);
        }
    }
}

void Parallel::doEnterState (std::vector<State *> &vps)
{
    State *state = vps.back ();
    if (state->depth_ <= this->depth_) return;
    
    vps.pop_back ();
    for (size_t i=0; i < this->substates_.size(); ++i) {
        if (state == substates_[i]) {
            state->enterState (false);
            if (!vps.empty ()) {
                state->doEnterState (vps);
            }
            return;
        }
    }   
}

void Parallel::exitState ()
{
    if (!active_) return;
    
    for (size_t i=0; i < this->substates_.size (); ++i) {
        substates_[i]->exitState ();
    }

    active_ = false;

    signal_onexit ();
}

bool Parallel::inState (std::string const& state_id, bool recursive) const
{
    for (size_t i=0; i < this->substates_.size (); ++i) {
        if (substates_[i]->state_id_ == state_id) {
            return true;
        }
    }

    if (recursive) {
        for (size_t i=0; i < this->substates_.size (); ++i) {
            if (substates_[i]->inState (state_id, recursive)) {
                return true;
            }
        }
    }
    return false;
}

bool Parallel::inState (State const *state, bool recursive) const
{
    for (size_t i=0; i < this->substates_.size (); ++i) {
        if (substates_[i] == state) {
            return true;
        }
    }

    if (recursive) {
        for (size_t i=0; i < this->substates_.size (); ++i) {
            if (substates_[i]->inState (state, recursive)) {
                return true;
            }
        }
    }
    return false;
}

void Parallel::onFrameMove (float t)
{
    if (this->isLeavingState()) {
        if (this->check_leaving_state_finished (t)) return;
    }

    for (size_t i=0; i < this->substates_.size (); ++i) {
        this->substates_[i]->frame_move (t);
        if (!this->active_) return;
    }

    this->pumpNoEvents ();

    if (!this->active_) return;

    for (size_t i=0; i < frame_move_slots_.size(); ++i) {
        frame_move_slots_[i] (t);
    }
}

}


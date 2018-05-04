#include "FrameMover.h"
#include <algorithm>

using namespace std;

namespace SCM {

double  FrameMover::system_move_time_;

FrameMover::FrameMover ()
    : total_elapsed_time_(0)
    , pause_(false)
    , frame_moving_(false)
{
}

FrameMover::~FrameMover ()
{
}

void FrameMover::frame_move (float elapsed_seconds)
{
    if (pause_) return;
    assert (!frame_moving_ && "Error! recursive frame_move() called");
    frame_moving_ = true;
    total_elapsed_time_ += elapsed_seconds;
    signal_on_frame_move_(elapsed_seconds);
    this->onFrameMove (elapsed_seconds);
    frame_moving_ = false;
}

void FrameMover::setPause (bool p)
{
    if (p != pause_) {
        if (p) {
            pause();
        } else {
            resume();
        }
    }
}

void FrameMover::togglePause ()
{
    if (pause_) {
        resume();
    } else {
        pause();
    }
}

void FrameMover::pause ()
{
    if (!pause_) {
        pause_ = true;
        signal_on_pause_();
        onPause();
    }
}

void FrameMover::resume ()
{
    if (pause_) {
        pause_ = false;
        signal_on_resume_();
        onResume();
    }
}

void FrameMover::reset_time ()
{
    total_elapsed_time_ = 0;
}


namespace {
struct TimedActionTypeCompare
{
    bool operator () (TimedActionType *lhs, TimedActionType *rhs) const
    {
        return lhs->time_ < rhs->time_;
    }
};
}

void PunctualFrameMover::addTimedAct (TimedActionType *act)
{
    act->retain ();
    list <TimedActionType *>::iterator it = std::upper_bound (timed_acts_.begin (), timed_acts_.end (), act, TimedActionTypeCompare ());
    timed_acts_.insert (it, act);
}

TimedActionType * PunctualFrameMover::registerTimedAction (float after_t, boost::function<void()> act, bool external_manage)
{
    TimedActionType * p = new TimedActionType (after_t + system_move_time_, act, external_manage);
    addTimedAct (p);
    p->release ();
    return p;
}

void PunctualFrameMover::clearTimedActions ()
{
    if (!timed_acts_.empty ()) {
        list <TimedActionType *>::iterator it = timed_acts_.begin ();
        list <TimedActionType *>::iterator it_end = timed_acts_.end ();
        for (; it != it_end ; ++it) {
            (*it)->release ();
        }
        timed_acts_.clear ();
    }
}

void PunctualFrameMover::pumpTimers (double t)
{
    system_move_time_ += t;
    // handle timed action
    if (!timed_acts_.empty ()) {
        list <TimedActionType *>::iterator it = timed_acts_.begin ();
        for (; it != timed_acts_.end () ;) {
            if ((*it)->time_ <= system_move_time_) {
                if (!(*it)->extern_manage_ || !(*it)->unique_ref ()) {
                    (*it)->signal_ ();
                }
                (*it)->release ();
                it = timed_acts_.erase (it);
            } else {
                break;
            }
        }
    }
}


}


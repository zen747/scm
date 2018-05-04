#ifndef IFrameMover_H
#define IFrameMover_H

#include "RefCountObject.h"
#include <string>
#include <list>
#include <boost/signals2.hpp>


namespace SCM {


class FrameMover: virtual public RefCountObject
{
protected:
    static double                       system_move_time_;

protected:
    double total_elapsed_time_;
    bool pause_;
    bool frame_moving_;

protected:

    virtual void onFrameMove (float elapsed_seconds) = 0;
    virtual void onPause() {}
    virtual void onResume() {}
    
public:
    FrameMover ();
    virtual ~FrameMover();

    virtual void frame_move (float elapsed_seconds);

    virtual void setPause (bool pause);
    virtual void togglePause ();
    virtual void pause ();
    virtual void resume ();
    inline bool paused () const {
        return pause_;
    }

    inline double total_elapsed_time () const {
        return this->total_elapsed_time_;
    }

    /** \brief reset total_elapsed_time to 0。*/
    virtual void reset_time ();
    
    boost::signals2::signal<void(float)> signal_on_frame_move_;
    boost::signals2::signal<void()> signal_on_pause_;
    boost::signals2::signal<void()> signal_on_resume_;
};

/**
PunctualFrameMover 讓使用者預訂未來某時間點呼叫某個動作。這邊以 signal/slot 的方式提供支援。
PunctualFrameMover let user specify actions in the future. Support by utilize signal/slot mechanism.
@see PunctualFrameMover::registerTimedAction ()
*/
struct TimedActionType: public RefCountObject
{
    typedef boost::signals2::signal<void()> signal_t;
    double                             time_;
    signal_t                           signal_;
    bool                               extern_manage_;
    boost::signals2::scoped_connection conn_;

    TimedActionType (double time, boost::function<void()> slot, bool self_manage)
        :time_(time), extern_manage_(self_manage)
    {
        conn_ = signal_.connect (slot);
    }

    ~TimedActionType ()
    {
    }

    bool operator< (TimedActionType const&rhs) const
    {
        return time_ < rhs.time_;
    }
};

class PunctualFrameMover: public FrameMover
{
protected:
    std::list <TimedActionType *>   timed_acts_;

protected:

    void addTimedAct (TimedActionType *act);
    
public:
    /** 在 after_t 秒後，執行動作 act。 如果external_manage為真，reference count > 1才執行動作。
     * execute action act after after_t seconds, if external_manage is true, only perform action if reference count > 1.
     */
    TimedActionType * registerTimedAction (float after_t, boost::function<void()> act, bool external_manage=false);

    /** 清除所有未執行動作 
     * clear all timed actions
     */
    void clearTimedActions ();
    void pumpTimers (double t);
};

}

#endif

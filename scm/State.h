#ifndef State_H
#define State_H

#include <string>
#include <vector>
#include <list>
#include <set>

#include <RefCountObject.h>
#include <FrameMover.h>

namespace SCM {

class StateMachine;

struct TransitionAttr: public RefCountObject
{
    std::string              event_;
    std::string              transition_target_;
    std::vector<std::string> random_target_;
    std::string              cond_; // for later connecting slot
    std::string              ontransit_; // for later connecting slot
    std::string              in_state_; // for inState check if not empty
    bool                     not_; // to support "!In(state)"

    TransitionAttr (std::string const &e, std::string const &t)
        : event_(e), transition_target_(t), not_(false)
    {
    }
};

struct Transition
{
    TransitionAttr       *attr_;
    boost::function<bool()>  cond_functor_;
    boost::signals2::signal<void()> signal_transit;

    Transition ()
        : attr_(0)
    {
    }


    ~Transition ()
    {
        if (attr_) attr_->release ();
        attr_ = 0;
    }

    void setAttr (TransitionAttr *attr)
    {
        attr->retain ();
        if (attr_) attr_->release ();
        attr_ = attr;
    }
};

/** State
以 scxml 為基礎。請參考 https://www.w3.org/TR/scxml/
Based on scxml, please refer to https://www.w3.org/TR/scxml/
*/

class State: public FrameMover
{
protected:
    StateMachine *machine_;
    State        *parent_;
    State        *current_state_;
    char         depth_;
    bool         is_a_final_;
    bool         done_;
    bool         slots_ready_;

    bool         active_;
    bool         is_unique_id_;

    std::string  state_id_;
    std::string  state_uid_;

    std::vector<State*> substates_;
    std::string  history_state_id_;

    std::vector<boost::shared_ptr<Transition> >    no_event_transitions_;
    std::vector<boost::shared_ptr<Transition> >    transitions_;

    std::vector<boost::function<void (float)> > frame_move_slots_;


public:
    // signals
    boost::signals2::signal<void()>      signal_done;
    boost::signals2::signal<void()>      signal_onentry;
    boost::signals2::signal<void()>      signal_onexit;

public:
    State (std::string const& state_id, State* parent, StateMachine *machine);

    virtual ~State ();

    void clear_substates ();

    inline std::string const & state_id () const
    {
        return state_id_;
    }
    
    inline std::string const & state_uid() const
    {
        return state_uid_;
    }

    virtual State *clone (State *parent, StateMachine *m);
    void clone_data (State *rhs);

    inline bool done () const {
        return done_;
    }
    inline bool active () const {
        return active_;
    }
    inline char depth () const {
        return depth_;
    }

    std::string initial_state () const;
    State * findState(std::string const&state_id, const State *exclude=0, bool check_parent=true) const;
    
    float leavingDelay () const;
    bool  isLeavingState () const;
    bool  check_leaving_state_finished (float t);
    // set to < 0 for indefinite delay
    void  setLeavingDelay (float delay);

    virtual void clearHistory ();
    virtual void clearDeepHistory ();

    virtual void changeState (Transition const &transition);
    virtual void doEnterState (std::vector<State *> &vps);
    virtual void doEnterSubState ();

    virtual bool inState (std::string const& state_id, bool recursive=true) const;
    virtual bool inState (State const* state, bool recursive=true) const;    
    virtual void makeSureEnterStates();

    friend class Parallel;
    friend class StateMachine;
    friend class StateMachineManager;

protected:
    virtual void onEvent (std::string const &e);
    virtual void enterState (bool enter_substate=true);
    virtual void exitState ();

    void doLeaveAferDelay ();

    State *findLCA (State const* ots);

    virtual void prepareActionCondSlots ();
    virtual void connectCondSlots ();
    virtual void connectActionSlots ();

    virtual void onFrameMove (float t);

    void pumpNoEvents ();

    virtual void reset_history ();

    bool trig_cond (Transition const &tran) const;

private:
    struct PRIVATE;
    friend struct PRIVATE;
    PRIVATE *private_;

};

}


#endif

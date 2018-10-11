#ifndef StateMachine_H
#define StateMachine_H

#include "State.h"
#include "Parallel.h"
#include "RefCountObject.h"

#include <string>
#include <map>
#include <deque>
#include <set>

namespace scm {

class StateMachineManager;

struct TimedEventType: public RefCountObject
{
    double      time_;
    std::string event_;
    bool        cancelable_;

    TimedEventType (double time, std::string const&str, bool cancelable)
        :time_(time), event_(str), cancelable_(cancelable)
    {}

    bool operator< (TimedEventType const&rhs) const
    {
        return time_ < rhs.time_;
    }
};


/** StateMachine
 * 以 scxml 為基礎。請參考 https://www.w3.org/TR/scxml/
 * Based on scxml, please refer to https://www.w3.org/TR/scxml/
*/
class StateMachine : public State
{
    typedef State super;

protected:
    typedef std::map<std::string, boost::function<void (float)> >  frame_move_map;
    typedef std::map<std::string, boost::function<bool ()> >       cond_slot_map;
    typedef std::map<std::string, boost::function<void ()> >       action_slot_map; // used for onentry, onexit, and ontransit, etc.

    std::string scxml_id_;

    StateMachineManager *manager_;
    
    bool slots_prepared_;
    bool slots_connected_;
    bool scxml_loaded_;
    bool on_event_;

    bool with_history_;
    bool allow_nop_entry_exit_slot_;
    bool do_exit_state_on_destroy_;
    
    bool engine_started_;
    
    frame_move_map             *frame_move_slots_;
    cond_slot_map              *cond_slots_;
    action_slot_map            *action_slots_;

    std::string transition_source_state_;
    std::string transition_target_state_;

    std::map<std::string, State*> states_map_;

    State *current_leaf_state_;

    friend class State;
    friend class Parallel;
    friend class StateMachineManager;

protected:
    void destroy_machine (bool do_exit_state=true);

    void prepare_slots ();
    void connect_slots ();
    void clear_slots ();

    virtual void onPrepareActionCondSlots () {}
    virtual void onConnectCondSlots () {}
    virtual void onConnectActionSlots () {}
    virtual void onLoadScxmlFailed () {}

    void onEvent(std::string const&e);

    virtual void onFrameMove (float t);

    StateMachine (StateMachineManager *manager);
    
	/** \brief after t seconds, enqueEvent event_e. */
	TimedEventType * registerTimedEvent(float after_t, std::string const&event_e, bool cancelable);

public:

    virtual ~StateMachine ();

    std::string const & transition_source_state () const { return transition_source_state_; }
    std::string const & transition_target_state () const { return transition_target_state_; }

    bool engineStarted () const;
    
    StateMachineManager *manager () { return manager_;}

    bool is_unique_id (std::string const&state_id) const;
    
    std::string const & scxml_id () const {
        return scxml_id_;
    }

    State *getEnterState () const {
        return current_leaf_state_;
    }
    
    bool re_enter_state() const {
        return transition_source_state_ == transition_target_state_;
    }

    State *getState (std::string const& state_uid) const;
    void addState (State *state);
    void removeState (State *state);
    virtual bool inState (std::string const&state_uid) const;
    
    /** \brief 將 event e 加到 event queue 中等待處理. Add event_e to event queue.*/
    void enqueEvent(std::string const&e);

    void prepareEngine ();

    void StartEngine ();

    void ReStartEngine ();

    /** \brief 停止 StateMachine， do_exit_state 指定是否呼叫 exitState()。 */
    virtual void ShutDownEngine (bool do_exit_state);

    void set_do_exit_state_on_destroy (bool yes) {
        do_exit_state_on_destroy_ = yes;
    }

    /** \brief 是否scxml已經載入完成。 Whether scxml already loaded. */
    bool engineReady () const;

    /** 是否正在離開某 state, 當有設定 leaving_delay時， 狀態會維持在離開中指定的時間。
     * Whether we are leaving this state? if leaving delay was set, machine will stay at this state for designated time.
     */
    bool isLeavingState () const;

    /** 把所有外部 event 做一次處理。
     * Handle all queued events.
     */
    void pumpQueuedEvents ();

    bool GetCondSlot (std::string const&name, boost::function<bool ()> &s);
    bool GetActionSlot (std::string const&name, boost::function<void()> &s);
    bool GetFrameMoveSlot (std::string const&name, boost::function<void (float)> &s);

    /** 建立 name 與 slot s 的對應，以供scxml 中的 cond 條件使用。
     * Mapping name and condition slot. Used in Transition conditions.
     */
    void setCondSlot (std::string const&name, boost::function<bool ()> const &s);
    /** 建立 name 與 slot s 的對應，以供scxml 中各state的 onentry, onexit 使用。
     * Mapping name and action slot. Used in state's onentry, onexit, etc. 
     */
    void setActionSlot (std::string const&name, boost::function<void ()> const &s);
    /** 建立 name 與 slot s 的對應，以供scxml 中各state的 frame_move 使用。
     * Mapping namd and frame_move slot. Used in state's frame_move.
     */
    void setFrameMoveSlot (std::string const&name, boost::function<void (float)> const &s);

    StateMachine* clone ();

    inline bool allow_nop_entry_exit () const {
        return allow_nop_entry_exit_slot_;
    }
    void set_allow_nop_entry_exit (bool yes) {
        allow_nop_entry_exit_slot_ = yes;
    }

	void registerTimedEvent(float after_t, std::string const&event_e) { registerTimedEvent(after_t, event_e, false); }
	TimedEventType * registerTimedEvent_cancelable(float after_t, std::string const&event_e) { registerTimedEvent(after_t, event_e, true); }
	void clearTimedEvents ();
    void pumpTimedEvents ();

    std::string const& state_id_of_history (std::string const&history_id) const;
    std::string const& history_type (std::string const& state_uid) const;
    std::string const& initial_state_of_state (std::string const& state_uid) const;
    std::string const& onentry_action (std::string const& state_uid) const;
    std::string const& onexit_action (std::string const& state_uid) const;
    std::string const& frame_move_action (std::string const& state_uid) const;
    std::vector<TransitionAttr *> transition_attr (std::string const& state_uid) const;
    size_t             num_of_states () const;
    const std::vector<std::string> & get_all_states () const;
    
public:
    // signals
    boost::signals2::signal<void ()>          signal_prepare_slots_;
    boost::signals2::signal<void ()>          signal_connect_cond_slots_;
    boost::signals2::signal<void ()>          signal_connect_action_slots_;
    

private:
    struct PRIVATE;
    friend struct PRIVATE;
    PRIVATE *private_;
};

#define REGISTER_STATE_SLOT(mach, state, onentry, onexit, obj) \
    { \
    boost::function<void ()> slot = boost::bind (onentry, obj); \
    mach->setActionSlot ("onentry_" state, slot); \
    slot = boost::bind (onexit, obj); \
    mach->setActionSlot ("onexit_" state, slot); \
    }
    
#define DECLARE_STATE_ENTRIES(state) \
    void onentry_##state (); \
    void onexit_##state ();
    
#define REGISTER_ACTION_SLOT(mach, action, method, obj) \
	mach->setActionSlot (action, boost::bind(method, obj));

#define REGISTER_FRAME_MOVE_SLOT(mach, state, method, obj) \
	mach->setFrameMoveSlot (state, boost::bind(method, obj, _1));

#define REGISTER_COND_SLOT(mach, cond, method, obj) \
	mach->setCondSlot (cond, boost::bind(method, obj));


}

#endif

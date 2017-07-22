#ifndef StateMachineManager_H
#define StateMachineManager_H

#include <StateMachine.h>
#include <uncopyable.h>

namespace SCM {

class StateMachineManager: public Uncopyable
{
public:
    static StateMachineManager *instance ();
    void release_instance ();
    
    StateMachineManager ();
    ~StateMachineManager();
    
    StateMachine *getMach (std::string const&scxml_id);
    
    void set_scxml (std::string const&scxml_id, std::string const&scxml_str);
    void prepare_machs ();
    
    bool loadMachFromFile (StateMachine *mach, std::string const&scxml_file);
    bool loadMachFromString (StateMachine *mach, std::string const&scxml_str);

    void addToActiveMach (StateMachine *mach);
    void pumpMachEvents ();

    std::string const& history_id_resided_state (std::string const&scxml_id, std::string const&history_id) const;
    std::string const& history_type (std::string const&scxml_id, std::string const& state_uid) const;
    std::string const& initial_state_of_state (std::string const&scxml_id, std::string const& state_uid) const;
    std::string const& onentry_action (std::string const&scxml_id, std::string const& state_uid) const;
    std::string const& onexit_action (std::string const&scxml_id, std::string const& state_uid) const;
    std::string const& frame_move_action (std::string const&scxml_id, std::string const& state_uid) const;
    std::vector<TransitionAttr *> transition_attr (std::string const&scxml_id, std::string const& state_uid) const;
    size_t             num_of_states (const std::string& scxml_id) const;
    bool is_unique_id (const std::string& scxml_id, std::string const&state_uid) const;
    const std::vector<std::string> & get_all_states (const std::string& scxml_id) const;
    
private:
    struct PRIVATE;
    friend struct PRIVATE;
    PRIVATE *private_;
};

}

#endif

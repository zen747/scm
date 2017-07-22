#include <StateMachineManager.h>
#include <expat.h>
#include <fstream>

namespace SCM {

static StateMachineManager *static_instance_;

using namespace std;

template <typename T>
class array_guard {
    T *ptr_;
public:
    array_guard (T *p)
        :ptr_(p)
    {}

    ~array_guard ()
    {
        delete [] ptr_;
    }

    T *get () const
    {
        return ptr_;
    }
};

size_t splitStringToVector (std::string const &valstr, std::vector<std::string> &val, size_t max_s=0xffffffff, std::string const&separators=", \t")
{
    if (valstr.empty ()) {
        return false;
    }

    size_t len = valstr.length ();
    std::string _str = valstr;
    // append ',' for later parsing logic
    if (separators.find (_str[len-1]) == string::npos)
    {
        _str.push_back(separators[0]);
        ++len;
    }

    size_t begin=0, end;
    size_t count=0;
    bool   single_quote_active = false;
    bool   double_quote_active = false;
    std::string tmp;
    for (size_t i=0; i < len; ++i)
    {
        bool reach = false;
        if (single_quote_active) {
            if (_str[i] == '\'') reach = true;
        } else if (double_quote_active) {
            if (_str[i] == '\"') reach = true;
        } else if (separators.find (_str[i]) != string::npos) {
            reach = true;
        }

        if (reach) {
            end = i;
            if (count >= max_s)
            {
                return false;
            }

            if (end-begin) {
                val.push_back (_str.substr (begin, end-begin));
                count++;
            }
            if (single_quote_active || double_quote_active) {
                ++i;
                ++end;
                single_quote_active = double_quote_active = false;
            }

            for (size_t j=end+1; j < len; ++j, ++i)
            {
                if (separators.find (_str[j]) == string::npos)
                {
                    if (_str[j]=='\'') {
                        single_quote_active = true;
                        ++j;
                        ++i;
                    } else if (_str[j]=='\"') {
                        double_quote_active = true;
                        ++j;
                        ++i;
                    }
                    begin = j;
                    break;
                }
            }
        }
    }

    return count;
}

struct StateMachineManager::PRIVATE
{
    StateMachineManager     * manager_;
    std::list<StateMachine *> active_machs_;
    // ids are unique
    std::map <std::string, StateMachine *>              mach_map_;
    std::map <std::string, std::map<std::string, std::string> > onentry_action_map_;
    std::map <std::string, std::map<std::string, std::string> > onexit_action_map_;
    std::map <std::string, std::map<std::string, std::string> > frame_move_action_map_;
    std::map <std::string, std::map<std::string, std::string> > initial_state_map_;
    std::map <std::string, std::map<std::string, std::string> > history_type_map_;
    std::map <std::string, std::map<std::string, std::string> > history_id_reside_state_;
    std::map <std::string, std::set<std::string> >      non_unique_ids_;
    std::map <std::string, std::vector<std::string> >   state_uids_;
    std::map <std::string, std::map<std::string, std::vector<TransitionAttr *> > > transition_attr_map_;

    std::map<std::string, std::string> scxml_map_;
    
    PRIVATE(StateMachineManager *manager)
    : manager_(manager)
    {
    }
    
    ~PRIVATE()
    {
        clearMachMap();
    }
    
    StateMachine *getMach (std::string const&scxml_id);
    void          clearMachMap ();

    static void XMLCALL startElement(void *userData, const char *name, const char **atts);
    static void XMLCALL endElement(void *userData, const char *name);

};

//----------------------------------------------
namespace {
    struct ParseStruct {
        State        *current_state;
        StateMachine *machine;
        bool          bEnterInitial;
        string        newInitialState;
        string        scxml_id_;
        string        state_id_;

        ParseStruct ()
            :current_state(0), machine(0), bEnterInitial(false)
        {}
    };
}

void StateMachineManager::PRIVATE::startElement(void* userData, const char* name, const char** atts)
{
    ParseStruct *parse = (ParseStruct *)userData;
    StateMachineManager *manager = parse->machine->manager();

    if (string(name) == "scxml") {
        string initialstate;
        vector<string> non_unique_ids;
        while (*atts) {
            const char *attr = *atts++;
            if (string(attr) == "initial") {
                attr = *atts++;
                initialstate = string (attr);
            } else if (string(attr) == "non-unique") {
                attr = *atts++;
                splitStringToVector(attr, non_unique_ids);
            } else {
                atts++;
            }
        }
        manager->private_->state_uids_[parse->scxml_id_].reserve(16);
        manager->private_->state_uids_[parse->scxml_id_].push_back(parse->scxml_id_);
        manager->private_->initial_state_map_[parse->scxml_id_][parse->current_state->state_uid()] = initialstate;
        manager->private_->non_unique_ids_[parse->scxml_id_].insert(non_unique_ids.begin(), non_unique_ids.end());
    } else if (string(name) == "state") {
        string stateid;
        string initialstate;
        string onentry;
        string onexit;
        string framemove;
        string history_type;
        float leaving_delay = 0;
        while (*atts) {
            const char *attr = *atts++;
            if (string(attr) == "id") {
                attr = *atts++;
                stateid= string (attr);
            } else if (string(attr) == "initial") {
                attr = *atts++;
                initialstate = string (attr);
            } else if (string(attr) == "history") {
                attr = *atts++;
                history_type = string (attr);
            } else if (string(attr) == "onentry") {
                attr = *atts++;
                onentry = string (attr);
            } else if (string(attr) == "onexit") {
                attr = *atts++;
                onexit = string (attr);
            } else if (string(attr) == "frame_move") {
                attr = *atts++;
                framemove = string (attr);
            } else if (string(attr) == "leaving_delay") {
                attr = *atts++;
                leaving_delay = strtod (attr, NULL);
            } else {
                atts++;
            }
        }
        
        if (!stateid.empty() && stateid[0] == '_') {
            assert (0 && "state id can't start with '_', which is reserved for internal use.");
        }

        State *state = new State(stateid, parse->current_state, parse->machine);
        if (parse->current_state) parse->current_state->substates_.push_back (state);
        state->setLeavingDelay (leaving_delay);
        
        string state_uid = state->state_uid();
        manager->private_->state_uids_[parse->scxml_id_].push_back(state_uid);
        
        if (onentry.empty ()) onentry = "onentry_" + state_uid;
        
        if (onexit.empty ()) onexit = "onexit_" + state_uid;
        
        if (framemove.empty ()) framemove = state_uid;
        
        if (manager->private_->history_type_map_[parse->scxml_id_][parse->current_state->state_uid()] == "deep") {
            manager->private_->history_type_map_[parse->scxml_id_][state_uid] = "deep";
        } else {
            manager->private_->history_type_map_[parse->scxml_id_][state_uid] = history_type;
        }

        if (!manager->private_->history_type_map_[parse->scxml_id_][state_uid].empty ()) {
            parse->machine->with_history_ = true;
        }

        manager->private_->initial_state_map_[parse->scxml_id_][state_uid] = initialstate;
        manager->private_->onentry_action_map_[parse->scxml_id_][state_uid] = onentry;
        manager->private_->onexit_action_map_[parse->scxml_id_][state_uid] = onexit;
        manager->private_->frame_move_action_map_[parse->scxml_id_][state_uid] = framemove;
        parse->current_state = state;
    } else if (string(name) == "onentry") {
        while (*atts) {
            const char *attr = *atts++;
            if (string(attr) == "action") {
                attr = *atts++;
                manager->private_->onentry_action_map_[parse->scxml_id_][parse->current_state->state_uid()] = attr;
            } else {
                atts++;
            }
        }
    } else if (string(name) == "onexit") {
        while (*atts) {
            const char *attr = *atts++;
            if (string(attr) == "action") {
                attr = *atts++;
                manager->private_->onexit_action_map_[parse->scxml_id_][parse->current_state->state_uid()] = attr;
            } else {
                atts++;
            }
        }
    } else if (string(name) == "frame_move") {
        while (*atts) {
            const char *attr = *atts++;
            if (string(attr) == "action") {
                attr = *atts++;
                manager->private_->frame_move_action_map_[parse->scxml_id_][parse->current_state->state_uid()] = attr;
            } else {
                atts++;
            }
        }
    } else if (string(name) == "history") {
        while (*atts) {
            const char *attr = *atts++;
            if (string(attr) == "type") {
                attr = *atts++;
                manager->private_->history_type_map_[parse->scxml_id_][parse->current_state->state_uid()] = attr;
            } else if (string(attr) == "id") {
                attr = *atts++;
                manager->private_->history_id_reside_state_[parse->scxml_id_][attr] = parse->current_state->state_uid();
            } else {
                atts++;
            }
        }
    } else if (string(name) == "parallel") {
        string stateid;
        string history_type;
        string onentry;
        string onexit;
        string framemove;
        float leaving_delay = 0;
        while (*atts) {
            const char *attr = *atts++;
            if (string(attr) == "id") {
                attr = *atts++;
                stateid = string (attr);
            } else if (string(attr) == "history") {
                attr = *atts++;
                history_type = string (attr);
            } else if (string(attr) == "onentry") {
                attr = *atts++;
                onentry = string (attr);
            } else if (string(attr) == "onexit") {
                attr = *atts++;
                onexit = string (attr);
            } else if (string(attr) == "frame_move") {
                attr = *atts++;
                framemove = string (attr);
            } else if (string(attr) == "leaving_delay") {
                attr = *atts++;
                leaving_delay = strtod (attr, NULL);
            } else {
                atts++;
            }
        }

        Parallel *state = new Parallel(stateid, parse->current_state, parse->machine);
        if (parse->current_state) parse->current_state->substates_.push_back (state);
        state->setLeavingDelay (leaving_delay);

        string state_uid = state->state_uid();
        manager->private_->state_uids_[parse->scxml_id_].push_back(state_uid);
        
        if (onentry.empty ()) onentry = "onentry_" + state_uid;
        
        if (onexit.empty ()) onexit = "onexit_" + state_uid;
        
        if (framemove.empty ()) framemove = state_uid;
                
        if (manager->private_->history_type_map_[parse->scxml_id_][parse->current_state->state_uid()] == "deep") {
            manager->private_->history_type_map_[parse->scxml_id_][state_uid] = "deep";
        } else {
            manager->private_->history_type_map_[parse->scxml_id_][state_uid] = history_type;
        }

        if (!manager->private_->history_type_map_[parse->scxml_id_][state_uid].empty ()) {
            parse->machine->with_history_ = true;
        }

        manager->private_->onentry_action_map_[parse->scxml_id_][state_uid] = onentry;
        manager->private_->onexit_action_map_[parse->scxml_id_][state_uid] = onexit;
        manager->private_->frame_move_action_map_[parse->scxml_id_][state_uid] = framemove;
        parse->current_state = state;
    } else if (string(name) == "final") {
        string stateid;
        string onentry;
        while (*atts) {
            const char *attr = *atts++;
            if (string(attr) == "id") {
                attr = *atts++;
                stateid = string (attr);
            } else if (string(attr) == "onentry") {
                attr = *atts++;
                onentry = string (attr);
            } else {
                atts++;
            }
        }
        State *state = new State(stateid, parse->current_state, parse->machine);
        if (onentry.empty ()) onentry = "onentry_" + state->state_uid();
        if (parse->current_state) parse->current_state->substates_.push_back (state);
        manager->private_->state_uids_[parse->scxml_id_].push_back(state->state_uid());
        manager->private_->onentry_action_map_[parse->scxml_id_][state->state_uid()] = onentry;
        state->is_a_final_ = true;
        parse->current_state = state;
    } else if (string(name) == "initial") {
        parse->bEnterInitial = true;
    } else if (string(name) == "transition") {
        if (parse->bEnterInitial) {
            while (*atts) {
                const char *attr = *atts++;
                if (string(attr) == "target") {
                    attr = *atts++;
                    parse->newInitialState = string (attr);
                } else {
                    atts++;
                }
            }
        } else {
            TransitionAttr * tran = new TransitionAttr ("","");

            while (*atts) {
                const char *attr = *atts++;
                if (string(attr) == "event") {
                    attr = *atts++;
                    tran->event_ = string (attr);
                } else if (string(attr) == "cond") {
                    attr = *atts++;
                    tran->cond_ = string (attr);
                } else if (string(attr) == "ontransit") {
                    attr = *atts++;
                    tran->ontransit_ = string (attr);
                } else if (string(attr) == "target") {
                    attr = *atts++;
                    tran->transition_target_ = string (attr);
                } else if (string(attr) == "random_target") {
                    attr = *atts++;
                    vector<string> values;
                    splitStringToVector (attr, values);
                    tran->random_target_ = values;
                } else {
                    atts++;
                }
            }

            if (!tran->cond_.empty () && tran->cond_[0] == '!') {
                tran->cond_ = tran->cond_.substr (1);
                tran->not_ = true;
            }

            manager->private_->transition_attr_map_[parse->scxml_id_][parse->current_state->state_uid()].push_back (tran);
        }
    }
}

void StateMachineManager::PRIVATE::endElement(void* userData, const char* name)
{
    ParseStruct *parse = (ParseStruct *)userData;
    StateMachineManager *manager = parse->machine->manager();

    if (string(name) == "state" || string(name) == "parallel" || string(name) == "final") {
        parse->current_state = parse->current_state->parent_;
    } else if (string(name) == "initial") {
        parse->bEnterInitial = false;
        manager->private_->initial_state_map_[parse->scxml_id_][parse->current_state->state_uid()] = parse->newInitialState;
    } else if (string(name) == "scxml") { // parse complete
        map<string, vector<TransitionAttr *> > &transition_map = manager->private_->transition_attr_map_[parse->scxml_id_];
        map<string, vector<TransitionAttr *> > ::iterator it = transition_map.begin ();
        for (; it != transition_map.end (); ++it) {
            string const &state_uid = it->first;
            State *st = parse->machine->getState(state_uid);

            for (size_t i=0; i < it->second.size (); ++i) {
                string &target_str = it->second[i]->transition_target_;
                if (target_str.find(',') == string::npos && parse->machine->is_unique_id(target_str)) continue;
                // support multiple targets
                vector<string> targets;
                vector<State*> tstates;
                splitStringToVector(target_str, targets);
                for (size_t i=0; i < targets.size(); ++i) {
                    if (!parse->machine->is_unique_id(targets[i])) {
                        State *s = st->findState(targets[i]);
                        assert (s && "can't find transition target, not state id?");
                        targets[i] = s->state_uid();
                    }
                    tstates.push_back(parse->machine->getState(targets[i]));
                }
                target_str = targets[0];
                for (size_t i=1; i < targets.size(); ++i) {
                    target_str += ("," + targets[i]);
                }
                
                // check multiple target have the same ancestor of parallel
                for (size_t i=1; i < tstates.size(); ++i) {
                    State *lca = tstates[0]->findLCA (tstates[i]);
                    assert (typeid(*lca) == typeid(Parallel) && "multiple targets but can't find common ancestor.");
                }
                
            }
        }

    }
}

StateMachineManager::StateMachineManager()
{
    private_ = new PRIVATE(this);
}

StateMachineManager::~StateMachineManager()
{
    delete private_;
}


StateMachineManager* StateMachineManager::instance()
{
    if (!static_instance_) {
        static_instance_ = new StateMachineManager;
    }
    return static_instance_;
}

void StateMachineManager::release_instance()
{
    delete static_instance_;
    static_instance_ = 0;
}


StateMachine *StateMachineManager::getMach (std::string const&scxml_id)
{
    return private_->getMach(scxml_id);
}

void StateMachineManager::addToActiveMach(StateMachine* mach)
{
    assert (mach);
    if (!mach) return;
    mach->retain();
    private_->active_machs_.push_back(mach);
}

void StateMachineManager::pumpMachEvents()
{
    while (!private_->active_machs_.empty ()) {
        std::list<StateMachine *> machs;
        machs.swap (private_->active_machs_);
        std::list<StateMachine *>::iterator it = machs.begin ();
        std::list<StateMachine *>::iterator it_end = machs.end ();
        for (; it != it_end; ++it) {
            (*it)->pumpQueuedEvents ();
            (*it)->release();
        }
    }

}

void StateMachineManager::set_scxml(const string& scxml_id, const string& scxml_str)
{
    private_->scxml_map_[scxml_id] = scxml_str;
}

void StateMachineManager::prepare_machs()
{
    map<string, string>::iterator it = private_->scxml_map_.begin();
    for (; it != private_->scxml_map_.end(); ++it) {
        string const&scxml_id = it->first;
        map<string, StateMachine *>::iterator itm = private_->mach_map_.find (scxml_id);
        StateMachine *mach = 0;
        if (itm == private_->mach_map_.end ()) {
            mach = new StateMachine (this);
            mach->scxml_id_ = scxml_id;
            private_->mach_map_[scxml_id] = mach;
        } else {
            mach = itm->second;
        }
        if (!mach->scxml_loaded_) {
            mach->scxml_loaded_ = this->loadMachFromString(mach, it->second);
            // "prepare mach '%s' %s", mach->scxml_id_.c_str(), mach->scxml_loaded_ ? "done" : "fail");
        } else {
            // "mach '%s' already prepared", mach->scxml_id_.c_str());
        }
    }
}


bool StateMachineManager::loadMachFromFile(StateMachine* mach, const string& scxml_file)
{
    FILE *zfile = fopen(scxml_file.c_str (), "rb" );

    if( zfile == NULL )
    {
        // "Error: can't open file " + scxml_file);
        return false;
    }

    fseek(zfile, 0, SEEK_END);
    size_t file_length = ftell (zfile);
    rewind (zfile);

    ParseStruct parse;
    parse.scxml_id_ = mach->scxml_id ();
    parse.machine = mach;
    parse.current_state = mach;
    mach->manager()->private_->transition_attr_map_[parse.scxml_id_].clear();

    const int BufferSize = 8192;
    char *buf = new char[BufferSize];
    array_guard<char> guard(buf);

    XML_Parser parser = XML_ParserCreate(NULL);
    XML_SetUserData(parser, &parse);

    XML_SetElementHandler(parser, PRIVATE::startElement, PRIVATE::endElement);
    bool parseFail = false;

    off_t total_readlen = 0;
    while (total_readlen < file_length) {
        int readlen = fread (buf, 1, BufferSize, zfile);
        total_readlen += readlen;
        if (XML_Parse(parser, buf, readlen, (total_readlen >= file_length)) == XML_STATUS_ERROR) {
            parseFail = true;
            break;
        }
    }

    XML_ParserFree(parser);

    fclose( zfile );
    
    return !parseFail;

}

bool StateMachineManager::loadMachFromString(StateMachine* mach, const string& scxml_str)
{
    ParseStruct parse;
    parse.scxml_id_ = mach->scxml_id ();
    parse.machine = mach;
    parse.current_state = mach;
    mach->manager()->private_->transition_attr_map_[parse.scxml_id_].clear();

    XML_Parser parser = XML_ParserCreate(NULL);
    XML_SetUserData(parser, &parse);

    XML_SetElementHandler(parser, PRIVATE::startElement, PRIVATE::endElement);
    bool parseFail = false;
    if (XML_Parse(parser, scxml_str.c_str (), scxml_str.length (), true) == XML_STATUS_ERROR) {
        parseFail = true;
    }

    XML_ParserFree(parser);

    return !parseFail;

}


StateMachine* StateMachineManager::PRIVATE::getMach(const std::string& scxml_id)
{
    map<string, StateMachine *>::iterator it = mach_map_.find (scxml_id);
    if (it != mach_map_.end ()) {
        return it->second->clone ();
    } else {
        StateMachine *mach = new StateMachine (manager_);
        mach_map_[scxml_id] = mach;
        mach->scxml_id_ = scxml_id;
        map<string, string>::iterator it = scxml_map_.find(scxml_id);
        if (it != scxml_map_.end()) {
            mach->scxml_loaded_ = manager_->loadMachFromString(mach, it->second);
        }
        return mach->clone ();
    }
}

void StateMachineManager::PRIVATE::clearMachMap ()
{
    for (std::map <std::string, StateMachine *>::iterator it=mach_map_.begin (); it != mach_map_.end (); ++it) {
        it->second->release ();
    }
    mach_map_.clear ();
    onentry_action_map_.clear ();
    onexit_action_map_.clear ();
    frame_move_action_map_.clear ();
    initial_state_map_.clear ();
    history_type_map_.clear ();
    history_id_reside_state_.clear();

    map <string, map<string, vector<TransitionAttr *> > >::iterator it = transition_attr_map_.begin ();
    for (; it != transition_attr_map_.end (); ++it) {
        map<string, vector<TransitionAttr *> >::iterator it2 = it->second.begin ();
        for (; it2 != it->second.end (); ++it2) {
            for (size_t i=0; i < it2->second.size (); ++i) {
                it2->second[i]->release ();
            }
        }
    }
    transition_attr_map_.clear ();
}

std::string const& StateMachineManager::history_id_resided_state(const std::string& scxml_id, const std::string& history_id) const
{
    return private_->history_id_reside_state_[scxml_id][history_id];
}


std::string const& StateMachineManager::history_type(const string& scxml_id, std::string const& state_uid) const
{
    return private_->history_type_map_[scxml_id][state_uid];
}

const string& StateMachineManager::initial_state_of_state(const string& scxml_id, std::string const& state_uid) const
{
    return private_->initial_state_map_[scxml_id][state_uid];
}

const string& StateMachineManager::onentry_action(const string& scxml_id, std::string const& state_uid) const
{
    return private_->onentry_action_map_[scxml_id][state_uid];
}

const string& StateMachineManager::onexit_action(const string& scxml_id, std::string const& state_uid) const
{
    return private_->onexit_action_map_[scxml_id][state_uid];
}

const string& StateMachineManager::frame_move_action(const string& scxml_id, std::string const& state_uid) const
{
    return private_->frame_move_action_map_[scxml_id][state_uid];
}

std::vector< TransitionAttr* > StateMachineManager::transition_attr(const string& scxml_id, std::string const& state_uid) const
{
    return private_->transition_attr_map_[scxml_id][state_uid];
}

size_t StateMachineManager::num_of_states(const string& scxml_id) const
{
    return private_->state_uids_[scxml_id].size();
}

bool StateMachineManager::is_unique_id(const string& scxml_id, const string& state_uid) const
{
    return private_->non_unique_ids_[scxml_id].count(state_uid) == 0;
}

const vector< string > & StateMachineManager::get_all_states(const string& scxml_id) const
{
    return private_->state_uids_[scxml_id];
}


}
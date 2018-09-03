#include "StateMachineManager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/json_parser.hpp>

using boost::property_tree::ptree;
using boost::property_tree::read_xml;
using boost::property_tree::read_json;
using namespace std;

namespace SCM {

class FileCloser: Uncopyable
{
    FILE *f_;
public:
    FileCloser(FILE *f)
    :f_(f)
    {}
    
    ~FileCloser()
    {
        fclose(f_);
    }
};

static StateMachineManager *static_instance_;

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

size_t splitStringToVector (string const &valstr, vector<string> &val, size_t max_s=0xffffffff, string const&separators=", \t")
{
    if (valstr.empty ()) {
        return false;
    }

    size_t len = valstr.length ();
    string _str = valstr;
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
    string tmp;
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
//----------------------------------------------
namespace {
    struct ParseStruct {
        State        *current_state_;
        StateMachine *machine;
        bool          bEnterInitial;
        string        newInitialState;
        string        scxml_id_;
        string        state_id_;

        ParseStruct ()
            :current_state_(0), machine(0), bEnterInitial(false)
        {}
    };
}
//----------------------------------------------

struct StateMachineManager::PRIVATE
{
    StateMachineManager     * manager_;
    // ids are unique
    map <string, StateMachine *>              mach_map_;
    map <string, map<string, string> > onentry_action_map_;
    map <string, map<string, string> > onexit_action_map_;
    map <string, map<string, string> > frame_move_action_map_;
    map <string, map<string, string> > initial_state_map_;
    map <string, map<string, string> > history_type_map_;
    map <string, map<string, string> > history_id_reside_state_;
    map <string, set<string> >      non_unique_ids_;
    map <string, vector<string> >   state_uids_;
    map <string, map<string, vector<TransitionAttr *> > > transition_attr_map_;

    map<string, string> scxml_map_;
    
    PRIVATE(StateMachineManager *manager)
    : manager_(manager)
    {
    }
    
    ~PRIVATE()
    {
        clearMachMap();
    }
    
    StateMachine *getMach (string const&scxml_id);
    void          clearMachMap ();

    static void get_item_attrs_in_ptree(ptree& pt, map<string, string> &attrs_map);
    static void parse_element(ParseStruct &data, ptree &pt, int level);
    static bool parse_scm_tree (ParseStruct &data, string const&scm_str);
    
    static void finish_scxml (ParseStruct &data, map<string, string> &attributes);
    static void handle_state_item (ParseStruct &data, map<string, string> &attributes);
    static void handle_parallel_item (ParseStruct &data, map<string, string> &attributes);
    static void handle_final_item (ParseStruct &data, map<string, string> &attributes);
    static void handle_transition_item (ParseStruct &data, map<string, string> &attributes);
    static void handle_history_item (ParseStruct &data, map<string, string> &attributes);
};

void StateMachineManager::PRIVATE::get_item_attrs_in_ptree(ptree& pt, map<string, string> &attrs_map)
{
    ptree::iterator end = pt.end();
    for (ptree::iterator pt_it = pt.begin(); pt_it != end; ++pt_it) {
        if (pt_it->first == "<xmlattr>") {
            return get_item_attrs_in_ptree(pt_it->second, attrs_map);
        } else {
            if (pt_it->second.empty()) {
                attrs_map[pt_it->first] = pt_it->second.data();
            }
        }
    }
}
namespace {
    void validate_state_id (string const&stateid)
    {
        if (!stateid.empty() && stateid[0] == '_') {
            assert (0 && "state id can't start with '_', which is reserved for internal use.");
            throw std::runtime_error("state id can't start with '_', which is reserved for internal use.");
        }
    }
}

void StateMachineManager::PRIVATE::parse_element(ParseStruct &data, ptree &pt, int level)
{    
    StateMachineManager *manager = data.machine->manager();
    ptree::iterator end = pt.end();
    for (ptree::iterator pt_it = pt.begin(); pt_it != end; ++pt_it) {
        if (pt_it->first == "<xmlattr>") {
            parse_element(data, pt_it->second, level);
        } else {
            //for (int i=0; i < level; ++i) {
            //    cout << "\t";
            //}
            
            if (pt_it->second.empty()) {
                if (pt_it->first == "non-unique") {
                    vector<string> non_unique_ids;
                    splitStringToVector(pt_it->second.data(), non_unique_ids);
                    data.machine->manager()->private_->non_unique_ids_[data.scxml_id_].insert(non_unique_ids.begin(), non_unique_ids.end());
                } else {
                    //
                }
            //    cout << pt_it->first << " = " << pt_it->second.data() << endl;
            } else {
                string tag = pt_it->first.data();
             //   cout << tag << ": \n";
                map<string,string> attrs_map;
                get_item_attrs_in_ptree (pt_it->second, attrs_map);
                
                if (tag == "scxml") {
                    manager->private_->state_uids_[data.scxml_id_].reserve(16);
                    manager->private_->state_uids_[data.scxml_id_].push_back(data.scxml_id_);
                } else if (tag == "state") {
                    string stateid = attrs_map["id"];
                //    cout << "id -> " << stateid << endl;
                    validate_state_id (stateid);
                    State *state = new State(stateid, data.current_state_, data.machine);
                    if (data.current_state_) data.current_state_->substates_.push_back (state);
                    data.current_state_ = state;
                    manager->private_->state_uids_[data.scxml_id_].push_back(state->state_uid());
                    handle_state_item(data, attrs_map);
                } else if (tag == "parallel") {
                    string stateid = attrs_map["id"];
               //     cout << "id -> " << stateid << endl;
                    validate_state_id (stateid);
                    Parallel *state = new Parallel(stateid, data.current_state_, data.machine);
                    if (data.current_state_) data.current_state_->substates_.push_back (state);
                    data.current_state_ = state;
                    manager->private_->state_uids_[data.scxml_id_].push_back(state->state_uid());
                    handle_parallel_item(data, attrs_map);
                } else if (tag == "final") {
                    string stateid = attrs_map["id"];
               //     cout << "id -> " << stateid << endl;
                    validate_state_id (stateid);
                    State *state = new State(stateid, data.current_state_, data.machine);
                    if (data.current_state_) data.current_state_->substates_.push_back (state);
                    data.current_state_ = state;
                    manager->private_->state_uids_[data.scxml_id_].push_back(state->state_uid());
                    handle_final_item(data, attrs_map);
                } else if (tag == "history") {
                    handle_history_item(data, attrs_map);
                } else if (tag == "transition") {
                    handle_transition_item(data, attrs_map);
                }
                
                parse_element(data, pt_it->second, level+1);
                
                if (tag == "scxml") {
                    finish_scxml (data, attrs_map);
                    data.current_state_ = data.current_state_->parent_;
                } else if (tag == "state") {
                    data.current_state_ = data.current_state_->parent_;
                } else if (tag == "parallel") {
                    data.current_state_ = data.current_state_->parent_;
                } else if (tag == "final") {
                    data.current_state_ = data.current_state_->parent_;
                }
            }
        }
    }
}

void StateMachineManager::PRIVATE::finish_scxml(ParseStruct& data, map<string, string> &attributes)
{
    StateMachineManager *manager = data.machine->manager();
    
    map<string,string>::iterator attr_it_end = attributes.end();
    map<string,string>::iterator attr_it = attributes.begin();
    for (; attr_it != attr_it_end; ++attr_it) {
        if (attr_it->first == "initial") {
            manager->private_->initial_state_map_[data.scxml_id_][data.current_state_->state_uid()] = attr_it->second;
        }
    }
    
    map<string, vector<TransitionAttr *> > &transition_map = manager->private_->transition_attr_map_[data.scxml_id_];
    map<string, vector<TransitionAttr *> > ::iterator tran_attr_it = transition_map.begin ();
    for (; tran_attr_it != transition_map.end (); ++tran_attr_it) {
        string const &state_uid = tran_attr_it->first;
        State *st = data.machine->getState(state_uid);

        for (size_t i=0; i < tran_attr_it->second.size (); ++i) {
            string &target_str = tran_attr_it->second[i]->transition_target_;
            if (target_str.find(',') == string::npos && data.machine->is_unique_id(target_str)) continue;
            // support multiple targets
            vector<string> targets;
            vector<State*> tstates;
            splitStringToVector(target_str, targets);
            for (size_t i=0; i < targets.size(); ++i) {
                if (!data.machine->is_unique_id(targets[i])) {
                    State *s = st->findState(targets[i]);
                    assert (s && "can't find transition target, not state id?");
                    targets[i] = s->state_uid();
                }
                tstates.push_back(data.machine->getState(targets[i]));
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

void StateMachineManager::PRIVATE::handle_state_item(ParseStruct& data, map<string, string> &attributes)
{
    StateMachineManager *manager = data.machine->manager();
    
    string stateid;
    string onentry;
    string onexit;
    string framemove;
    string history_type;
    float leaving_delay = 0;
    map<string,string>::iterator attr_it_end = attributes.end();
    map<string,string>::iterator attr_it = attributes.begin();
    for (; attr_it != attr_it_end; ++attr_it) {
        if (attr_it->first == "initial") {
            manager->private_->initial_state_map_[data.scxml_id_][data.current_state_->state_uid()] = attr_it->second;
        } else if (attr_it->first == "history") {
            history_type = attr_it->second;
        } else if (attr_it->first == "onentry") {
            onentry = attr_it->second;
        } else if (attr_it->first == "onexit") {
            onexit = attr_it->second;
        } else if (attr_it->first == "frame_move") {
            framemove = attr_it->second;
        } else if (attr_it->first == "leaving_delay") {
            leaving_delay = strtod (attr_it->second.c_str(), NULL);
        }
    }
    
    string state_uid = data.current_state_->state_uid();
    
    data.current_state_->setLeavingDelay (leaving_delay);
    
    if (onentry.empty ()) onentry = "onentry_" + state_uid;
    
    if (onexit.empty ()) onexit = "onexit_" + state_uid;
    
    if (framemove.empty ()) framemove = state_uid;
    
    if (manager->private_->history_type_map_[data.scxml_id_][data.current_state_->parent_->state_uid()] == "deep") {
        manager->private_->history_type_map_[data.scxml_id_][state_uid] = "deep";
    } else {
        manager->private_->history_type_map_[data.scxml_id_][state_uid] = history_type;
    }

    if (!manager->private_->history_type_map_[data.scxml_id_][state_uid].empty ()) {
        data.machine->with_history_ = true;
    }

    manager->private_->onentry_action_map_[data.scxml_id_][state_uid] = onentry;
    manager->private_->onexit_action_map_[data.scxml_id_][state_uid] = onexit;
    manager->private_->frame_move_action_map_[data.scxml_id_][state_uid] = framemove;
    
}

void StateMachineManager::PRIVATE::handle_parallel_item(ParseStruct& data, map<string, string> &attributes)
{
    StateMachineManager *manager = data.machine->manager();
    
    string stateid;
    string history_type;
    string onentry;
    string onexit;
    string framemove;
    float leaving_delay = 0;
    map<string,string>::iterator attr_it_end = attributes.end();
    map<string,string>::iterator attr_it = attributes.begin();
    for (; attr_it != attr_it_end; ++attr_it) {
        if (attr_it->first == "history") {
            history_type = attr_it->second;
        } else if (attr_it->first == "onentry") {
            onentry = attr_it->second;
        } else if (attr_it->first == "onexit") {
            onexit = attr_it->second;
        } else if (attr_it->first == "frame_move") {
            framemove = attr_it->second;
        } else if (attr_it->first == "leaving_delay") {
            leaving_delay = strtod (attr_it->second.c_str(), NULL);
        }
    }

    string state_uid = data.current_state_->state_uid();
    
    data.current_state_->setLeavingDelay (leaving_delay);

    if (onentry.empty ()) onentry = "onentry_" + state_uid;
    
    if (onexit.empty ()) onexit = "onexit_" + state_uid;
    
    if (framemove.empty ()) framemove = state_uid;
            
    if (manager->private_->history_type_map_[data.scxml_id_][data.current_state_->parent_->state_uid()] == "deep") {
        manager->private_->history_type_map_[data.scxml_id_][state_uid] = "deep";
    } else {
        manager->private_->history_type_map_[data.scxml_id_][state_uid] = history_type;
    }

    if (!manager->private_->history_type_map_[data.scxml_id_][state_uid].empty ()) {
        data.machine->with_history_ = true;
    }

    manager->private_->onentry_action_map_[data.scxml_id_][state_uid] = onentry;
    manager->private_->onexit_action_map_[data.scxml_id_][state_uid] = onexit;
    manager->private_->frame_move_action_map_[data.scxml_id_][state_uid] = framemove;

}

void StateMachineManager::PRIVATE::handle_final_item(ParseStruct& data, map<string, string> &attributes)
{
    StateMachineManager *manager = data.machine->manager();
    
    string stateid;
    string onentry;
    string framemove;
    map<string,string>::iterator attr_it_end = attributes.end();
    map<string,string>::iterator attr_it = attributes.begin();
    for (; attr_it != attr_it_end; ++attr_it) {
        if (attr_it->first == "onentry") {
            onentry = attr_it->second;
        } else if (attr_it->first == "frame_move") {
            framemove = attr_it->second;
        }
    }

    string state_uid = data.current_state_->state_uid();

    if (onentry.empty ()) onentry = "onentry_" + state_uid;
    if (framemove.empty ()) framemove = state_uid;

    manager->private_->onentry_action_map_[data.scxml_id_][state_uid] = onentry;
    manager->private_->frame_move_action_map_[data.scxml_id_][state_uid] = framemove;
    data.current_state_->is_a_final_ = true;

}

void StateMachineManager::PRIVATE::handle_transition_item(ParseStruct& data, map<string, string> &attributes)
{
    StateMachineManager *manager = data.machine->manager();
    
    TransitionAttr * tran = new TransitionAttr ("","");

    map<string,string>::iterator attr_it_end = attributes.end();
    map<string,string>::iterator attr_it = attributes.begin();
    for (; attr_it != attr_it_end; ++attr_it) {
        if (attr_it->first == "event") {
            tran->event_ = attr_it->second;
        } else if (attr_it->first == "cond") {
            tran->cond_ = attr_it->second;
        } else if (attr_it->first == "ontransit") {
            tran->ontransit_ = attr_it->second;
        } else if (attr_it->first == "target") {
            tran->transition_target_ = attr_it->second;
        } else if (attr_it->first == "random_target") {
            vector<string> values;
            splitStringToVector (attr_it->second.c_str(), values);
            tran->random_target_ = values;
        }
    }

    if (!tran->cond_.empty () && tran->cond_[0] == '!') {
        tran->cond_ = tran->cond_.substr (1);
        tran->not_ = true;
    }

    manager->private_->transition_attr_map_[data.scxml_id_][data.current_state_->state_uid()].push_back (tran);

}

void StateMachineManager::PRIVATE::handle_history_item(ParseStruct& data, map<string, string> &attributes)
{
    StateMachineManager *manager = data.machine->manager();
    
    const string &state_uid = data.current_state_->state_uid();
    
    map<string,string>::iterator attr_it_end = attributes.end();
    map<string,string>::iterator attr_it = attributes.begin();
    for (; attr_it != attr_it_end; ++attr_it) {
        if (attr_it->first == "type") {
            manager->private_->history_type_map_[data.scxml_id_][state_uid] = attr_it->second;
        } else if (attr_it->first == "id") {
            manager->private_->history_id_reside_state_[data.scxml_id_][attr_it->second] = state_uid;
        }
    }

}


bool StateMachineManager::PRIVATE::parse_scm_tree (ParseStruct &data, string const&scm_str)
{

    ptree pt;
    
    int idx=0;
    char first_char = scm_str[idx++];
    while (isspace(first_char)) {
        first_char = scm_str[idx++];
    }
    
    if (first_char == '<') {
        // xml
        try {
            istringstream stream(scm_str);
            read_xml(stream, pt);
            parse_element(data, pt, 0);
        } catch (exception &e) {
            // read xml failed
            cerr << "read scm scxml failed: " << e.what() << endl;
            return false;
        }
    } else {
        // json
        string scm_json = scm_str;
        for (size_t i=0; i < scm_json.length(); ++i) {
            if (scm_json[i] == '\'') {
                scm_json[i] = '"';
            }
        }
        try {
            istringstream stream(scm_json);
            read_json(stream, pt);
            parse_element(data, pt, 0);
        } catch (exception &e) {
            // read json failed
            cerr << "read scm json failed: " << e.what() << endl;
            return false;
        }
        
    }
    
    return true;
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


StateMachine *StateMachineManager::getMach (string const&scxml_id)
{
    return private_->getMach(scxml_id);
}

void StateMachineManager::set_scxml(const string& scxml_id, const string& scxml_str)
{
    private_->scxml_map_[scxml_id] = scxml_str;
}

void StateMachineManager::set_scxml_file(const string& scxml_id, const string& scxml_filepath)
{
    private_->scxml_map_[scxml_id] = "file:" + scxml_filepath;
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
            if (it->second.substr(0, 5) == "file:") {
                mach->scxml_loaded_ = this->loadMachFromFile(mach, it->second.substr(5));
            } else {
                mach->scxml_loaded_ = this->loadMachFromString(mach, it->second);
            }
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
    
    FileCloser guard(zfile);

    fseek(zfile, 0, SEEK_END);
    size_t file_length = ftell (zfile);
    rewind (zfile);
    string scxml_str;
    scxml_str.resize(file_length);
    size_t readlen = fread(&scxml_str[0], 1, file_length, zfile);
    if (readlen < file_length) {
        return false;
    }
    return loadMachFromString(mach, scxml_str);
}

bool StateMachineManager::loadMachFromString(StateMachine* mach, const string& scm_str)
{
    ParseStruct parse;
    parse.scxml_id_ = mach->scxml_id ();
    parse.machine = mach;
    parse.current_state_ = mach;
    private_->transition_attr_map_[parse.scxml_id_].clear();

    return private_->parse_scm_tree(parse, scm_str);
}


StateMachine* StateMachineManager::PRIVATE::getMach(const string& scxml_id)
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
            if (it->second.substr(0, 5) == "file:") {
                mach->scxml_loaded_ = manager_->loadMachFromFile(mach, it->second.substr(5));
            } else {
                mach->scxml_loaded_ = manager_->loadMachFromString(mach, it->second);
            }
        }
        return mach->clone ();
    }
}

void StateMachineManager::PRIVATE::clearMachMap ()
{
    for (map <string, StateMachine *>::iterator it=mach_map_.begin (); it != mach_map_.end (); ++it) {
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

string const& StateMachineManager::history_id_resided_state(const string& scxml_id, const string& history_id) const
{
    return private_->history_id_reside_state_[scxml_id][history_id];
}


string const& StateMachineManager::history_type(const string& scxml_id, string const& state_uid) const
{
    return private_->history_type_map_[scxml_id][state_uid];
}

const string& StateMachineManager::initial_state_of_state(const string& scxml_id, string const& state_uid) const
{
    return private_->initial_state_map_[scxml_id][state_uid];
}

const string& StateMachineManager::onentry_action(const string& scxml_id, string const& state_uid) const
{
    return private_->onentry_action_map_[scxml_id][state_uid];
}

const string& StateMachineManager::onexit_action(const string& scxml_id, string const& state_uid) const
{
    return private_->onexit_action_map_[scxml_id][state_uid];
}

const string& StateMachineManager::frame_move_action(const string& scxml_id, string const& state_uid) const
{
    return private_->frame_move_action_map_[scxml_id][state_uid];
}

vector< TransitionAttr* > StateMachineManager::transition_attr(const string& scxml_id, string const& state_uid) const
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
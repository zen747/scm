#include <scm/StateMachineManager.h>
#include <scm/uncopyable.h>

#include <iostream>

using namespace std;
using namespace SCM;

std::string client_scxml = "\
   <scxml> \
       <state id='new'> \
           <transition event='punch' target='punching'/> \
       </state> \
       <parallel id='live'> \
            <state id='punch'> \
                <transition event='fail' target='punch_fail'/> \
                <state id='punching'> \
                    <transition event='linked' target='linked'/> \
                </state> \
                <state id='linked'> \
                    <transition event='established' target='punch_success'/> \
                </state> \
                <state id='punch_success'> \
                </state> \
                <state id='punch_fail'> \
                    <transition event='try_again' target='punching'/> \
                </state> \
            </state> \
            <state id='mode'> \
                <state id='relay'> \
                    <state id='relay_init'> \
                        <transition event='cipher_ready' target='relay_work'/> \
                    </state> \
                    <state id='relay_work'> \
                        <transition cond='In(punch_success)' target='established'/> \
                    </state> \
                </state> \
                <state id='established'> \
                </state> \
            </state> \
       </parallel> \
       <transition event='close' target='closed'/> \
       <final id='closed'>' \
       </final> \
    </scxml> \
";

std::string client_json = "{\
    'scxml': {\
        'state': {\
            'id': 'new', \
            'transition': {\
                'event': 'punch',\
                'target': 'punching'\
            }\
        },\
        'parallel': {\
            'id': 'live', \
            'state': {\
                'id': 'punch', \
                'transition': {\
                    'event': 'fail',\
                    'target': 'punch_fail'\
                },\
                'state': {\
                    'id': 'punching',\
                    'transition': {\
                        'event': 'linked',\
                        'target': 'linked'\
                    }\
                },\
                'state': {\
                    'id': 'linked',\
                    'transition': {\
                        'event': 'established',\
                        'target': 'punch_success'\
                    }\
                },\
                'state': {\
                    'id': 'punch_success'\
                },\
                'state': {\
                    'id': 'punch_fail',\
                    'transition': {\
                        'event': 'try_again', \
                        'target': 'punching' \
                    }\
                }\
            },\
            'state': {\
                'id': 'mode',\
                'state': {\
                    'id': 'relay',\
                    'state': {\
                        'id':'relay_init',\
                        'transition': {\
                            'event': 'cipher_ready',\
                            'target': 'relay_work'\
                        }\
                    },\
                    'state': {\
                        'id': 'relay_work', \
                        'transition': {\
                            'cond': 'In(punch_success)',\
                            'target': 'established'\
                        }\
                    }\
                },\
                'state': {\
                    'id': 'established'\
                }\
            }\
        },\
        'transition': {\
            'event': 'close',\
            'target': 'closed'\
        },\
        'final': {\
            'id': 'closed'\
        }\
    }\
}\
";

class TheMachine : public Uncopyable
{
    StateMachine *mach_;
    
public:
    TheMachine()
    {
        mach_ = StateMachineManager::instance()->getMach("test-machine");
        mach_->retain();
        REGISTER_STATE_SLOT (mach_, "new", &TheMachine::onentry_new, &TheMachine::onexit_new, this);
        REGISTER_STATE_SLOT (mach_, "live", &TheMachine::onentry_live, &TheMachine::onexit_live, this);
        REGISTER_STATE_SLOT (mach_, "punch", &TheMachine::onentry_punch, &TheMachine::onexit_punch, this);
        REGISTER_STATE_SLOT (mach_, "punching", &TheMachine::onentry_punching, &TheMachine::onexit_punching, this);
        REGISTER_STATE_SLOT (mach_, "linked", &TheMachine::onentry_linked, &TheMachine::onexit_linked, this);
        REGISTER_STATE_SLOT (mach_, "punch_success", &TheMachine::onentry_punch_success, &TheMachine::onexit_punch_success, this);
        REGISTER_STATE_SLOT (mach_, "punch_fail", &TheMachine::onentry_punch_fail, &TheMachine::onexit_punch_fail, this);
        REGISTER_STATE_SLOT (mach_, "mode", &TheMachine::onentry_mode, &TheMachine::onexit_mode, this);
        REGISTER_STATE_SLOT (mach_, "relay", &TheMachine::onentry_relay, &TheMachine::onexit_relay, this);
        REGISTER_STATE_SLOT (mach_, "relay_init", &TheMachine::onentry_relay_init, &TheMachine::onexit_relay_init, this);
        REGISTER_STATE_SLOT (mach_, "relay_work", &TheMachine::onentry_relay_work, &TheMachine::onexit_relay_work, this);
        REGISTER_STATE_SLOT (mach_, "established", &TheMachine::onentry_established, &TheMachine::onexit_established, this);
        REGISTER_STATE_SLOT (mach_, "closed", &TheMachine::onentry_closed, &TheMachine::onexit_closed, this);

        mach_->StartEngine();
    }
    
    ~TheMachine ()
    {
        mach_->release();
    }
    
    void onentry_new ()
    {
        cout << "onentry_new" << endl;
    }
    
    void onexit_new ()
    {
        cout << "onexit_new" << endl;
    }
    
    void onentry_live ()
    {
        cout << "onentry_live" << endl;
    }
    
    void onexit_live ()
    {
        cout << "onexit_live" << endl;
    }
    
    void onentry_punch ()
    {
        cout << "onentry_punch" << endl;
    }
    
    void onexit_punch ()
    {
        cout << "onexit_punch" << endl;
    }
    
    void onentry_punching ()
    {
        cout << "onentry_punching" << endl;
    }
    
    void onexit_punching ()
    {
        cout << "onexit_punching" << endl;
    }
    
    void onentry_linked ()
    {
        cout << "onentry_linked" << endl;
    }
    
    void onexit_linked ()
    {
        cout << "onexit_linked" << endl;
    }
    
    void onentry_punch_success ()
    {
        cout << "onentry_punch_success" << endl;
    }
    
    void onexit_punch_success ()
    {
        cout << "onexit_punch_success" << endl;
    }
    
    void onentry_punch_fail ()
    {
        cout << "onentry_punch_fail" << endl;
    }
    
    void onexit_punch_fail ()
    {
        cout << "onexit_punch_fail" << endl;
    }
    
    void onentry_mode ()
    {
        cout << "onentry_mode" << endl;
    }
    
    void onexit_mode ()
    {
        cout << "onexit_mode" << endl;
    }
    
    void onentry_relay ()
    {
        cout << "onentry_relay" << endl;
    }
    
    void onexit_relay ()
    {
        cout << "onexit_relay" << endl;
    }
    
    void onentry_relay_init ()
    {
        cout << "onentry_relay_init" << endl;
    }
    
    void onexit_relay_init ()
    {
        cout << "onexit_relay_init" << endl;
    }
    
    void onentry_relay_work ()
    {
        cout << "onentry_relay_work" << endl;
    }
    
    void onexit_relay_work ()
    {
        cout << "onexit_relay_work" << endl;
    }
    
    void onentry_established ()
    {
        cout << "onentry_established" << endl;
    }
    
    void onexit_established ()
    {
        cout << "onexit_established" << endl;
    }
    
    void onentry_closed ()
    {
        cout << "onentry_closed" << endl;
    }
    
    void onexit_closed ()
    {
        assert(0 && "exit final");
        cout << "onexit_closed" << endl;
    }
    
    void test ()
    {
        mach_->enqueEvent("punch");
        mach_->enqueEvent("linked");
        mach_->frame_move(0);
    }
};

int main(int argc, char* argv[])
{
    AutoReleasePool apool;
#if USE_XML
    StateMachineManager::instance()->set_scxml("test-machine", client_scxml);
#else
    StateMachineManager::instance()->set_scxml("test-machine", client_json);
#endif
    {
        TheMachine mach;
        mach.test ();
    }
    return 0;
}

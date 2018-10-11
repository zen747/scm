``` {style="color:#1f1c1b;background-color:#ffffff;"}
This is a c++ library supporting statechart (in scxml format) originated by David Harel.
Features like hierarchical states, parallel states, and history are ready for you to command.
You won't find another one as easy and flexible to use as SCM.

Just read this simple example.

#include <scm/StateMachineManager.h>
#include <scm/uncopyable.h>

#include <iostream>

using namespace std;
using namespace scm;

std::string client_scxml = "\
   <scxml> \
       <state id='appear'> \
           <transition event='born' ontransit='say_hello' target='live'/> \
       </state> \
       <parallel id='live'> \
            <transition event='hp_zero' target='dead'/> \
            <state id='eat'> \
            </state> \
            <state id='move'> \
            </state> \
       </parallel> \
       <final id='dead'/>' \
    </scxml> \
";

class Life: public Uncopyable
{
    StateMachine *mach_;
    
public:
    Life()
    {
        mach_ = StateMachineManager::instance()->getMach("the life");
        mach_->retain();
        mach_->set_do_exit_state_on_destroy(true);
        REGISTER_STATE_SLOT (mach_, "appear", &Life::onentry_appear, &Life::onexit_appear, this);
        REGISTER_STATE_SLOT (mach_, "live", &Life::onentry_live, &Life::onexit_live, this);
        REGISTER_STATE_SLOT (mach_, "eat", &Life::onentry_eat, &Life::onexit_eat, this);
        REGISTER_STATE_SLOT (mach_, "move", &Life::onentry_move, &Life::onexit_move, this);
        REGISTER_STATE_SLOT (mach_, "dead", &Life::onentry_dead, &Life::onexit_dead, this);
        REGISTER_ACTION_SLOT(mach_, "say_hello", &Life::say_hello, this);
        mach_->StartEngine();
    }
    
    ~Life ()
    {
        mach_->release();
    }
    
    void onentry_appear ()
    {
        cout << "come to exist" << endl;
    }
    
    void onexit_appear()
    {
        cout << "we are going to..." << endl;
    }
    
    void onentry_live ()
    {
        cout << "start living" << endl;
    }
    
    void onexit_live ()
    {
        cout << "no longer live" << endl;
    }
    
    void onentry_eat ()
    {
        cout << "start eating" << endl;
    }
    
    void onexit_eat ()
    {
        cout << "stop eating" << endl;
    }
    
    void onentry_move ()
    {
        cout << "start moving" << endl;
    }
    
    void onexit_move ()
    {
        cout << "stop moving" << endl;
    }
    
    void onentry_dead ()
    {
        cout << "end" << endl;
    }
    
    void onexit_dead ()
    {
        assert (0 && "should not exit final state");
        cout << "no, this won't get called." << endl;
    }
    
    void say_hello ()
    {
        cout << "\n*** Hello, World! ***\n" << endl;
    }
    
    void test ()
    {
        mach_->enqueEvent("born");
        mach_->frame_move(0); // state change to 'live'
        mach_->enqueEvent("hp_zero");
        mach_->frame_move(0); // state change to 'dead'
    }
};

int main(int argc, char* argv[])
{
    AutoReleasePool apool;
    StateMachineManager::instance()->set_scxml("the life", client_scxml);
    {
        Life life;
        life.test ();
    }
    StateMachineManager::instance()->pumpMachEvents();
    StateMachineManager::instance()->release_instance();
    AutoReleasePool::pumpPools();
    return 0;
}



Build this and run it, you should see:
"""
come to exist
we are going to...

*** Hello, World! ***

start living
start eating
start moving
stop eating
stop moving
no longer live
end
"""

Simply,
1. you can load the scxml from external file or from a string defined in your code.
2. you connect these onentry_ onexit_, etc. slots
3. you start the engine, call the framemove in main loop.
Done.

It's that easy!

Read the tutorials at
(English) http://zen747.blogspot.tw/2017/07/a-scm-framework-tutorial-statechart.html
(Traditional Chinese) http://zen747.blogspot.tw/2017/07/scm-framework.html

```

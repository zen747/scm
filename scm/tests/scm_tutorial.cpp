#include <scm/StateMachineManager.h>
#include <scm/uncopyable.h>

#include <iostream>

using namespace std;
using namespace SCM;

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
    return 0;
}

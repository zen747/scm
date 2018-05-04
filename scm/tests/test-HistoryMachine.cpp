#include <scm/StateMachineManager.h>
#include <scm/uncopyable.h>

#include <iostream>

using namespace std;
using namespace SCM;

std::string watch_scxml = "\
   <scxml non-unique='on,off'> \
       <state id='time'> \
            <transition event='a' target='alarm1'/> \
            <transition event='c_down' target='wait'/> \
       </state> \
       <state id='wait'> \
            <transition event='c_up' target='time'/> \
            <transition event='2_sec' target='update'/> \
       </state> \
       <state id='update'> \
            <history id='histu' type='shallow'/> \
            <transition event='d' target='histu'/> \
            <state id='sec'> \
                <transition event='c' target='1min'/> \
            </state> \
            <state id='1min'> \
                <transition event='c' target='10min'/> \
            </state> \
            <state id='10min'> \
                <transition event='c' target='hr'/> \
            </state> \
            <state id='hr'> \
                <transition event='c' target='time'/> \
            </state> \
       </state> \
       <state id='alarm1' history='shallow'> \
            <transition event='a' target='alarm2'/> \
            <state id='off' > \
                <transition event='d' target='on'/> \
            </state> \
            <state id='on' > \
                <transition event='d' target='off'/> \
            </state> \
       </state> \
       <state id='alarm2' history='shallow'> \
            <transition event='a' target='chime'/> \
            <state id='off' > \
                <transition event='d' target='on'/> \
            </state> \
            <state id='on' > \
                <transition event='d' target='off'/> \
            </state> \
       </state> \
       <state id='chime' history='shallow'> \
            <transition event='a' target='stopwatch'/> \
            <state id='off' > \
                    <transition event='d' target='on'/> \
            </state> \
            <state id='on' > \
                <transition event='d' target='off'/> \
            </state> \
       </state> \
       <state id='stopwatch' history='deep'> \
            <transition event='a' target='time'/> \
            <state id='zero'> \
                <transition event='b' target='on,regular'/> \
            </state> \
            <parallel> \
                <state id='run'> \
                    <state id='on' > \
                        <transition event='b' target='off'/> \
                    </state> \
                    <state id='off' > \
                        <transition event='b' target='on'/> \
                    </state> \
                </state> \
                <state id='display'> \
                    <state id='regular'> \
                        <transition event='d' cond='In(on)' target='lap'/> \
                        <transition event='d' cond='In(off)' target='zero'/> \
                    </state> \
                    <state id='lap'> \
                        <transition event='d' target='regular'/> \
                    </state> \
                </state> \
            </parallel> \
       </state> \
    </scxml> \
";


class TheMachine : public Uncopyable
{
    StateMachine *mach_;
    int hr_;
    int min_;
    int sec_;
    
public:
    TheMachine()
    : hr_(0)
    , min_(0)
    , sec_(0)
    {
        mach_ = StateMachineManager::instance()->getMach("watch");
        mach_->retain();
        vector<string> const&states = mach_->get_all_states();
        cout << "we have states: " << endl;
        for (size_t state_idx=0; state_idx < states.size(); ++state_idx) {
            State *st = mach_->getState(states[state_idx]);
            if (st == 0) continue; // <- state machine itself
            for (int i=0; i < st->depth(); ++i) {
                cout << "    ";
            }
            cout << states[state_idx] << endl;
            mach_->setActionSlot ("onentry_" + states[state_idx], boost::bind (&TheMachine::onentry_report_state, this, false));
            mach_->setActionSlot ("onexit_" + states[state_idx], boost::bind (&TheMachine::onexit_report_state, this));
        }
        cout << endl;
        mach_->setActionSlot ("onentry_sec", boost::bind (&TheMachine::onentry_sec, this));
        mach_->setActionSlot ("onentry_1min", boost::bind (&TheMachine::onentry_1min, this));
        mach_->setActionSlot ("onentry_10min", boost::bind (&TheMachine::onentry_10min, this));
        mach_->setActionSlot ("onentry_hr", boost::bind (&TheMachine::onentry_hr, this));
        mach_->StartEngine();
    }
    
    ~TheMachine ()
    {
        mach_->release();
    }
    
    void onentry_sec()
    {
        if (mach_->re_enter_state()) {
            sec_ = 0;
        }
        onentry_report_state(true);
    }
    
    void onentry_1min()
    {
        if (mach_->re_enter_state()) {
            ++min_;
        }
        onentry_report_state(true);
    }
    
    void onentry_10min()
    {
        if (mach_->re_enter_state()) {
            min_ += 10;
        }
        onentry_report_state(true);
    }
    
    void onentry_hr()
    {
        if (mach_->re_enter_state()) {
            ++hr_;
        }
        onentry_report_state(true);
    }
    
    void onentry_report_state(bool with_time)
    {
        ++sec_;
        cout << "enter state " << mach_->getEnterState()->state_uid();
        if (with_time) {
            cout << ". time " << hr_ << ":" << min_ << ":" << sec_;
        }
        cout << endl;
    }
    
    void onexit_report_state()
    {
        cout << "exit state " << mach_->getEnterState()->state_uid() << endl;
    }
    
    void test ()
    {
        // time
        mach_->enqueEvent("c_down"); // -> wait
        mach_->enqueEvent("2_sec"); // -> update, you will use registerTimedEvent to generate event after 2 seconds
        mach_->enqueEvent("d"); //  reset, 1 second
        mach_->enqueEvent("d"); //  reset, 1 second
        mach_->enqueEvent("d"); //  reset, 1 second
        mach_->enqueEvent("c"); // -> 1min state
        mach_->enqueEvent("d"); //  1 min
        mach_->enqueEvent("d"); //  2 min
        mach_->enqueEvent("c"); // -> 10min state
        mach_->enqueEvent("d"); //  12 min
        mach_->enqueEvent("c"); // -> hr state
        mach_->enqueEvent("d"); //  1 hr
        mach_->enqueEvent("d"); //  2 hr
        mach_->enqueEvent("c"); // -> time
        
        mach_->enqueEvent("a"); // -> alarm1
        mach_->enqueEvent("d");
        mach_->enqueEvent("a"); // -> alarm2
        mach_->enqueEvent("d");
        mach_->enqueEvent("a"); // -> chime
        mach_->enqueEvent("d");
        mach_->enqueEvent("a"); // -> stopwatch.zero
        mach_->enqueEvent("b"); // -> run.on
        mach_->enqueEvent("d"); // -> display.lap
        mach_->enqueEvent("a"); // -> time
        mach_->enqueEvent("d"); // no effect
        mach_->enqueEvent("a"); // -> alarm1
        mach_->enqueEvent("d");
        mach_->enqueEvent("a"); // -> alarm2
        mach_->enqueEvent("d");
        mach_->enqueEvent("a"); // -> chime
        mach_->enqueEvent("d");
        mach_->enqueEvent("a"); // -> stopwatch, what's run and display in?
        
        mach_->frame_move(0);
    }
};

int main(int argc, char* argv[])
{
    AutoReleasePool apool;
    StateMachineManager::instance()->set_scxml("watch", watch_scxml);
    {
        TheMachine mach;
        mach.test ();
    }
    return 0;
}

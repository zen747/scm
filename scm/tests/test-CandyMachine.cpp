#include <scm/StateMachineManager.h>
#include <iostream>

using namespace std;
using namespace SCM;

std::string cm_scxml = "\
   <scxml> \
        <state id='idle'> \
            <transition event='empty' target='disabled'/> \
            <transition event='coin' target='active'/> \
        </state> \
        <state id='active'> \
            <transition event='release-candy' ontransit='releaseCandy' target='releasing'/> \
            <transition event='withdraw-coin' ontransit='withdrawCoins' target='idle'/> \
        </state> \
        <state id='releasing'> \
            <transition event='candy-released' cond='condNoCandy' target='disabled'/> \
            <transition event='candy-released' target='idle'/> \
        </state> \
        <state id='disabled'> \
            <transition event='add-candy' cond='condNoCredit' target='idle'/> \
            <transition event='add-candy' target='active'/> \
        </state> \
    </scxml> \
";

class TheCandyMachine : public Uncopyable
{
    StateMachine *mach_;
    int           credit_; // 
    int           num_of_candy_stored_;
    
public:
    TheCandyMachine()
    : credit_(0)
    , num_of_candy_stored_(0)
    {
        mach_ = StateMachineManager::instance()->getMach("cm_scxml");
        REGISTER_STATE_SLOT (mach_, "idle", &TheCandyMachine::onentry_idle, &TheCandyMachine::onexit_idle, this);
        REGISTER_STATE_SLOT (mach_, "active", &TheCandyMachine::onentry_active, &TheCandyMachine::onexit_active, this);
        REGISTER_STATE_SLOT (mach_, "releasing", &TheCandyMachine::onentry_releasing, &TheCandyMachine::onexit_releasing, this);
        REGISTER_STATE_SLOT (mach_, "disabled", &TheCandyMachine::onentry_disabled, &TheCandyMachine::onexit_disabled, this);

        //boost::function<bool() const> cond_slot;
        REGISTER_COND_SLOT(mach_, "condNoCandy", &TheCandyMachine::condNoCandy, this);
        REGISTER_COND_SLOT(mach_, "condNoCredit", &TheCandyMachine::condNoCredit, this);
        
        // boost::function<void()> 
        REGISTER_ACTION_SLOT(mach_, "releaseCandy", &TheCandyMachine::releaseCandy, this);
        REGISTER_ACTION_SLOT(mach_, "withdrawCoins", &TheCandyMachine::withdrawCoins, this);
        
        mach_->StartEngine();
    }
    
    ~TheCandyMachine ()
    {
        mach_->ShutDownEngine(true);
    }
    
    void store_candy (int num)
    {
        num_of_candy_stored_ += num;
        mach_->enqueEvent("add-candy");
        cout << "store " << num << " gumballs, now machine has " << num_of_candy_stored_ << " gumballs." << endl;
    }
    
    void insertQuater ()
    {
        insert_coin(25);
        cout << "you insert a quarter, now credit = " << credit_ << endl;
    }
    
    void ejectQuater ()
    {
        mach_->enqueEvent("withdraw-coin");
        cout << "you pulled the eject crank" << endl;
    }
    
    void turnCrank ()
    {
        mach_->enqueEvent("release-candy");
        cout << "you turned release crank" << endl;
    }
    
protected:
    
    void insert_coin (int credit)
    {
        credit_ += credit;
        mach_->enqueEvent("coin");
    }
        
    void onentry_idle ()
    {
        cout << "onentry_idle" << endl;
        cout << "Machine is waiting for quarter" << endl;
        if (num_of_candy_stored_ == 0) {
            mach_->enqueEvent ("empty");
        }
    }
    
    void onexit_idle ()
    {
        cout << "onexit_idle" << endl;
    }
    
    void onentry_active ()
    {
        cout << "onentry_active" << endl;
    }
    
    void onexit_active ()
    {
        cout << "onexit_active" << endl;
    }
    
    void onentry_releasing ()
    {
        cout << "onentry_releasing" << endl;
        //WorkerManager::instance()->perform_work_after(1.0, boost::bind(&TheCandyMachine::candy_released, this), false);
        candy_released ();
    }
    
    void onexit_releasing ()
    {
        cout << "onexit_releasing" << endl;
    }
    
    void onentry_disabled ()
    {
        cout << "onentry_disabled" << endl;
    }
    
    void onexit_disabled ()
    {
        cout << "onexit_disabled" << endl;
    }
    
    bool condNoCandy () const
    {
        return num_of_candy_stored_ == 0;
    }
    
    bool condNoCredit () const
    {
        return credit_ == 0;
    }
    
    void releaseCandy ()
    {
        int num_to_release = credit_ / 25;
        if (num_to_release > num_of_candy_stored_) {
            num_to_release = num_of_candy_stored_;
        }
        cout << "release " << num_to_release << " gumballs" << endl;
        num_of_candy_stored_ -= num_to_release;
        credit_ -= num_to_release * 25;
    }
    
    void withdrawCoins ()
    {
        cout << "there you go, the money, " << credit_ << endl;
        credit_ = 0;
        cout << "Quarter returned" << endl;
    }
    
    void candy_released ()
    {
        mach_->enqueEvent("candy-released");
    }
    
public:
    
    void report ()
    {
        cout << "\nA Candy Selling Machine\n";
        cout << "Inventory: " << num_of_candy_stored_ << " gumballs\n";
        cout << "Credit: " << credit_ << endl << endl;
    }
    
    void init ()
    {
        mach_->frame_move(0);
        assert (mach_->inState("disabled"));
        this->store_candy(5);
        mach_->frame_move(0);
        assert (mach_->inState("idle"));
        report ();        
    }
    
    void frame_move ()
    {
        mach_->frame_move(0);
    }
    
    void test ()
    {
        this->insertQuater();
        frame_move();
        this->turnCrank();
        frame_move();
        report ();
        
        this->insertQuater();
        frame_move();
        this->ejectQuater();
        frame_move();
        report ();
        this->turnCrank();
        frame_move();
        report ();
        
        this->insertQuater(); frame_move();
        this->turnCrank(); frame_move();
        this->insertQuater(); frame_move();
        this->turnCrank(); frame_move();
        this->ejectQuater(); frame_move();
        report();

        this->insertQuater(); frame_move();
        this->insertQuater(); frame_move();
        this->turnCrank(); frame_move();
        this->insertQuater(); frame_move();
        this->turnCrank(); frame_move();
        this->insertQuater(); frame_move();
        this->ejectQuater(); frame_move();
        report();

        this->store_candy(5);
        frame_move();
        this->turnCrank(); frame_move();        
        report();
    }
};

int main(int argc, char* argv[])
{
    AutoReleasePool apool;
    StateMachineManager::instance()->set_scxml("cm_scxml", cm_scxml);
    {
        TheCandyMachine mach;
        mach.init ();
        mach.test ();
    }
    
    return 0;
}

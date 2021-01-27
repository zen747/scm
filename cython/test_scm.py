from scm import *

client_scxml = """\
   <scxml non-unique='on,off'> 
       <state id='appear'> 
           <transition event='born' ontransit='say_hello' target='live'/> 
       </state> 
       <parallel id='live'> 
            <transition event='hp_zero' target='dead'/> 
            <state id='eat'> 
                <state id='on'/>
                <state id='off'/>
            </state> 
            <state id='move'> 
                <state id='on'/>
                <state id='off'/>
            </state> 
       </parallel> 
       <final id='dead'/>
    </scxml> 
"""

class Life:
    def __init__(self):
        self.mach_ = PyStateMachineManager.instance().getMach('the life')
        self.mach_.set_do_exit_state_on_destroy(True)
#        self.mach_.register_state_slot("appear", self.onentry_appear, self.onexit_appear)
#        self.mach_.register_state_slot("live", self.onentry_live, self.onexit_live)
#        self.mach_.register_state_slot("eat", self.onentry_eat, self.onexit_eat)
#        self.mach_.register_state_slot("move", self.onentry_move, self.onexit_move)
#        self.mach_.register_state_slot("dead", self.onentry_dead, self.onexit_dead)
#        self.mach_.register_state_slot("move.on", self.onentry_move_on, self.onexit_move_on)
        # Instead of register every entry/exit slot that follow this 'onentry_xxx', 'onexit_xxx' naming convetion,
        # You can simply use StateMachine.register_handler() for brevity,
        self.mach_.register_handler(self)
        # For other cases, use setActionSlot as usual.
        self.mach_.setActionSlot('say_hello', self.say_hello)
        print(">> let's start the engine")
        self.mach_.StartEngine()
        print(">> should enter the initial state appear")
        
    def test(self):
        self.mach_.enqueEvent("born")
        self.mach_.frame_move(0.001) # state change to 'live'
        # or 
        #PyStateMachineManager.instance().pumpMachEvents()
        self.mach_.enqueEvent("hp_zero")
        #self.mach_.frame_move(0.001) # state change to 'dead'
        # or 
        PyStateMachineManager.instance().pumpMachEvents()
        
    def onentry_appear(self):
        print("come to exist")
    
    def onexit_appear(self):
        print("we are going to...")
    
    def onentry_live(self):
        print("start living")
    
    def onexit_live(self):
        print("no longer live")
    
    def onentry_eat(self):
        print("start eating")
    
    def onexit_eat(self):
        print("stop eating")
    
    def onentry_move(self):
        print("start moving")
    
    def onexit_move(self):
        print("stop moving")
        
    def onentry_move_on(self):
        print("start move on")
    
    def onexit_move_on(self):
        print("stop move on")
        
    def onentry_dead(self):
        print("end")
    
    def onexit_dead(self):
        assert (0 and "should not exit final state");
        print("no, this won't get called.")
    
    def say_hello(self):
        print("\n*** Hello, World! ***\n")


if __name__ == '__main__':
    # Because of underlying c++ implementation, we have to declare an autorelease pool.
    pool = PyAutoReleasePool()
    PyStateMachineManager.instance().set_scxml("the life", client_scxml)

    life = Life()
    life.test()
    
    pool.pumpPools()
    del pool
    
    print("that's all, bye!")

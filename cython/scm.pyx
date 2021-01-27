# distutils: language=c++

from cscm cimport *
from libcpp.string cimport string

cdef class PyAutoReleasePool:
    cdef AutoReleasePool *c_ptr
    def __cinit__(self):
        self.c_ptr = new AutoReleasePool()
        
    def __dealloc__(self):
        del self.c_ptr
        
    def pumpPool(self):
        self.c_ptr.pumpPool()
        
    @classmethod
    def pumpPools(self):
        AutoReleasePool.pumpPools()

cdef class PyStateMachine:
    cdef StateMachine *c_ptr
        
    def __dealloc__(self):
        self.c_ptr.release()
        
    def StartEngine(self):
        self.c_ptr.StartEngine()
        
    def enqueEvent(self, event):
        self.c_ptr.enqueEvent(event.encode())
        
    def setActionSlot(self, action, slot):
        self.c_ptr.setActionSlot(action.encode(), PyObjWrapper(slot))
    
    def frame_move(self, elapsed_time):
        self.c_ptr.frame_move(elapsed_time)
        
    def set_do_exit_state_on_destroy(self, yesno):
        self.c_ptr.set_do_exit_state_on_destroy(yesno)
        
    def get_all_states(self):
        return self.c_ptr.get_all_states()
    
    @staticmethod
    cdef create(StateMachine *mach):
        py_mach = PyStateMachine()
        py_mach.c_ptr = mach
        py_mach.c_ptr.retain()
        return py_mach
    
    def register_state_slot(self, state, onentry, onexit):
        self.setActionSlot('onentry_'+state, onentry)
        self.setActionSlot('onexit_'+state, onexit)
    
    def register_handler(self, handler):
        states = self.get_all_states()
        for stateb in states:
            state = stateb.decode()
            s = state.replace('.', '_')
            try:
                self.setActionSlot('onentry_'+state, eval('handler.onentry_' + s))
            except:
                pass
            try:
                self.setActionSlot('onexit_'+state, eval('handler.onexit_' + s))
            except:
                pass

_py_state_machine_manager_instance = None

cdef class PyStateMachineManager:
    cdef StateMachineManager *c_ptr
    def __cinit__(self):
        self.c_ptr = new StateMachineManager()
        
    def __dealloc__(self):
        del self.c_ptr
        
    def getMach(self, scxml_id):
        cdef StateMachine *mach = self.c_ptr.getMach(scxml_id.encode())
        return PyStateMachine.create(mach) 
        
    def set_scxml(self, scxml_id, scxml_str):
        self.c_ptr.set_scxml(scxml_id.encode(), scxml_str.encode())
        
    def pumpMachEvents(self):
        self.c_ptr.pumpMachEvents()

    @classmethod
    def instance(self):
        global _py_state_machine_manager_instance
        if not _py_state_machine_manager_instance:
            _py_state_machine_manager_instance = PyStateMachineManager()
        return _py_state_machine_manager_instance

    @classmethod
    def release_instance(self):
        _py_state_machine_manager_instance = None

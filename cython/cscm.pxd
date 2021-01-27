from libcpp.string cimport string
from libcpp.vector cimport vector

cdef extern from "py_obj_wrapper.hpp":
    cdef cppclass PyObjWrapper:
        PyObjWrapper() 
        PyObjWrapper(pyobject)
        
cdef extern from "scm/RefCountObject.h" namespace "scm":
    cdef cppclass RefCountObject:
        void retain()
        void release()
        void autorelease()
        int ref_count() const
        bint unique_ref() const
        
    cdef cppclass AutoReleasePool:
        AutoReleasePool() except +
        void pumpPool()
        @staticmethod
        void pumpPools()

cdef extern from "scm/FrameMover.h" namespace "scm":
    cdef cppclass FrameMover(RefCountObject):
        void frame_move(float elapsed_time)
        void setPause (bint pause)
        void togglePause ()
        void pause ()
        void resume ()
        inline bint paused () const
        inline double total_elapsed_time () const
        void reset_time ()
    
cdef extern from "scm/State.h" namespace "scm":
    cdef cppclass State(FrameMover):
        pass
        
cdef extern from "scm/StateMachine.h" namespace "scm":
    cdef cppclass StateMachine(State):
        void StartEngine()
        void enqueEvent(const string event)
        void setActionSlot(const string action, PyObjWrapper) except +
        void set_do_exit_state_on_destroy(bint)
        const vector[string] get_all_states() const
                
cdef extern from "scm/StateMachineManager.h" namespace "scm":
    cdef cppclass StateMachineManager:
        @staticmethod
        StateMachineManager * instance() except +
        @staticmethod
        void release_instance()
        StateMachineManager() except +
        StateMachine *getMach(const string scxml_id)
        void set_scxml(const string scxml_id, const string scxml_str)
        void pumpMachEvents()
        const vector[string] get_all_states(const string scxml_id) const

#include <Python.h>
#include "call_py_obj.h"

class PyObjWrapper {
    PyObject * held_obj_;
public:
    PyObjWrapper(PyObject *o)
    : held_obj_(o)
    {
        Py_XINCREF(o);
    }
    
    PyObjWrapper(const PyObjWrapper &rhs)
    : PyObjWrapper(rhs.held_obj_)
    {
    }
    
    PyObjWrapper(PyObjWrapper &rhs)
    : PyObjWrapper(rhs.held_obj_)
    {
        rhs.held_obj_ = 0;
    }
    
    PyObjWrapper():PyObjWrapper(nullptr)
    {
    }
    
    ~PyObjWrapper()
    {
        Py_XDECREF(held_obj_);
    }
    
    PyObjWrapper &operator=(const PyObjWrapper &rhs)
    {
        PyObjWrapper tmp = rhs;
        return (*this = std::move(tmp));
    }
    
    PyObjWrapper &operator=(PyObjWrapper &rhs)
    {
        held_obj_ = rhs.held_obj_;
        rhs.held_obj_ = 0;
        return *this;
    }
    
    void operator()(void)
    {
        if (held_obj_) {
            call_py_obj(held_obj_);
        }
    }
    
};

cmake_minimum_required(VERSION 3.0)
# project name, statechart machine
PROJECT(scm)

FIND_PACKAGE(Boost REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})

# source files
set (STATE_SRCS 
    RefCountObject.cpp
    FrameMover.cpp
    StateMachineManager.cpp
    StateMachine.cpp
    Parallel.cpp
    State.cpp
    RefCountObject.h
    FrameMover.h
    StateMachineManager.h
    StateMachine.h
    Parallel.h
    State.h
)

add_library(scm_static STATIC ${STATE_SRCS})
add_library(scm SHARED ${STATE_SRCS})
install (TARGETS scm_static DESTINATION lib)
install (TARGETS scm DESTINATION lib)

add_subdirectory(tests)

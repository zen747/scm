cmake_minimum_required (VERSION 2.8.4)

# project name, statechart machine
PROJECT(scm)

FIND_PACKAGE(Boost REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})

FIND_PACKAGE(EXPAT REQUIRED)
INCLUDE_DIRECTORIES(${EXPAT_INCLUDE_DIRS})

# definitions
ADD_DEFINITIONS ("-DXML_STATIC")

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

add_library(scm STATIC ${STATE_SRCS})
install (TARGETS scm DESTINATION lib)

add_subdirectory(tests)

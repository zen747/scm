#ifndef Parallel_H
#define Parallel_H

#include "State.h"

namespace SCM {

/** Parallel state
* 以 scxml 為基礎。請參考 https://www.w3.org/TR/scxml/
* Based on scxml. please refer to https://www.w3.org/TR/scxml/
*/
class Parallel: public State  // a parallel region
{
public:
    Parallel (std::string const& state_id, State* parent, StateMachine *machine);


    virtual bool inState (std::string const& state_id, bool recursive=true) const;
    virtual bool inState (State const* state, bool recursive=true) const;
    virtual void makeSureEnterStates();

    virtual Parallel *clone (State *parent, StateMachine *m);

protected:
    virtual void onEvent (std::string const &e);
    virtual void enterState (bool enter_substate=true);
    virtual void exitState ();
    virtual void doEnterState (std::vector<State *> &vps);
    virtual void onFrameMove (float t);


    std::set<std::string>  finished_substates_;

};

}

#endif

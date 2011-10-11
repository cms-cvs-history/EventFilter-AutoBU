/*
 * BStateMachine.h
 *
 * AutoBU State Chart
 *
 *  Created on: Sep 21, 2011
 *      Author: aspataru - andrei.cristian.spataru@cern.ch
 */

#ifndef EVFSTATEMACHINE_H_
#define EVFSTATEMACHINE_H_

#include "EventFilter/AutoBU/interface/SharedResources.h"
#include "EventFilter/Utilities/interface/Exception.h"

#include <boost/statechart/event.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/transition.hpp>
#include <boost/mpl/list.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace bsc = boost::statechart;

namespace evf {

////////////////////////////////////////////////
//// Forward declarations of state classes: ////
////////////////////////////////////////////////

// Outer states:
class Failed;
class Normal;

// Inner states of Normal:
class Halted;
class Halting;
class Configuring;
class Ready;

// Inner states of Ready:
class Stopped;
class Enabled;
class Enabling;
class Stopping;

// Inner states of Enabled:
class Executing;

////////////////////////////
//// Transition events: ////
////////////////////////////

class Configure: public bsc::event<Configure> {
};
class ConfigureDone: public bsc::event<ConfigureDone> {
};
class Enable: public bsc::event<Enable> {
};
class EnableDone: public bsc::event<EnableDone> {
};
class Stop: public bsc::event<Stop> {
};
class StopDone: public bsc::event<StopDone> {
};
class Halt: public bsc::event<Halt> {
};
class HaltDone: public bsc::event<HaltDone> {
};
class Fail: public bsc::event<Fail> {
};

//______________________________________________________________________________
/**
 Abstract base for state classes

 */

class BaseState {

public:

	BaseState();
	virtual ~BaseState() = 0;
	std::string stateName() const;
	void moveToFailedState(xcept::Exception& exception) const;

protected:

	virtual std::string do_stateName() const = 0;

	virtual void do_moveToFailedState(xcept::Exception& exception) const = 0;

	void safeEntryAction();
	virtual void do_entryActionWork() = 0;

	void safeExitAction();
	virtual void do_exitActionWork() = 0;

};

//______________________________________________________________________________
/**
 State machine class

 */

class BStateMachine: public bsc::state_machine<BStateMachine, Normal> {

public:

	BStateMachine(SharedResourcesPtr sr);
	~BStateMachine();

	std::string getCurrentStateName() const;
	BaseState const& getCurrentState() const;

	SharedResourcesPtr getSharedResources() const {
		return sharedResources_;
	}

	void setExternallyVisibleState(const std::string&);

private:

	SharedResourcesPtr sharedResources_;

};

////////////////////////
//// State classes: ////
////////////////////////

//______________________________________________________________________________
/**
 Failed state

 */
class Failed: public bsc::state<Failed, BStateMachine>, public BaseState {

public:

	Failed( my_context);
	virtual ~Failed();

private:

	virtual std::string do_stateName() const;
	virtual void do_entryActionWork();
	virtual void do_exitActionWork();
	virtual void do_moveToFailedState(xcept::Exception& exception) const;

};

//______________________________________________________________________________
/**
 Normal state

 */

class Normal: public bsc::state<Normal, BStateMachine, Halted>,
		public BaseState {

public:

	typedef bsc::transition<Fail, Failed> FT;
	typedef boost::mpl::list<FT> reactions;

	Normal( my_context);
	virtual ~Normal();

private:

	virtual std::string do_stateName() const;
	virtual void do_entryActionWork();
	virtual void do_exitActionWork();
	virtual void do_moveToFailedState(xcept::Exception& exception) const;
};

//______________________________________________________________________________
/**
 Halted state

 */

class Halted: public bsc::state<Halted, Normal>, public BaseState {

public:

	typedef bsc::transition<Configure, Configuring> RT;
	typedef boost::mpl::list<RT> reactions;

	Halted( my_context);
	virtual ~Halted();

private:

	virtual std::string do_stateName() const;
	virtual void do_entryActionWork();
	virtual void do_exitActionWork();
	virtual void do_moveToFailedState(xcept::Exception& exception) const;

};

//______________________________________________________________________________
/**
 Configuring state

 */

class Configuring: public bsc::state<Configuring, Normal>, public BaseState {

public:

	typedef bsc::transition<ConfigureDone, Ready> CR;
	typedef boost::mpl::list<CR> reactions;

	Configuring( my_context);
	virtual ~Configuring();

private:

	virtual std::string do_stateName() const;
	virtual void do_entryActionWork();
	virtual void do_exitActionWork();
	virtual void do_moveToFailedState(xcept::Exception& exception) const;

};

//______________________________________________________________________________
/**
 Ready state

 */

class Ready: public bsc::state<Ready, Normal, Stopped>, public BaseState {

public:

	typedef bsc::transition<Halt, Halting> HT;
	typedef boost::mpl::list<HT> reactions;

	Ready( my_context);
	virtual ~Ready();

private:

	virtual std::string do_stateName() const;
	virtual void do_entryActionWork();
	virtual void do_exitActionWork();
	virtual void do_moveToFailedState(xcept::Exception& exception) const;

};

//______________________________________________________________________________
/**
 Stopped state

 */

class Stopped: public bsc::state<Stopped, Ready>, public BaseState {

public:

	typedef bsc::transition<Enable, Enabling> ET;
	typedef boost::mpl::list<ET> reactions;

	Stopped( my_context);
	virtual ~Stopped();

private:

	virtual std::string do_stateName() const;
	virtual void do_entryActionWork();
	virtual void do_exitActionWork();
	virtual void do_moveToFailedState(xcept::Exception& exception) const;

};

//______________________________________________________________________________
/**
 Enabling state

 */

class Enabling: public bsc::state<Enabling, Ready>, public BaseState {

public:

	typedef bsc::transition<EnableDone, Enabled> ED;
	typedef boost::mpl::list<ED> reactions;

	Enabling( my_context);
	virtual ~Enabling();

private:

	virtual std::string do_stateName() const;
	virtual void do_entryActionWork();
	virtual void do_exitActionWork();
	virtual void do_moveToFailedState(xcept::Exception& exception) const;

};

//______________________________________________________________________________
/**
 Enabled state

 */

class Enabled: public bsc::state<Enabled, Ready, Executing>, public BaseState {

public:

	typedef bsc::transition<Stop, Stopping> ST;
	typedef boost::mpl::list<ST> reactions;

	Enabled( my_context);
	virtual ~Enabled();

private:

	virtual std::string do_stateName() const;
	virtual void do_entryActionWork();
	virtual void do_exitActionWork();
	virtual void do_moveToFailedState(xcept::Exception& exception) const;

};

//______________________________________________________________________________
/**
 Executing state

 */

class Executing: public bsc::state<Executing, Enabled>,
		public BaseState,
		public toolbox::lang::Class {

public:

	Executing( my_context);
	virtual ~Executing();

private:

	virtual std::string do_stateName() const;
	virtual void startExecutionWorkLoop() throw (evf::Exception);
	virtual void do_entryActionWork();
	virtual void do_exitActionWork();
	bool execute(toolbox::task::WorkLoop* wl);
	bool generateEvent(ABUEvent* evt);
	/// Creates a message chain for the Resource Broker
	toolbox::mem::Reference *createMsgChain(evf::ABUEvent *evt,
			unsigned int fuResourceId);
	/// Updates the maximum FED size generated
	void updateMaxFedGen(unsigned int testSize);
	virtual void do_moveToFailedState(xcept::Exception& exception) const;

};

//______________________________________________________________________________
/**
 Stopping state

 */

class Stopping: public bsc::state<Stopping, Ready>, public BaseState {

public:

	typedef bsc::transition<StopDone, Stopped> SD;
	typedef boost::mpl::list<SD> reactions;

	Stopping( my_context);
	virtual ~Stopping();

private:

	virtual std::string do_stateName() const;
	virtual void do_entryActionWork();
	virtual void do_exitActionWork();
	virtual void do_moveToFailedState(xcept::Exception& exception) const;

	bool drainQueues();
	toolbox::mem::Reference *createMsgChain(evf::ABUEvent *evt,
			unsigned int fuResourceId);
	bool generateEvent(ABUEvent* evt);
	bool destructionIsDone() const;

};

//______________________________________________________________________________
/**
 Halting state

 */

class Halting: public bsc::state<Halting, Normal>, public BaseState {

public:

	typedef bsc::transition<HaltDone, Halted> HD;
	typedef boost::mpl::list<HD> reactions;

	Halting( my_context);
	virtual ~Halting();

private:

	virtual std::string do_stateName() const;
	virtual void do_entryActionWork();
	virtual void do_exitActionWork();
	virtual void do_moveToFailedState(xcept::Exception& exception) const;

	bool destructionIsDone() const;

};

typedef boost::shared_ptr<BStateMachine> BStateMachinePtr;

} // end namespace evf

#endif /* EVFSTATEMACHINE_H_ */

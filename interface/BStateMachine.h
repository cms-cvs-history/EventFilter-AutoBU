/*
 * BStateMachine.h
 *
 * AutoBU State Chart
 *
 *  Created on: Sep 21, 2011
 *      Author: aspataru - andrei.cristian.spataru@cern.ch
 */

#ifndef EVFBOOSTSTATEMACHINE_H_
#define EVFBOOSTSTATEMACHINE_H_

#include "EventFilter/Utilities/interface/Exception.h"
#include "EventFilter/AutoBU/interface/ABUEvent.h"
#include "xdaq2rc/RcmsStateNotifier.h"
#include "xdata/String.h"
#include "xdata/Bag.h"
#include "xdaq/Application.h"

#include <boost/statechart/event.hpp>
#include <boost/statechart/in_state_reaction.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/transition.hpp>
#include <boost/mpl/list.hpp>
#include <boost/shared_ptr.hpp>

#include <iostream>
#include <string>
#include <vector>

using std::string;
using std::cout;
using std::endl;

namespace bsc = boost::statechart;

namespace evf {

class SharedResources;
typedef boost::shared_ptr<SharedResources> SharedResourcesPtr;

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
	/*
	 * DEFAULT implementations of state-dependent actions
	 */
	virtual void do_stateAction() const {
		cout << "AutoBU state machine: no >>STATE ACTION<< defined for state: "
				<< stateName() << endl;
	}
	virtual bool execute() const {
		cout << "BStateMachine: current state: " << stateName()
				<< " does not support action: >>execute<<" << endl;
		::sleep(1);
		return true;
	}

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

	BStateMachine(xdaq::Application* app, SharedResourcesPtr sr);
	~BStateMachine();

	std::string getCurrentStateName() const;
	BaseState const& getCurrentState() const;

	SharedResourcesPtr getSharedResources() const {
		return sharedResources_;
	}

	void setExternallyVisibleState(const std::string& s);
	void setInternalStateName(const std::string& s);

	inline string getExternallyVisibleState() {
		return visibleStateName_.value_;
	}
	inline xdata::String* getExternallyVisibleStatePtr() {
		return &visibleStateName_;
	}
	inline bool firstTimeInHalted() const {
		return firstTimeInHalted_;
	}
	inline void setFirstTimeInHaltedFalse() {
		firstTimeInHalted_ = false;
	}

	// get the RCMS StateListener parameter (classname/instance)
	xdata::Bag<xdaq2rc::ClassnameAndInstance>* rcmsStateListener();
	// report if RCMS StateListener was found
	xdata::Boolean* foundRcmsStateListener();
	void findRcmsStateListener(xdaq::Application* app);
	void rcmsStateChangeNotify(string state);

private:
	void updateWebGUIExternalState(string newStateName) const;
	void updateWebGUIInternalState(string newStateName) const;

private:

	SharedResourcesPtr sharedResources_;
	xdaq2rc::RcmsStateNotifier rcmsStateNotifier_;
	xdata::String visibleStateName_;
	string internalStateName_;
	bool firstTimeInHalted_;

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

	// state-dependent actions
	// the execute workloop will be stopped in this state
	virtual bool execute() const {
		return false;
	}

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

	// state-dependent actions
	virtual void do_stateAction() const;

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

	// state-dependent actions
	// the execute workloop will be stopped in this state
	virtual bool execute() const {
		return false;
	}

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

	// state-dependent actions
	virtual void do_stateAction() const;

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

class Executing: public bsc::state<Executing, Enabled>, public BaseState {

public:

	Executing( my_context);
	virtual ~Executing();

	// state-dependent actions
	virtual void do_stateAction() const;
	virtual bool execute() const;

private:

	virtual std::string do_stateName() const;
	virtual void do_entryActionWork();
	virtual void do_exitActionWork();
	bool generateEvent(ABUEvent* evt) const;
	/// Creates a message chain for the Resource Broker
	toolbox::mem::Reference *createMsgChain(evf::ABUEvent *evt,
			unsigned int fuResourceId) const;
	/// Updates the maximum FED size generated
	virtual void do_moveToFailedState(xcept::Exception& exception) const;

private:
	SharedResourcesPtr resources;
	double gaussianMean_;
	double gaussianWidth_;

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

	// state-dependent actions
	virtual void do_stateAction() const;
	// this will stop execution of the workloop once the state is reached
	virtual bool execute() const {
		return false;
	}

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

	// state-dependent actions
	virtual void do_stateAction() const;
	// this will stop execution of the workloop once the state is reached
	virtual bool execute() const {
		return false;
	}

private:

	virtual std::string do_stateName() const;
	virtual void do_entryActionWork();
	virtual void do_exitActionWork();
	virtual void do_moveToFailedState(xcept::Exception& exception) const;

	bool destructionIsDone() const;

};

typedef boost::shared_ptr<BStateMachine> BStateMachinePtr;

} // end namespace evf

#endif /* EVFBOOSTSTATEMACHINE_H_ */

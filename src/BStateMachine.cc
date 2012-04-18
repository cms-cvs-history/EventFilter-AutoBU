/** \class BStateMachine
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"
#include "EventFilter/AutoBU/interface/SharedResources.h"

#include <typeinfo>
#include <fstream>

using namespace evf::autobu_statemachine;

BStateMachine::BStateMachine(xdaq::Application* app, SharedResourcesPtr sr) :
			sharedResources_(sr),
			rcmsStateNotifier_(app->getApplicationLogger(),
					app->getApplicationDescriptor(),
					app->getApplicationContext()), visibleStateName_("N/A"),
			internalStateName_("N/A"), firstTimeInHalted_(true) {

	// pass pointer of FSM to shared resources
	sharedResources_->setFsmPointer(this);
}

BStateMachine::~BStateMachine() {
	std::cout << "State machine DESTROYED!" << std::endl;
}

BaseState const& BStateMachine::getCurrentState() const {
	return state_cast<BaseState const&> ();
}

std::string BStateMachine::getCurrentStateName() const {
	// throws bad_cast if state transition was not completed
	try {
		return getCurrentState().stateName();
	} catch (std::bad_cast) {
		return "CURRENT STATE NAME NOT AVAILABLE!!!";
	}
}

void BStateMachine::setExternallyVisibleState(const std::string& s) {
	visibleStateName_ = s;
	updateWebGUIExternalState(visibleStateName_);
}

void BStateMachine::setInternalStateName(const std::string& s) {
	internalStateName_ = s;
	updateWebGUIInternalState(internalStateName_);
}

// get the RCMS StateListener parameter (classname/instance)
xdata::Bag<xdaq2rc::ClassnameAndInstance>* BStateMachine::rcmsStateListener() {
	return rcmsStateNotifier_.getRcmsStateListenerParameter();
}

// report if RCMS StateListener was found
xdata::Boolean* BStateMachine::foundRcmsStateListener() {
	return rcmsStateNotifier_.getFoundRcmsStateListenerParameter();
}

void BStateMachine::findRcmsStateListener(xdaq::Application* app) {
	rcmsStateNotifier_.findRcmsStateListener(); //might not be needed
	rcmsStateNotifier_.subscribeToChangesInRcmsStateListener(
			app->getApplicationInfoSpace());
}

void BStateMachine::rcmsStateChangeNotify(string state) {
	try {
		std::cout << "-->RCMS state change notify: " << state << std::endl;
		rcmsStateNotifier_.stateChanged(state,
				"AutoBU has reached target state " + state);
	} catch (xcept::Exception&) {
		std::cout << "Failed to notify state change: " << state << std::endl;
	}
}

void BStateMachine::updateWebGUIExternalState(string newStateName) const {
	sharedResources_->updateGUIExternalState(newStateName);
}

void BStateMachine::updateWebGUIInternalState(string newStateName) const {
	sharedResources_->updateGUIInternalState(newStateName);
}

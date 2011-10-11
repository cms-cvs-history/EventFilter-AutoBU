/** \class BStateMachine
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"

#include <typeinfo>
#include <fstream>

using namespace evf;

BStateMachine::BStateMachine(SharedResourcesPtr sr) :
	sharedResources_(sr) {
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
		return "in_transition";
	}
}

void BStateMachine::setExternallyVisibleState(const std::string& s) {

}

/** \class BaseState
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"
#include "EventFilter/AutoBU/interface/SharedResources.h"
#include "EventFilter/Utilities/interface/Exception.h"

#include <boost/statechart/event_base.hpp>
#include "interface/evb/i2oEVBMsgs.h"
#include "interface/shared/i2oXFunctionCodes.h"
#include "i2o/Method.h"
#include "i2o/utils/AddressMap.h"
#include "xcept/tools.h"

#include <iostream>

using std::string;
using namespace boost::statechart;
using namespace evf::autobu_statemachine;

BaseState::BaseState() {
}

BaseState::~BaseState() {
}

std::string BaseState::stateName() const {
	return do_stateName();
}

void BaseState::moveToFailedState(xcept::Exception& exception) const {
	do_moveToFailedState(exception);
}

////////////////////////////////////////////////////////////
// Default implementation for (some) virtual functions.
////////////////////////////////////////////////////////////

void BaseState::safeEntryAction() {

	const std::string unknown = "unknown exception";
	std::string msg = "Error going into " + stateName() + " state: ";
	std::string enter = "NEW STATE -> " + stateName();
	try {
		std::cout << enter << std::endl;
		do_entryActionWork();
	} catch (xcept::Exception& e) {
		XCEPT_RETHROW(evf::Exception, msg, e);
	}
}

void BaseState::safeExitAction() {

	const std::string unknown = "unknown exception";
	std::string msg = "Error leaving " + stateName() + " state: ";
	std::string exit = "EXITING STATE -> " + stateName();
	try {
		std::cout << exit << std::endl;
		do_exitActionWork();
	} catch (xcept::Exception& e) {
		XCEPT_RETHROW(evf::Exception, msg, e);
	}
}


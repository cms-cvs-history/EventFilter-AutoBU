/** \class Failed
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"

#include <iostream>

using std::string;
using namespace evf::autobu_statemachine;

void Failed::do_entryActionWork() {
	outermost_context().setExternallyVisibleState(stateName());
	outermost_context().setInternalStateName(stateName());
	outermost_context().rcmsStateChangeNotify(stateName());
}

Failed::Failed(my_context c) :
	my_base(c) {
	safeEntryAction();
}

void Failed::do_exitActionWork() {

}

Failed::~Failed() {
	safeExitAction();
}

string Failed::do_stateName() const {
	return std::string("Failed");
}

void Failed::do_moveToFailedState(xcept::Exception& exception) const {
	// nothing can be done here
}

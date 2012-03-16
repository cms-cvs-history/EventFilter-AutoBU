/** \class Halted
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"

#include <iostream>

using std::string;
using namespace evf;

void Halted::do_entryActionWork() {
	outermost_context().setExternallyVisibleState(stateName());
	outermost_context().setInternalStateName(stateName());
	if (!outermost_context().firstTimeInHalted())
		outermost_context().rcmsStateChangeNotify(stateName());
}

Halted::Halted(my_context c) :
	my_base(c) {
	safeEntryAction();
}

void Halted::do_exitActionWork() {
	outermost_context().setFirstTimeInHaltedFalse();
}

Halted::~Halted() {
	safeExitAction();
}

string Halted::do_stateName() const {
	return std::string("Halted");
}

void Halted::do_moveToFailedState(xcept::Exception& exception) const {
}

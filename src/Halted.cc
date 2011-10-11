/** \class Halted
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"

#include <iostream>

using std::string;
using namespace evf;

void Halted::do_entryActionWork() {
}

Halted::Halted(my_context c) :
	my_base(c) {
	safeEntryAction();
}

void Halted::do_exitActionWork() {
}

Halted::~Halted() {
	safeExitAction();
}

string Halted::do_stateName() const {
	return std::string("Halted");
}

void Halted::do_moveToFailedState(xcept::Exception& exception) const {
}

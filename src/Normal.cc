/** \class Normal
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"

#include <iostream>

using std::string;
using namespace evf::autobu_statemachine;

void Normal::do_entryActionWork() {
}

Normal::Normal(my_context c) :
	my_base(c) {
	safeEntryAction();
}

void Normal::do_exitActionWork() {
}

Normal::~Normal() {
	safeExitAction();
}

string Normal::do_stateName() const {
	return std::string("Normal");
}

void Normal::do_moveToFailedState(xcept::Exception& exception) const {
}

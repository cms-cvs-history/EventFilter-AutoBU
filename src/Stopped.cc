/** \class Stopped
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"

#include <iostream>

using std::string;
using namespace evf;

Stopped::Stopped(my_context c) :
	my_base(c) {
	safeEntryAction();
}

void Stopped::do_entryActionWork() {
}

Stopped::~Stopped() {
	safeExitAction();
}

void Stopped::do_exitActionWork() {
}

string Stopped::do_stateName() const {
	return std::string("Stopped");
}

void Stopped::do_moveToFailedState(xcept::Exception& exception) const {
}

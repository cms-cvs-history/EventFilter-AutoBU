/** \class Enabled
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/SharedResources.h"
#include "EventFilter/AutoBU/interface/BStateMachine.h"

#include <iostream>

using std::string;
using namespace evf;

Enabled::Enabled(my_context c) :
	my_base(c) {
	safeEntryAction();
}

void Enabled::do_entryActionWork() {

}

Enabled::~Enabled() {
	safeExitAction();
}

void Enabled::do_exitActionWork() {
}

string Enabled::do_stateName() const {
	return std::string("Enabled");
}

void Enabled::do_moveToFailedState(xcept::Exception& exception) const {
}

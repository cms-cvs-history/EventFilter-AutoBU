/** \class Ready
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/SharedResources.h"
#include "EventFilter/AutoBU/interface/BStateMachine.h"

using std::string;
using namespace evf::autobu_statemachine;

Ready::Ready(my_context c) :
	my_base(c) {
	safeEntryAction();
}

void Ready::do_entryActionWork() {
	// notifications handled in sub-state Stopped
	/*
	outermost_context().setExternallyVisibleState(stateName());
	outermost_context().rcmsStateChangeNotify(stateName());
	*/
}

Ready::~Ready() {
	safeExitAction();
}

void Ready::do_exitActionWork() {

}

string Ready::do_stateName() const {
	return std::string("Ready");
}

void Ready::do_moveToFailedState(xcept::Exception& exception) const {
}
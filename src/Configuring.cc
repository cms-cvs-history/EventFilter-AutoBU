/** \class Configuring
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"
#include "EventFilter/AutoBU/interface/SharedResources.h"
#include <iostream>

using namespace evf::autobu_statemachine;
using std::string;

void Configuring::do_entryActionWork() {
	outermost_context().setExternallyVisibleState(stateName());
	outermost_context().setInternalStateName(stateName());
	outermost_context().rcmsStateChangeNotify(stateName());
}

Configuring::Configuring(my_context c) :
	my_base(c) {
	safeEntryAction();
}

// state-dependent actions
void Configuring::do_stateAction() const {
	/*
	 *  CONFIGURING
	 */
	SharedResourcesPtr resources = outermost_context().getSharedResources();

	try {
		LOG4CPLUS_INFO(resources->log_, "Start configuring ...");

		resources->initSemaphores();

		LOG4CPLUS_INFO(resources->log_, "Finished configuring!");

		EventPtr stMachEvent(new ConfigureDone());
		resources->enqEvent(stMachEvent);
	}

	catch (xcept::Exception &e) {
		string msg = "configuring FAILED: " + (string) e.what();
		EventPtr stMachEvent(new Fail());
		resources->enqEvent(stMachEvent);
	}
}

void Configuring::do_exitActionWork() {
}

Configuring::~Configuring() {
	safeExitAction();
}

string Configuring::do_stateName() const {
	return std::string("Configuring");
}

void Configuring::do_moveToFailedState(xcept::Exception& exception) const {

}

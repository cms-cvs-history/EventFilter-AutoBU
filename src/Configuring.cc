/** \class Configuring
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"

#include <iostream>

using namespace evf;
using std::string;

//#define FORCE_I2O_BU

void Configuring::do_entryActionWork() {
	//outermost_context().setExternallyVisibleState("Configuring");

	/*
	 *  CONFIGURING
	 */

	SharedResourcesPtr resources = outermost_context().getSharedResources();

	try {
		LOG4CPLUS_INFO(resources->logger(), "Start configuring ...");

		// BUFU
		resources->setInterface(BUFUInterface::instance());
#ifdef FORCE_I2O_BU
		resources->setInterface(BUFUInterface::forceNewInstance());
#endif
		resources->interface()->registerBU(resources->bu(), resources->logger());

		resources->reset();
		LOG4CPLUS_INFO(resources->logger(), "Finished configuring!");

		EventPtr stMachEvent(new ConfigureDone());
		resources->enqEvent(stMachEvent);
	}

	catch (xcept::Exception &e) {
		string msg = "configuring FAILED: " + (string) e.what();
		EventPtr stMachEvent(new Fail());
		resources->enqEvent(stMachEvent);
	}
}

Configuring::Configuring(my_context c) :
	my_base(c) {
	safeEntryAction();
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

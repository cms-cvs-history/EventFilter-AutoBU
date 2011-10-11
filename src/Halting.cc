/** \class Halting
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"

#include <iostream>

using std::string;
using namespace evf;

void Halting::do_entryActionWork() {
	SharedResourcesPtr resources = outermost_context().getSharedResources();

	// HALTING
	try {
		LOG4CPLUS_INFO(resources->logger(), "Start halting ...");

		// clear task queue
		while (resources->taskQueue()->size() > 0)
			resources->taskQueue()->pop();
		resources->taskQueueSize() = resources->taskQueue()->size();

		// clear ReadyToSend Id's
		while (resources->readyIds()->size() > 0)
			resources->readyIds()->pop();
		resources->rtsQSize() = resources->readyIds()->size();

		// reset average queue size counter
		resources->avgTaskQueueSize() = 0;

		// clear maxFedSize counter
		resources->maxFedSizeGen() = 0;

		LOG4CPLUS_INFO(resources->logger(), "Finished halting!");

		EventPtr stMachEvent(new HaltDone());
		resources->enqEvent(stMachEvent);

	} catch (xcept::Exception &e) {
		string msg = "halting FAILED: " + (string) e.what();
		EventPtr stMachEvent(new Fail());
		resources->enqEvent(stMachEvent);
	}
}

Halting::Halting(my_context c) :
	my_base(c) {
	safeEntryAction();
}

void Halting::do_exitActionWork() {
}

Halting::~Halting() {
	safeExitAction();
}

string Halting::do_stateName() const {
	return std::string("Halting");
}

void Halting::do_moveToFailedState(xcept::Exception& exception) const {
}

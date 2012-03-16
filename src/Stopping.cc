/** \class Stopping
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"
#include "EventFilter/AutoBU/interface/SharedResources.h"

#include <iostream>

using std::endl;
using namespace evf;

Stopping::Stopping(my_context c) :
	my_base(c) {
	safeEntryAction();
}

void Stopping::do_entryActionWork() {
	outermost_context().setExternallyVisibleState(stateName());
	outermost_context().setInternalStateName(stateName());
	outermost_context().rcmsStateChangeNotify(stateName());
}

void Stopping::do_stateAction() const {
	SharedResourcesPtr resources = outermost_context().getSharedResources();

	//resources->stopExecution() = true;

	// STOPPING
	try {
		LOG4CPLUS_INFO(resources->logger(), "Start stopping :) ...");

		if (0 != PlaybackRawDataProvider::instance()
				&& (!resources->replay().value_ || resources->nbEventsBuilt()
						< (uint32_t) resources->events()->size())) {

			// let the playback go to the last event and exit
			LOG4CPLUS_INFO(resources->logger(), "Playback going to last event!");
			PlaybackRawDataProvider::instance()->setFreeToEof();
		}

		// set restarted flag
		resources->restarted() = true;

		LOG4CPLUS_INFO(resources->logger(), "Finished stopping!");

		EventPtr stMachEvent(new StopDone());
		resources->enqEvent(stMachEvent);

	} catch (xcept::Exception &e) {
		string msg = "stopping FAILED: " + (string) e.what();
		EventPtr stMachEvent(new Fail());
		resources->enqEvent(stMachEvent);
	}
}

Stopping::~Stopping() {
	safeExitAction();
}

void Stopping::do_exitActionWork() {

}

string Stopping::do_stateName() const {
	return std::string("Stopping");
}

void Stopping::do_moveToFailedState(xcept::Exception& exception) const {
}

/** \class Stopping
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"
#include "EventFilter/AutoBU/interface/SharedResources.h"

#include <iostream>

using std::endl;
using namespace evf::autobu_statemachine;

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

	// STOPPING
	try {
		LOG4CPLUS_INFO(resources->log_, "Start stopping :) ...");

		if (0 != PlaybackRawDataProvider::instance()
				&& (!resources->replay_.value_ || resources->nbEventsBuilt_
						< (uint32_t) resources->events_.size())) {

			// let the playback go to the last event and exit
			LOG4CPLUS_INFO(resources->log_, "Playback going to last event!");
			PlaybackRawDataProvider::instance()->setFreeToEof();
		}

		// clear task queue
		while (resources->taskQueue_.size() > 0)
			resources->taskQueue_.pop();
		resources->taskQueueSize_ = resources->taskQueue_.size();

		// clear ReadyToSend Id's
		while (resources->readyIds_.size() > 0)
			resources->readyIds_.pop();
		resources->readyToSendQueueSize_ = resources->readyIds_.size();

		// reset average queue size counter
		resources->avgTaskQueueSize_ = 0;

		resources->emptyQueues();
		resources->resetCounters();

		//todo remove?
		resources->initSemaphores();

		LOG4CPLUS_INFO(resources->log_, "Finished stopping!");

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

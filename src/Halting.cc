/** \class Halting
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BStateMachine.h"
#include "EventFilter/AutoBU/interface/SharedResources.h"

#include <iostream>

using std::string;
using namespace evf::autobu_statemachine;

void Halting::do_entryActionWork() {
	outermost_context().setExternallyVisibleState(stateName());
	outermost_context().setInternalStateName(stateName());
	outermost_context().rcmsStateChangeNotify(stateName());
}

Halting::Halting(my_context c) :
	my_base(c) {
	safeEntryAction();
}

void Halting::do_stateAction() const {
	SharedResourcesPtr resources = outermost_context().getSharedResources();

	// HALTING
	try {
		LOG4CPLUS_INFO(resources->log_, "Start halting ...");

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

		LOG4CPLUS_INFO(resources->log_, "Finished halting!");

		EventPtr stMachEvent(new HaltDone());
		resources->enqEvent(stMachEvent);

	} catch (xcept::Exception &e) {
		string msg = "halting FAILED: " + (string) e.what();
		EventPtr stMachEvent(new Fail());
		resources->enqEvent(stMachEvent);
	}
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

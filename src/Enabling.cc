/** \class Enabling
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/SharedResources.h"
#include "EventFilter/AutoBU/interface/BStateMachine.h"
#include "DataFormats/FEDRawData/interface/FEDNumbering.h"

#include <iostream>

using std::string;
using namespace evf::autobu_statemachine;

Enabling::Enabling(my_context c) :
	my_base(c) {
	safeEntryAction();
}

void Enabling::do_entryActionWork() {
	outermost_context().setExternallyVisibleState(stateName());
	outermost_context().setInternalStateName(stateName());
	outermost_context().rcmsStateChangeNotify(stateName());
}

Enabling::~Enabling() {
	safeExitAction();
}

void Enabling::do_stateAction() const {
	SharedResourcesPtr resources = outermost_context().getSharedResources();

	// from configuring
	resources->populateEvents();

	// set stopExecution flag to false
	resources->stopExecution_ = false;

	try {
		// playback case
		if (0 != PlaybackRawDataProvider::instance()) {
			for (unsigned int i = 0; i < (unsigned int) FEDNumbering::MAXFEDID
					+ 1; i++)
				if (FEDNumbering::inRange(i))
					resources->validFedIds_.push_back(i);

			// random case
		} else {
			// put valid fed id's in
			for (unsigned int i = 0; i < (unsigned int) FEDNumbering::MAXFEDID
					+ 1; i++)
				if (FEDNumbering::inRangeNoGT(i))
					resources->validFedIds_.push_back(i);

		}

		// put initial build requests in
		for (unsigned int i = 0; i < resources->queueSize_; i++) {
			resources->pushTask('b');
		}

		// ENABLING
		// start the main (execution) workloop
		resources->startExecutionWorkLoop();

		EventPtr stMachEvent(new EnableDone());
		resources->enqEvent(stMachEvent);

	} catch (xcept::Exception &e) {
		string msg = "enabling FAILED: " + (string) e.what();
		EventPtr stMachEvent(new Fail());
		resources->enqEvent(stMachEvent);
	}
}

void Enabling::do_exitActionWork() {

}

string Enabling::do_stateName() const {
	return std::string("Enabling");
}

void Enabling::do_moveToFailedState(xcept::Exception& exception) const {
}

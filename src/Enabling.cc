/** \class Enabling
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/SharedResources.h"
#include "EventFilter/AutoBU/interface/BStateMachine.h"
#include "DataFormats/FEDRawData/interface/FEDNumbering.h"

#include <iostream>

using std::string;
using namespace evf;

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

	// set stopExecution flag to false
	resources->stopExecution() = false;

	try {
		// playback case

		/*
		 * for PLAYBACK mode, keeping only valid id's returned by
		 * FEDNumbering::inRangeNoGT makes file processing faster
		 */
		//if (/*0 != PlaybackRawDataProvider::instance()*/ ) {
		if (false) {
			if (!resources->restarted()) {
				for (unsigned int i = 0; i
						< (unsigned int) FEDNumbering::MAXFEDID + 1; i++)
					if (FEDNumbering::inRange(i))
						resources->validFedIds()->push_back(i);
			}
			// random case
		} else {
			// put valid fed id's in
			if (!resources->restarted()) {
				for (unsigned int i = 0; i
						< (unsigned int) FEDNumbering::MAXFEDID + 1; i++)
					if (FEDNumbering::inRangeNoGT(i))
						resources->validFedIds()->push_back(i);
			}
		}

		// put initial build requests in
		// if entering state for the first time
		if (!resources->restarted()) {
			for (unsigned int i = 0; i < resources->queueSize(); i++) {
				resources->pushTask('b');
			}
		}

		// ENABLING
		// start the main (execution) workloop
		resources->startExecutionWorkLoop();

		EventPtr stMachEvent(new EnableDone());
		resources->enqEvent(stMachEvent);

	} catch (xcept::Exception &e) {
		string msg = "enabling FAILED: " + (string) e.what();
		//fsm_.fireFailed(msg, this);
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

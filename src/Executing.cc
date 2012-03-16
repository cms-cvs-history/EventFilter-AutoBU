/** \class Executing
 *
 *  \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/BU.h"
#include "EventFilter/AutoBU/interface/BStateMachine.h"
#include "EventFilter/AutoBU/interface/SharedResources.h"
#include "EventFilter/AutoBU/interface/FrameSizeUtils.h"
#include "DataFormats/FEDRawData/interface/FEDRawDataCollection.h"
#include "EventFilter/AutoBU/interface/BUMessageCutter.h"
#include "CLHEP/Random/RandGauss.h"

#include <iostream>

using std::string;
using std::cout;
using std::endl;
using namespace evf;

//#define DEBUG_BU_1
//#define DEBUG_BU_2
//#define DEBUG_SF_EXPANSION

//______________________________________________________________________________

void Executing::do_entryActionWork() {
	resources = outermost_context().getSharedResources();

	gaussianMean_ = std::log((double) resources->fedSizeMean());
	gaussianWidth_ = std::sqrt(
			std::log(
					0.5 * (1 + std::sqrt(
							1.0 + 4.0 * resources->fedSizeWidth().value_
									* resources->fedSizeWidth().value_
									/ resources->fedSizeMean().value_
									/ resources->fedSizeMean().value_))));
	resources->buResIdInUse_ = 0;

	outermost_context().setExternallyVisibleState("Enabled");
	outermost_context().setInternalStateName(stateName());
	outermost_context().rcmsStateChangeNotify("Enabled");
}

//______________________________________________________________________________

void Executing::do_stateAction() const {
	/*

	 */
}

//______________________________________________________________________________

bool Executing::execute() const {

	if (resources->stopExecution()) {
		LOG4CPLUS_INFO(resources->logger(), "Stopping 'execute' workloop!");
		return false;
	}

	//  TASK
	resources->qLock();

	// pop a task if queue not empty
	if (resources->taskQueue()->size() != 0) {

		BUTask current = resources->taskQueue()->front();
		resources->taskQueue()->pop();
		resources->decreaseTaskQueueSize();
		// task queue avg
		resources->registerSize();

		resources->qUnlock();

		unsigned int buResourceId = 0;
		char currentType = current.type();

		switch (currentType) {

		//BUILDING
		case 'b': {
			//std::cout << "exec Build " << current.id() << std::endl;
			resources->lock();
			buResourceId = resources->freeIds()->front();
			resources->freeIds()->pop();
			resources->unlock();

			if (buResourceId >= (uint32_t) resources->events()->size()) {
				LOG4CPLUS_INFO(resources->logger(),
						"shutdown 'executing' workloop.");
				std::cout << "b: buResourceId = " << buResourceId
						<< " generated events = "
						<< resources->events()->size() << std::endl;
				return false;
			}

			ABUEvent* evt = resources->eventAt(buResourceId);
			resources->buResIdInUse_ = buResourceId;
			if (generateEvent(evt)) {
				resources->lock();
				resources->increaseEventsBuilt();
				resources->builtIds()->push(buResourceId);
				resources->unlock();

				// TASK
				resources->pushTask('s');

			} else {
				LOG4CPLUS_INFO(resources->logger(),
						"building:received null post -- input file closed?");
				resources->lock();
				unsigned int saveBUResourceId = buResourceId;
				//buResourceId = freeIds_.front(); freeIds_.pop();
				resources->freeIds()->push(saveBUResourceId);
				resources->unlock();
				//return false;
				break;
			}
			break;
		}
			//SENDING - part 1 (send)
		case 's': {
			//std::cout << "exec Send " << current.id() << std::endl;
			resources->lock();
			buResourceId = resources->builtIds()->front();

			resources->builtIds()->pop();
			resources->unlock();

			if (buResourceId >= (uint32_t) resources->events()->size()) {
				LOG4CPLUS_INFO(resources->logger(),
						"shutdown 'executing' workloop.");
				std::cout << "s: buResourceId = " << buResourceId
						<< " generated events = "
						<< resources->events()->size() << std::endl;
				return false;
			}

			// add to queue of ready Ids
			resources->readyIds()->push(buResourceId);
			resources->increaseRTSQS();
			break;
		}
			// SENDING - part 2 (request)
		case 'r':
			//std::cout << "exec Request " << current.id() << std::endl;
			// if there are ready id's
			if (resources->readyIds()->size() > 0) {
				resources->lock();
				unsigned int fuResourceId = resources->rqstIds()->front();
				resources->rqstIds()->pop();
				resources->unlock();
				buResourceId = resources->readyIds()->front();
				resources->readyIds()->pop();
				resources->decreaseRTSQS();

				ABUEvent* evt = resources->eventAt(buResourceId);
				toolbox::mem::Reference* msg =
						createMsgChain(evt, fuResourceId);

				resources->lock();
				resources->decreaseEventsInBU();
				resources->increaseEventsSent();
				resources->sentIds()->insert(buResourceId);
				resources->unlock();
#ifdef DEBUG_BU_1
				std::cout << "posting frame for resource id " << buResourceId
				<< std::endl;
#endif

				resources->bu()->postI2OFrame(resources->fuAppDesc(), msg);

#ifdef DEBUG_BU_1
				std::cout << "posted frame for resource id " << buResourceId
				<< std::endl;
#endif
			}
			// no ready id's, re-insert this request into the queue
			else {
				resources->pushTask('r');
			}
			break;
		} // end switch
	}

	else {
		resources->qUnlock();
		::usleep(10000);
	}

	return true;
}

//______________________________________________________________________________

Executing::Executing(my_context c) :
	my_base(c) {
	safeEntryAction();
}

//______________________________________________________________________________

void Executing::do_exitActionWork() {
	SharedResourcesPtr resources = outermost_context().getSharedResources();
	resources->stopExecution() = true;
	::usleep(10000);
}

//______________________________________________________________________________

Executing::~Executing() {
	safeExitAction();
}

//______________________________________________________________________________

string Executing::do_stateName() const {
	return std::string("Executing");
}

//______________________________________________________________________________

bool Executing::generateEvent(ABUEvent* evt) const {

	//SharedResourcesPtr resources = outermost_context().getSharedResources();

	// replay?
	if (resources->replay().value_ && resources->nbEventsBuilt()
			>= (uint32_t) resources->events()->size()) {
		PlaybackRawDataProvider::instance()->setFreeToEof();
		return true;
	}

	unsigned int evtNumber = (resources->firstEvent()
			+ resources->eventNumber()++) % 0x1000000;

	// if event is not empty, memory is already allocated
	if (evt->isInitialised()) {
#ifdef DEBUG_BU_2
		std::cout << "event is initialized, resetting with: " << evtNumber
		<< std::endl;
#endif
		if (evt->usedSf() > 0) {

			evt->reset(evtNumber);
		}
		// event is empty, need space for superfragments
	} else {
#ifdef DEBUG_BU_2
		std::cout << "event is NOT initialized, starting it with: "
		<< evtNumber << std::endl;
#endif
		evt->initialise(evtNumber);
	}

	unsigned int currentFedIndex = 0;
	unsigned int iSf = 0;
	FEDRawDataCollection* event = 0;
	unsigned int fedId;
	unsigned int fedSize;
	unsigned char* fedAddr;
	unsigned int runNumber, evtNoFromFile;

	//PLAYBACK

	if (0 != PlaybackRawDataProvider::instance()) {

		/*
		 * FIX: Modified PlaybackRawDataProvider to return a 0 pointer
		 * if the input file was already closed.
		 */
		event = PlaybackRawDataProvider::instance()->getFEDRawData(runNumber,
				evtNoFromFile);

		if (event == 0)
			return false;
	}

	while (currentFedIndex < resources->validFedIds()->size()) {

		while (iSf < evt->getCapacity()) {

			// get a fragment
			SuperFragment* sf = evt->sfAt(iSf);
			sf->use();

			bool pushedOK;

			// FIXED SIZE
			if ("FIXED" == resources->mode().toString()) {
				pushedOK = sf->pushFrame(evt->getEvtNumber(),
						resources->validFedIdAt(currentFedIndex),
						MIN_WORDS_IN_FRAME, INITIAL_CRC);
			}

			// RANDOM-SIMPLE
			else if ("RANDOM-SIMPLE" == resources->mode().toString()) {
				unsigned int maxSizeForRandomSimple = resources->fedSizeMax()
						/ 16;
				maxSizeForRandomSimple /= BYTES_IN_WORD;
				unsigned int randSize = FrameSizeUtils::boundedRandom(
						maxSizeForRandomSimple);

				pushedOK = sf->pushFrame(evt->getEvtNumber(),
						resources->validFedIdAt(currentFedIndex), randSize,
						INITIAL_CRC);
			}

			// A LA EMILIO
			else if ("REALISTIC" == resources->mode().toString()) {
				unsigned int fedSizeMin = MIN_WORDS_IN_FRAME * BYTES_IN_WORD;
				unsigned int fedSize = resources->fedSizeMean().value_;
				double logFedSize = CLHEP::RandGauss::shoot(gaussianMean_,
						gaussianWidth_);
				fedSize = (unsigned int) (std::exp(logFedSize));
				if (fedSize < fedSizeMin)
					fedSize = fedSizeMin;
				if (fedSize > resources->fedSizeMax().value_)
					fedSize = resources->fedSizeMax().value_;
				fedSize -= fedSize % 8;

				// FED Size in WORDS... pushFrame takes the size in WORDS
				fedSize /= BYTES_IN_WORD;

				pushedOK = sf->pushFrame(evt->getEvtNumber(),
						resources->validFedIdAt(currentFedIndex), fedSize,
						INITIAL_CRC);
			}

			// PLAYBACK
			else if ("PLAYBACK" == resources->mode().toString() && 0
					!= PlaybackRawDataProvider::instance()) {

				fedId = resources->validFedIdAt(currentFedIndex);
				fedSize = event->FEDData(fedId).size();

				if (fedSize > 0) {
					fedAddr = event->FEDData(fedId).data();
					if (resources->overwriteEvtId().value_ && fedAddr != 0) {
						FEDHeader initH(fedAddr);
						//FEDHeader::set(fedAddr, initH.triggerType(), evtNumber,
						FEDHeader::set(fedAddr, initH.triggerType(),
								evt->getEvtNumber(), initH.bxID(),
								initH.sourceID(), initH.version(),
								initH.moreHeaders());
					}

					pushedOK = sf->pushFrame(fedAddr, fedSize);
				} else {
					// no expansion needed in this case
					// just skip
					pushedOK = true;
				}

			}
			// mode not recognised
			else {
				LOG4CPLUS_ERROR(
						resources->logger(),
						"FED Frame generation mode: "
								<< resources->mode().toString()
								<< " NOT RECOGNIZED!");
				return false;
			}

			if (!pushedOK) {
				// expand super fragment
				SuperFragment* expanded = FrameSizeUtils::expandSuperFragment(
						sf);
				evt->replaceSuperFragment(iSf, expanded);
				sf = evt->sfAt(iSf);
				sf->use();
#ifdef DEBUG_SF_EXPANSION
				std::cout << "Expanding: (Ev.SF) " << resources->buResIdInUse_
				<< "." << iSf << " to new size "
				<< evt->sfAt(iSf)->getTrueSize() << std::endl;
#endif
			}

			else {
				currentFedIndex++;
				iSf++;
			}

			if (currentFedIndex == resources->validFedIds()->size())
				break;
		}
		// have we finished id's?
		if (currentFedIndex == resources->validFedIds()->size()) {
			break;
		} else {
			iSf = 0;
		}
	}

	if (0 != PlaybackRawDataProvider::instance()) {
		delete event;
	}

	return true;
}

//______________________________________________________________________________

toolbox::mem::Reference *Executing::createMsgChain(ABUEvent* evt,
		unsigned int fuResourceId) const {

	//SharedResourcesPtr resources = outermost_context().getSharedResources();

	// initialize message cutter
	BUMessageCutter mCutter(resources->msgBufferSize(), resources->logger(),
			resources->buAppDesc(), resources->fuAppDesc(),
			resources->i2oPool());
	// call to createMSgChain
	if ("SIMPLE" == resources->msgChainCreationMode().toString()) {
		return mCutter.createMessageChainSimple(evt, fuResourceId);
	}
	return mCutter.createMessageChain(evt, fuResourceId);

}

//______________________________________________________________________________

void Executing::do_moveToFailedState(xcept::Exception& exception) const {
	//outermost_context().getSharedResources()->moveToFailedState(exception);
}

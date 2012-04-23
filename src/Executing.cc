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
using namespace evf::autobu_statemachine;

//#define DEBUG_BU_1
//#define DEBUG_BU_2
//#define DEBUG_SF_EXPANSION

//______________________________________________________________________________

void Executing::do_entryActionWork() {
	resources = outermost_context().getSharedResources();

	gaussianMean_ = std::log((double) resources->fedSizeMean_);
	gaussianWidth_ = std::sqrt(
			std::log(
					0.5 * (1 + std::sqrt(
							1.0 + 4.0 * resources->fedSizeWidth_.value_
									* resources->fedSizeWidth_.value_
									/ resources->fedSizeMean_.value_
									/ resources->fedSizeMean_.value_))));
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

	if (resources->stopExecution_) {
		LOG4CPLUS_INFO(resources->log_, "Stopping 'execute' workloop!");
		return false;
	}

	//  TASK
	resources->qLock();

	// pop a task if queue not empty
	if (resources->taskQueue_.size() != 0) {

		BUTask current = resources->taskQueue_.front();
		resources->taskQueue_.pop();
		resources->taskQueueSize_--;
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
			buResourceId = resources->freeIds_.front();
			resources->freeIds_.pop();
			resources->unlock();

			if (buResourceId >= (uint32_t) resources->events_.size()) {
				LOG4CPLUS_INFO(resources->log_,
						"shutdown 'executing' workloop.");
				std::cout << "b: buResourceId = " << buResourceId
						<< " generated events = " << resources->events_.size()
						<< std::endl;
				return false;
			}

			ABUEvent* evt = resources->events_[buResourceId];
			resources->buResIdInUse_ = buResourceId;
			if (generateEvent(evt)) {
				resources->lock();
				resources->nbEventsBuilt_++;
				resources->builtIds_.push(buResourceId);
				resources->unlock();

				// TASK
				resources->pushTask('s');

			} else {
				LOG4CPLUS_INFO(resources->log_,
						"building:received null post -- input file closed?");
				resources->lock();
				unsigned int saveBUResourceId = buResourceId;
				//buResourceId = freeIds_.front(); freeIds_.pop();
				resources->freeIds_.push(saveBUResourceId);
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
			buResourceId = resources->builtIds_.front();

			resources->builtIds_.pop();
			resources->unlock();

			if (buResourceId >= (uint32_t) resources->events_.size()) {
				LOG4CPLUS_INFO(resources->log_,
						"shutdown 'executing' workloop.");
				std::cout << "s: buResourceId = " << buResourceId
						<< " generated events = " << resources->events_.size()
						<< std::endl;
				return false;
			}

			// add to queue of ready Ids
			resources->readyIds_.push(buResourceId);
			resources->readyToSendQueueSize_++;
			break;
		}
			// SENDING - part 2 (request)
		case 'r':
			//std::cout << "exec Request " << current.id() << std::endl;
			// if there are ready id's
			if (resources->readyIds_.size() > 0) {
				resources->lock();
				unsigned int fuResourceId = resources->rqstIds_.front();
				resources->rqstIds_.pop();
				resources->unlock();
				buResourceId = resources->readyIds_.front();
				resources->readyIds_.pop();
				resources->readyToSendQueueSize_--;

				ABUEvent* evt = resources->events_[buResourceId];
				toolbox::mem::Reference* msg =
						createMsgChain(evt, fuResourceId);

				resources->lock();
				resources->nbEventsInBU_--;
				resources->nbEventsSent_++;
				resources->sentIds_.insert(buResourceId);
				resources->unlock();
#ifdef DEBUG_BU_1
				std::cout << "posting frame for bu resource id "
				<< buResourceId << " fuResId " << fuResourceId
				<< std::endl;
#endif

				resources->bu_->postI2OFrame(resources->fuAppDesc_, msg);

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
	resources->stopExecution_ = true;
	::sleep(2);
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
	if (resources->replay_.value_ && resources->nbEventsBuilt_
			>= (uint32_t) resources->events_.size()) {
                if (0!=PlaybackRawDataProvider::instance())
		  PlaybackRawDataProvider::instance()->setFreeToEof();
		return true;
	}

	unsigned int evtNumber = (resources->firstEvent_ + resources->evtNumber_++)
			% 0x1000000;

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

	while (currentFedIndex < resources->validFedIds_.size()) {

		while (iSf < evt->getCapacity()) {

			// get a fragment
			SuperFragment* sf = evt->sfAt(iSf);
			sf->use();
			if (resources->crc_)
				sf->computeCRC();

			bool pushedOK;

			// FIXED SIZE
			if ("RANDOM" == resources->mode_.toString()
					&& resources->useFixedFedSize_) {
				pushedOK = sf->pushFrame(evt->getEvtNumber(),
						resources->validFedIds_[currentFedIndex],
						MIN_WORDS_IN_FRAME, INITIAL_CRC);
			}

			// A LA EMILIO
			else if ("RANDOM" == resources->mode_.toString()) {
				unsigned int fedSizeMin = MIN_WORDS_IN_FRAME * BYTES_IN_WORD;
				unsigned int fedSize = resources->fedSizeMean_.value_;
				double logFedSize = CLHEP::RandGauss::shoot(gaussianMean_,
						gaussianWidth_);
				fedSize = (unsigned int) (std::exp(logFedSize));
				if (fedSize < fedSizeMin)
					fedSize = fedSizeMin;
				if (fedSize > resources->fedSizeMax_.value_)
					fedSize = resources->fedSizeMax_.value_;
				fedSize -= fedSize % 8;

				// FED Size in WORDS... pushFrame takes the size in WORDS
				fedSize /= BYTES_IN_WORD;

				pushedOK = sf->pushFrame(evt->getEvtNumber(),
						resources->validFedIds_[currentFedIndex], fedSize,
						INITIAL_CRC);
			}

			// PLAYBACK
			else if ("PLAYBACK" == resources->mode_.toString() && 0
					!= PlaybackRawDataProvider::instance()) {

				fedId = resources->validFedIds_[currentFedIndex];
				fedSize = event->FEDData(fedId).size();

				if (fedSize > 0) {
					fedAddr = event->FEDData(fedId).data();
					if (resources->overwriteEvtId_.value_ && fedAddr != 0) {
						FEDHeader initH(fedAddr);
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
						resources->log_,
						"FED Frame generation mode: "
								<< resources->mode_.toString()
								<< " NOT RECOGNIZED! Please select either <RANDOM>, <PLAYBACK>, or set the useFixedFedSize_ appropriately!");
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

			if (currentFedIndex == resources->validFedIds_.size())
				break;
		}
		// have we finished id's?
		if (currentFedIndex == resources->validFedIds_.size()) {
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
	BUMessageCutter mCutter(resources->msgBufferSize_, resources->log_,
			resources->buAppDesc_, resources->fuAppDesc_, resources->i2oPool_);
	// call to createMSgChain
	if ("SIMPLE" == resources->msgChainCreationMode_.toString()) {
		return mCutter.createMessageChainSimple(evt, fuResourceId);
	}
	return mCutter.createMessageChain(evt, fuResourceId);

}

//______________________________________________________________________________

void Executing::do_moveToFailedState(xcept::Exception& exception) const {
	//outermost_context().getSharedResources()->moveToFailedState(exception);
}

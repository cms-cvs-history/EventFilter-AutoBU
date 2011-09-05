////////////////////////////////////////////////////////////////////////////////
//
// BU
// --
//
//                                         Emilio Meschi <emilio.meschi@cern.ch>
//                       Philipp Schieferdecker <philipp.schieferdecker@cern.ch>
//							    Andrei Spataru <andrei.cristian.spataru@cern.ch>
////////////////////////////////////////////////////////////////////////////////


#include "EventFilter/AutoBU/interface/BU.h"
#include "EventFilter/AutoBU/interface/FrameSizeUtils.h"
#include "EventFilter/AutoBU/interface/BUMessageCutter.h"
#include "EventFilter/AutoBU/interface/ABUConfig.h"

#include "DataFormats/FEDRawData/interface/FEDNumbering.h"
#include "DataFormats/FEDRawData/interface/FEDRawDataCollection.h"

#include "xoap/SOAPEnvelope.h"
#include "xoap/SOAPBody.h"
#include "xoap/domutils.h"

#include <netinet/in.h>
#include <sstream>
#include <errno.h>

using namespace std;
using namespace evf;

//#define DEBUG_BU
//#define FORCE_I2O_BU
#define BU_MONITOR

////////////////////////////////////////////////////////////////////////////////
// construction/destruction
////////////////////////////////////////////////////////////////////////////////

//______________________________________________________________________________

BU::BU(xdaq::ApplicationStub *s) :
	xdaq::Application(s), log_(getApplicationLogger()),
			buAppDesc_(getApplicationDescriptor()), fuAppDesc_(0),
			buAppContext_(getApplicationContext()), fsm_(this), gui_(0),
			evtNumber_(0), buTaskId_(0), isExecuting_(false),
			isHalting_(false), wlExecuting_(0), asExecuting_(0), instance_(0),
			runNumber_(0), memUsedInMB_(0.0), taskQueueSize_(0),
			readyToSendQueueSize_(0), avgTaskQueueSize_(0),
			maxFedSizeGenerated_(0), nbEventsInBU_(0), nbEventsRequested_(0),
			nbEventsBuilt_(0), nbEventsSent_(0), nbEventsDiscarded_(0),
			mode_("RANDOM-SIMPLE"), replay_(false), crc_(true),
			overwriteEvtId_(true), firstEvent_(1), queueSize_(256),
			eventBufferSize_(0x400000), msgBufferSize_(32768),
			fedSizeMax_(65536), fedSizeMean_(1024), fedSizeWidth_(1024),
			useFixedFedSize_(false),
			initialSFBuffer_(INITIAL_SUPERFRAGMENT_SIZE_BYTES),
			msgChainCreationMode_("NORMAL"), i2oPool_(0), qSizeIter_(0),
			startTime_(0), lastDiff_(0) {

	// initialize state machine
	fsm_.initialize<evf::BU> (this);

	// initialize application info
	url_ = getApplicationDescriptor()->getContextDescriptor()->getURL() + "/"
			+ getApplicationDescriptor()->getURN();
	class_ = getApplicationDescriptor()->getClassName();
	instance_ = getApplicationDescriptor()->getInstance();
	hostname_ = getApplicationDescriptor()->getContextDescriptor()->getURL();
	sourceId_ = class_.toString() + instance_.toString();

	// i2o callbacks
	i2o::bind(this, &BU::I2O_BU_ALLOCATE_Callback, I2O_BU_ALLOCATE,
			XDAQ_ORGANIZATION_ID);
	i2o::bind(this, &BU::I2O_BU_DISCARD_Callback, I2O_BU_DISCARD,
			XDAQ_ORGANIZATION_ID);

	// allocate i2o memery pool
	string i2oPoolName = sourceId_ + "_i2oPool";
	try {
		toolbox::mem::HeapAllocator *allocator =
				new toolbox::mem::HeapAllocator();
		toolbox::net::URN urn("toolbox-mem-pool", i2oPoolName);
		toolbox::mem::MemoryPoolFactory* poolFactory =
				toolbox::mem::getMemoryPoolFactory();
		i2oPool_ = poolFactory->createPool(urn, allocator);
	} catch (toolbox::mem::exception::Exception& e) {
		string s = "Failed to create pool: " + i2oPoolName;
		LOG4CPLUS_FATAL(log_, s);
		XCEPT_RETHROW(xcept::Exception, s, e);
	}

	// web interface
	xgi::bind(this, &evf::BU::webPageRequest, "Default");
	gui_ = new WebGUI(this, &fsm_);
	gui_->setSmallAppIcon("/rubuilder/bu/images/bu32x32.gif");
	gui_->setLargeAppIcon("/rubuilder/bu/images/bu64x64.gif");

	vector<toolbox::lang::Method*> methods = gui_->getMethods();
	vector<toolbox::lang::Method*>::iterator it;
	for (it = methods.begin(); it != methods.end(); ++it) {
		if ((*it)->type() == "cgi") {
			string name = static_cast<xgi::MethodSignature*> (*it)->name();
			xgi::bind(this, &evf::BU::webPageRequest, name);
		}
	}
	xgi::bind(this, &evf::BU::customWebPage, "customWebPage");

	// export parameters to info space(s)
	exportParameters();

	// findRcmsStateListener
	fsm_.findRcmsStateListener();

	// initialize array values for qSize
	for (unsigned int i = 0; i < AVG_TASK_QUEUE_STAT_WIDTH; i++)
		qSizes_[i] = 0;
}

//______________________________________________________________________________

BU::~BU() {
	while (!events_.empty()) {
		delete events_.back();
		events_.pop_back();
	}
}
////////////////////////////////////////////////////////////////////////////////
// implementation of member functions
////////////////////////////////////////////////////////////////////////////////

//______________________________________________________________________________

bool BU::configuring(toolbox::task::WorkLoop* wl) {
	isHalting_ = false;
	try {
		LOG4CPLUS_INFO(log_, "Start configuring ...");

		// BUFU
		bufu_ = BUFUInterface::instance();
#ifdef FORCE_I2O_BU
		bufu_ = BUFUInterface::forceNewInstance();
#endif
		bufu_->registerBU(this, &log_);

		reset();
		LOG4CPLUS_INFO(log_, "Finished configuring!");
		fsm_.fireEvent("ConfigureDone", this);
	}

	catch (xcept::Exception &e) {
		string msg = "configuring FAILED: " + (string) e.what();
		fsm_.fireFailed(msg, this);
	}
	return false;
}

//______________________________________________________________________________

bool BU::enabling(toolbox::task::WorkLoop* wl) {

	isHalting_ = false;

#ifdef BU_MONITOR
	// reset start time and start performance monitor workloop
	startTime_ = time(0);
	startPerfMonitorWorkloop();
#endif

	try {
		// playback case
		// SEC GT??
		if (false) {
			//if (0 != PlaybackRawDataProvider::instance()) {
			for (unsigned int i = 0; i < (unsigned int) FEDNumbering::MAXFEDID
					+ 1; i++)
				if (FEDNumbering::inRange(i))
					validFedIds_.push_back(i);

			// random case
		} else {
			for (unsigned int i = 0; i < (unsigned int) FEDNumbering::MAXFEDID
					+ 1; i++)
				if (FEDNumbering::inRangeNoGT(i))
					validFedIds_.push_back(i);
		}

		// put initial build requests in
		for (unsigned int i = 0; i < queueSize_; i++) {
			pushTask('b');
		}

		//TASK
		if (!isExecuting_)
			startExecutionWorkLoop();

		fsm_.fireEvent("EnableDone", this);

	} catch (xcept::Exception &e) {
		string msg = "enabling FAILED: " + (string) e.what();
		fsm_.fireFailed(msg, this);
	}

	return false;
}

//______________________________________________________________________________

bool BU::stopping(toolbox::task::WorkLoop* wl) {

	try {
		LOG4CPLUS_INFO(log_, "Start stopping :) ...");

		if (0 != PlaybackRawDataProvider::instance() && (!replay_.value_
				|| nbEventsBuilt_ < (uint32_t) events_.size())) {
			lock();
			freeIds_.push(events_.size());
			unlock();
			//postBuild();
			pushTask('b');

			while (!builtIds_.empty()) {
				LOG4CPLUS_INFO(log_,
						"wait to flush ... #builtIds=" << builtIds_.size());
				::sleep(1);
			}
			// let the playback go to the last event and exit
			PlaybackRawDataProvider::instance()->setFreeToEof();
		}

		lock();
		builtIds_.push(events_.size());
		unlock();

		//postSend();
		pushTask('s');

		while (!sentIds_.empty()) {
			LOG4CPLUS_INFO(log_, "wait to flush ...");
			::sleep(1);
		}
		if (0 != PlaybackRawDataProvider::instance() && (replay_.value_
				&& nbEventsBuilt_ >= (uint32_t) events_.size())) {
			lock();
			freeIds_.push(events_.size());
			unlock();
			//postBuild();
			pushTask('b');
		}
		LOG4CPLUS_INFO(log_, "Finished stopping!");
		fsm_.fireEvent("StopDone", this);
	} catch (xcept::Exception &e) {
		string msg = "stopping FAILED: " + (string) e.what();
		fsm_.fireFailed(msg, this);
	}

	return false;
}

//______________________________________________________________________________

bool BU::halting(toolbox::task::WorkLoop* wl) {

	try {
		LOG4CPLUS_INFO(log_, "Start halting ...");
		isHalting_ = true;
		if (isExecuting_) {
			lock();
			freeIds_.push(events_.size());
			builtIds_.push(events_.size());
			unlock();
			//postBuild();
			//postSend();
			pushTask('b');
			pushTask('s');
		}
		LOG4CPLUS_INFO(log_, "Finished halting!");
		fsm_.fireEvent("HaltDone", this);
	} catch (xcept::Exception &e) {
		string msg = "halting FAILED: " + (string) e.what();
		fsm_.fireFailed(msg, this);
	}

	return false;
}

//______________________________________________________________________________

xoap::MessageReference BU::fsmCallback(xoap::MessageReference msg)
		throw (xoap::exception::Exception) {
	return fsm_.commandCallback(msg);
}

//______________________________________________________________________________

void BU::I2O_BU_ALLOCATE_Callback(toolbox::mem::Reference *bufRef) {
	if (isHalting_) {
		LOG4CPLUS_WARN(log_, "Ignore BU_ALLOCATE message while halting.");
		bufRef->release();
		return;
	}

	I2O_MESSAGE_FRAME *stdMsg;
	I2O_BU_ALLOCATE_MESSAGE_FRAME *msg;

	stdMsg = (I2O_MESSAGE_FRAME*) bufRef->getDataLocation();
	msg = (I2O_BU_ALLOCATE_MESSAGE_FRAME*) stdMsg;

	if (0 == fuAppDesc_) {
		I2O_TID fuTid = stdMsg->InitiatorAddress;
		fuAppDesc_ = i2o::utils::getAddressMap()->getApplicationDescriptor(
				fuTid);
	}

	for (unsigned int i = 0; i < msg->n; i++) {
		unsigned int fuResourceId = msg->allocate[i].fuTransactionId;

		// TASK
		pushTask('r');

		lock();
		rqstIds_.push(fuResourceId);
		//postRqst();
		nbEventsRequested_++;
		nbEventsInBU_++;
		unlock();
	}

	bufRef->release();
}

//______________________________________________________________________________

void BU::I2O_BU_DISCARD_Callback(toolbox::mem::Reference *bufRef) {

	if (isHalting_) {
		LOG4CPLUS_WARN(log_, "Ignore BU_DISCARD message while halting.");
		bufRef->release();
		return;
	}

	I2O_MESSAGE_FRAME *stdMsg = (I2O_MESSAGE_FRAME*) bufRef->getDataLocation();
	I2O_BU_DISCARD_MESSAGE_FRAME*msg = (I2O_BU_DISCARD_MESSAGE_FRAME*) stdMsg;
	unsigned int buResourceId = msg->buResourceId[0];

	lock();
	int result = sentIds_.erase(buResourceId);
	unlock();

	if (!result) {
		LOG4CPLUS_ERROR(log_,
				"can't discard unknown buResourceId '" << buResourceId << "'");
	} else {
		lock();
		freeIds_.push(buResourceId);
		nbEventsDiscarded_.value_++;
		unlock();
		//postBuild();

		// TASK
		pushTask('b');
	}

	bufRef->release();
}

//______________________________________________________________________________

void BU::DIRECT_BU_ALLOCATE(const UIntVec_t& fuResourceIds,
		xdaq::ApplicationDescriptor* fuAppDesc) {

	if (isHalting_) {
		LOG4CPLUS_WARN(log_, "Ignore BU_ALLOCATE call while halting.");
		return;
	}

	if (0 == fuAppDesc_) {
		fuAppDesc_ = fuAppDesc;
	}

	for (UInt_t i = 0; i < fuResourceIds.size(); i++) {
		UInt_t fuResourceId = fuResourceIds[i];

		// TASK
		pushTask('r');

		lock();
		rqstIds_.push(fuResourceId);
		nbEventsRequested_++;
		nbEventsInBU_++;
		unlock();
	}
}

//______________________________________________________________________________

void BU::DIRECT_BU_DISCARD(UInt_t buResourceId) {
	if (isHalting_) {
		LOG4CPLUS_WARN(log_, "Ignore BU_DISCARD call while halting.");
		return;
	}

	lock();
	int result = sentIds_.erase(buResourceId);
	unlock();

	if (!result) {
		LOG4CPLUS_ERROR(log_,
				"can't discard unknown buResourceId '" << buResourceId << "'");
	} else {
		lock();
		freeIds_.push(buResourceId);
		nbEventsDiscarded_.value_++;
		unlock();

		// TASK
		pushTask('b');
	}

}

//______________________________________________________________________________

void BU::actionPerformed(xdata::Event& e) {

	gui_->monInfoSpace()->lock();
	if (e.type() == "urn:xdata-event:ItemGroupRetrieveEvent") {
		if (0 != PlaybackRawDataProvider::instance())
			mode_ = "PLAYBACK";

		if (0 != i2oPool_)
			memUsedInMB_ = i2oPool_->getMemoryUsage().getUsed() * 9.53674e-07;
		else
			memUsedInMB_ = 0.0;
	} else if (e.type() == "ItemChangedEvent") {
		string item = dynamic_cast<xdata::ItemChangedEvent&> (e).itemName();
	}
	gui_->monInfoSpace()->unlock();

}

//______________________________________________________________________________

void BU::webPageRequest(xgi::Input *in, xgi::Output *out)
		throw (xgi::exception::Exception) {
	string name = in->getenv("PATH_INFO");
	if (name.empty())
		name = "defaultWebPage";
	static_cast<xgi::MethodSignature*> (gui_->getMethod(name))->invoke(in, out);
}

//______________________________________________________________________________

void BU::customWebPage(xgi::Input*in, xgi::Output*out)
		throw (xgi::exception::Exception) {
	*out << "<html></html>" << endl;
}

//______________________________________________________________________________

void BU::startExecutionWorkLoop() throw (evf::Exception) {
	try {
		LOG4CPLUS_INFO(log_, "Start 'EXECUTION' workloop");
		wlExecuting_ = toolbox::task::getWorkLoopFactory()->getWorkLoop(
				sourceId_ + "Executing", "waiting");
		if (!wlExecuting_->isActive())
			wlExecuting_->activate();
		asExecuting_ = toolbox::task::bind(this, &BU::executing,
				sourceId_ + "Executing");
		wlExecuting_->submit(asExecuting_);
		isExecuting_ = true;
	} catch (xcept::Exception& e) {
		string msg = "Failed to start workloop 'EXECUTING'.";
		XCEPT_RETHROW(evf::Exception, msg, e);
	}

}

//______________________________________________________________________________

void BU::startPerfMonitorWorkloop() throw (evf::Exception) {
	try {
		LOG4CPLUS_INFO(log_, "Start 'PERFORMANCE MONITOR' workloop");
		wlMonitoring_ = toolbox::task::getWorkLoopFactory()->getWorkLoop(
				sourceId_ + "Monitoring", "waiting");
		if (!wlMonitoring_->isActive())
			wlMonitoring_->activate();
		asMonitoring_ = toolbox::task::bind(this, &BU::monitoring,
				sourceId_ + "Monitoring");
		wlMonitoring_->submit(asMonitoring_);
	} catch (xcept::Exception& e) {
		string msg = "Failed to start workloop 'PERFORMANCE MONITOR'.";
		XCEPT_RETHROW(evf::Exception, msg, e);
	}

}

//______________________________________________________________________________

bool BU::executing(toolbox::task::WorkLoop* wl) {
	//  TASK
	qLock();
	// pop a task if queue not empty
	if (taskQueue_.size() != 0) {
		BUTask current = taskQueue_.front();
		taskQueue_.pop();
		taskQueueSize_--;

		// task queue avg
		registerSize();
		avgTaskQueueSize_ = getAverageSize();

		qUnlock();

		//BUILDING
		unsigned int buResourceId = 0;
		char currentType = current.type();

		switch (currentType) {
		case 'b':
			//std::cout << "exec Build " << current.id() << std::endl;
			lock();
			buResourceId = freeIds_.front();
			freeIds_.pop();
			unlock();
			if (buResourceId >= (uint32_t) events_.size()) {
				LOG4CPLUS_INFO(log_, "shutdown 'executing' workloop.");
				isExecuting_ = false;
				return false;
			}

			if (!isHalting_) {
				ABUEvent* evt = events_[buResourceId];
				if (generateEvent(evt)) {
					lock();
					nbEventsBuilt_++;
					builtIds_.push(buResourceId);
					unlock();

					// TASK
					pushTask('s');

				} else {
					LOG4CPLUS_INFO(log_, "building:received null post");
					lock();
					unsigned int saveBUResourceId = buResourceId;
					//buResourceId = freeIds_.front(); freeIds_.pop();
					freeIds_.push(saveBUResourceId);
					unlock();
					isExecuting_ = false;
					return false;
				}
			}
			break;

			//SENDING - part 1 (send)
		case 's':
			//std::cout << "exec Send " << current.id() << std::endl;
			lock();
			buResourceId = builtIds_.front();

			builtIds_.pop();
			unlock();

			if (buResourceId >= (uint32_t) events_.size()) {
				LOG4CPLUS_INFO(log_, "shutdown 'executing' workloop.");
				isExecuting_ = false;
				return false;
			}

			// add to queue of ready Ids
			readyIds_.push(buResourceId);
			readyToSendQueueSize_++;
			break;

			// SENDING - part 2 (request)
		case 'r':
			//std::cout << "exec Request " << current.id() << std::endl;
			if (!isHalting_) {
				// if there are ready id's
				if (readyIds_.size() > 0) {
					lock();
					unsigned int fuResourceId = rqstIds_.front();
					rqstIds_.pop();
					unlock();

					buResourceId = readyIds_.front();
					readyIds_.pop();
					readyToSendQueueSize_--;

					ABUEvent* evt = events_[buResourceId];
					toolbox::mem::Reference* msg = createMsgChain(evt,
							fuResourceId);

					lock();

					nbEventsInBU_--;
					nbEventsSent_++;
					sentIds_.insert(buResourceId);
					unlock();
#ifdef DEBUG_BU
					std::cout << "posting frame for resource id " << buResourceId
					<< std::endl;
#endif

					// BUFU
					bufu_->take(fuAppDesc_, msg);

#ifdef DEBUG_BU
					std::cout << "posted frame for resource id " << buResourceId
					<< std::endl;
#endif
				}
				// no ready id's, re-insert this request into the queue
				else {
					pushTask('r');
				}
			}
			break;
		} // end switch
	}
	//queue is empty
	else {
		qUnlock();
	}

	return true;
}

//______________________________________________________________________________

bool BU::monitoring(toolbox::task::WorkLoop* wl) {
	time_t tcurr = time(0);
	unsigned int diff = (unsigned int) difftime(tcurr, startTime_);

	if (diff % 5 == 0 && diff != 0) {
		if (diff != lastDiff_) {
			std::cout << "BU : Sent events at " << diff << "s: " << nbEventsSent_
					<< std::endl;
			lastDiff_ = diff;
		}
	}

	return true;
}

//______________________________________________________________________________

void BU::postI2OFrame(xdaq::ApplicationDescriptor* fuAppDesc,
		toolbox::mem::Reference* bufRef) {
	getApplicationContext()->postFrame(bufRef, getApplicationDescriptor(),
			fuAppDesc);
}

////////////////////////////////////////////////////////////////////////////////
// implementation of private member functions
////////////////////////////////////////////////////////////////////////////////

//______________________________________________________________________________

void BU::lock() {

	while (0 != sem_wait(&lock_)) {
		if (errno != EINTR) {
			LOG4CPLUS_ERROR(log_, "Cannot obtain lock on sem LOCK!");
		}
	}
}

//______________________________________________________________________________

void BU::qLock() {

	while (0 != sem_wait(&qLock_)) {
		if (errno != EINTR) {
			LOG4CPLUS_ERROR(log_, "Cannot obtain lock on sem QLOCK!");
		}
	}
}

//______________________________________________________________________________

void BU::exportParameters() {
	if (0 == gui_) {
		LOG4CPLUS_ERROR(log_, "No GUI, can't export parameters");
		return;
	}

	gui_->addMonitorParam("url", &url_);
	gui_->addMonitorParam("class", &class_);
	gui_->addMonitorParam("instance", &instance_);
	gui_->addMonitorParam("hostname", &hostname_);
	gui_->addMonitorParam("runNumber", &runNumber_);
	gui_->addMonitorParam("stateName", fsm_.stateName());
	gui_->addMonitorParam("memUsedInMB", &memUsedInMB_);

	gui_->addMonitorParam("taskQueueSize", &taskQueueSize_);
	gui_->addMonitorParam("readyToSendQueueSize", &readyToSendQueueSize_);
	gui_->addMonitorParam("avgTaskQueueSize", &avgTaskQueueSize_);
	gui_->addMonitorParam("maxFedSizeGenerated", &maxFedSizeGenerated_);

	gui_->addMonitorCounter("nbEvtsInBU", &nbEventsInBU_);
	gui_->addMonitorCounter("nbEvtsRequested", &nbEventsRequested_);
	gui_->addMonitorCounter("nbEvtsBuilt", &nbEventsBuilt_);
	gui_->addMonitorCounter("nbEvtsSent", &nbEventsSent_);
	gui_->addMonitorCounter("nbEvtsDiscarded", &nbEventsDiscarded_);

	gui_->addStandardParam("mode", &mode_);
	gui_->addStandardParam("replay", &replay_);
	gui_->addStandardParam("overwriteEvtId", &overwriteEvtId_);
	gui_->addStandardParam("crc", &crc_);
	gui_->addStandardParam("firstEvent", &firstEvent_);
	gui_->addStandardParam("queueSize", &queueSize_);
	gui_->addStandardParam("eventBufferSize", &eventBufferSize_);
	gui_->addStandardParam("msgBufferSize", &msgBufferSize_);
	gui_->addStandardParam("fedSizeMax", &fedSizeMax_);
	gui_->addStandardParam("fedSizeMean", &fedSizeMean_);
	gui_->addStandardParam("fedSizeWidth", &fedSizeWidth_);
	gui_->addStandardParam("useFixedFedSize", &useFixedFedSize_);

	gui_->addStandardParam("initialSuperFragmentSize", &initialSFBuffer_);
	gui_->addStandardParam("msgChainCreationMode", &msgChainCreationMode_);

	gui_->addStandardParam("monSleepSec", &monSleepSec_);
	gui_->addStandardParam("rcmsStateListener", fsm_.rcmsStateListener());
	gui_->addStandardParam("foundRcmsStateListener",
			fsm_.foundRcmsStateListener());

	gui_->exportParameters();

	gui_->addItemChangedListener("crc", this);

}

//______________________________________________________________________________

void BU::reset() {

	gui_->resetCounters();

	taskQueueSize_ = 0;
	readyToSendQueueSize_ = 0;

	while (events_.size()) {
		delete events_.back();
		events_.pop_back();
	}

	while (!rqstIds_.empty())
		rqstIds_.pop();
	while (!freeIds_.empty())
		freeIds_.pop();
	while (!builtIds_.empty())
		builtIds_.pop();
	sentIds_.clear();

	sem_init(&lock_, 0, 1);
	sem_init(&qLock_, 0, 1);

	// get an event configuration
	ABUConfig config;

	for (unsigned int i = 0; i < queueSize_; i++) {
		events_.push_back(new ABUEvent(i, config));
		freeIds_.push(i);
	}
}

//______________________________________________________________________________

bool BU::generateEvent(ABUEvent* evt) {

	// replay?
	if (replay_.value_ && nbEventsBuilt_ >= (uint32_t) events_.size()) {
		PlaybackRawDataProvider::instance()->setFreeToEof();
		return true;
	}

	unsigned int evtNumber = (firstEvent_ + evtNumber_++) % 0x1000000;

	// if event is not empty, memory is already allocated
	if (evt->isInitialised()) {
#ifdef DEBUG_BU
		std::cout << "event is initialized, resetting with: " << evtNumber
		<< std::endl;
#endif
		if (evt->usedSf() > 0) {

			evt->reset(evtNumber);
		}
		// event is empty, need space for superfragments
	} else {
#ifdef DEBUG_BU
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

		event = PlaybackRawDataProvider::instance()->getFEDRawData(runNumber,
				evtNoFromFile);
		if (event == 0)
			return false;
	}

	while (currentFedIndex < validFedIds_.size()) {

		while (iSf < evt->getCapacity()) {

			// get a fragment
			SuperFragment* sf = evt->sfAt(iSf);
			sf->use();

			bool pushedOK;

			// FIXED SIZE
			if ("FIXED" == mode_.toString()) {
				pushedOK = sf->pushFrame(evt->getEvtNumber(),
						validFedIds_[currentFedIndex], MIN_WORDS_IN_FRAME,
						INITIAL_CRC);
				updateMaxFedGen(MIN_WORDS_IN_FRAME * BYTES_IN_WORD);
			}

			// RANDOM-SIMPLE
			else if ("RANDOM-SIMPLE" == mode_.toString()) {
				// NOTE: for limit behaviour, try fedSizeMax_ * 1.5
				unsigned int maxSize = fedSizeMax_ / 2;
				maxSize /= WORD_SIZE_IN_BITS;
				unsigned int randSize = FrameSizeUtils::boundedRandom(maxSize);

				pushedOK = sf->pushFrame(evt->getEvtNumber(),
						validFedIds_[currentFedIndex], randSize, INITIAL_CRC);
				updateMaxFedGen(randSize * BYTES_IN_WORD);
			}

			// PLAYBACK
			else if ("PLAYBACK" == mode_.toString() && 0
					!= PlaybackRawDataProvider::instance()) {

				fedId = validFedIds_[currentFedIndex];
				fedSize = event->FEDData(fedId).size();

				if (fedSize > 0) {
					fedAddr = event->FEDData(fedId).data();
					if (overwriteEvtId_.value_ && fedAddr != 0) {
						FEDHeader initH(fedAddr);
						//FEDHeader::set(fedAddr, initH.triggerType(), evtNumber,
						FEDHeader::set(fedAddr, initH.triggerType(),
								evt->getEvtNumber(), initH.bxID(),
								initH.sourceID(), initH.version(),
								initH.moreHeaders());
					}

					pushedOK = sf->pushFrame(fedAddr, fedSize);
					updateMaxFedGen(fedSize);

				} else {
					// no expansion needed in this case
					// just skip
					pushedOK = true;
				}

			}
			// mode not recognised
			else {
				LOG4CPLUS_ERROR(
						log_,
						"FED Frame generation mode: " << mode_.toString()
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
#ifdef DEBUG_BU
				std::cout << "Expanding SF " << iSf << " to new size "
				<< evt->sfAt(iSf)->getTrueSize() << std::endl;
#endif
			}

			else {
				currentFedIndex++;
				iSf++;
			}

			if (currentFedIndex == validFedIds_.size())
				break;
		}
		// have we finished id's?
		if (currentFedIndex == validFedIds_.size()) {
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

toolbox::mem::Reference *BU::createMsgChain(ABUEvent* evt,
		unsigned int fuResourceId) {

	// initialize message cutter
	BUMessageCutter mCutter(msgBufferSize_, &log_, buAppDesc_, fuAppDesc_,
			i2oPool_);
	// call to createMSgChain
	if ("SIMPLE" == msgChainCreationMode_.toString()) {
		return mCutter.createMessageChainSimple(evt, fuResourceId);
	}
	return mCutter.createMessageChain(evt, fuResourceId);

}

//______________________________________________________________________________

bool BU::pushTask(unsigned char type) {
	qLock();
	if (taskQueue_.size() <= MAX_TASK_QUEUE_SIZE) {
		buTaskId_ = buTaskId_++ % 0x1000000;
		BUTask rqst(buTaskId_, type);
		taskQueue_.push(rqst);
		taskQueueSize_++;

		// task queue avg
		registerSize();
		avgTaskQueueSize_ = getAverageSize();

	} else {
		LOG4CPLUS_ERROR(log_, "Task queue is FULL! Rejecting further tasks!");
		qUnlock();
		return false;
	}
	qUnlock();
	return true;
}

//______________________________________________________________________________

void BU::registerSize() {
	if (qSizeIter_ < AVG_TASK_QUEUE_STAT_WIDTH) {
		qSizes_[qSizeIter_] = taskQueueSize_;
		qSizeIter_++;
	} else {
		qSizeIter_ = 0;
	}
}

//______________________________________________________________________________

unsigned int BU::getAverageSize() const {
	unsigned int sum = 0;
	for (unsigned int i = 0; i < AVG_TASK_QUEUE_STAT_WIDTH; i++) {
		sum += qSizes_[i];
	}
	return sum / AVG_TASK_QUEUE_STAT_WIDTH;
}

//______________________________________________________________________________

void BU::updateMaxFedGen(unsigned int testSize) {
	if (testSize > maxFedSizeGenerated_) {
		maxFedSizeGenerated_ = testSize;
	}
}

////////////////////////////////////////////////////////////////////////////////
// xdaq instantiator implementation macro
////////////////////////////////////////////////////////////////////////////////

XDAQ_INSTANTIATOR_IMPL(BU)

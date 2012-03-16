/*
 * SharedResources.cc
 *
 *  Created on: Sep 22, 2011
 *      \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/SharedResources.h"

using namespace evf;

SharedResources::SharedResources() :
	gui_(0), queueSize_(1), taskQueueSize_(0), nbEventsBuilt_(0),
			readyToSendQueueSize_(0), nbEventsInBU_(0), nbEventsSent_(0),
			replay_(false), firstEvent_(1), mode_("RANDOM-SIMPLE"),
			overwriteEvtId_(true), msgChainCreationMode_("NORMAL"),
			avgTaskQueueSize_(0), nbEventsRequested_(0), nbEventsDiscarded_(0),
			fedSizeMax_(65536), fedSizeMean_(16), fedSizeWidth_(1024),
			evtNumber_(0), buTaskId_(0), stopExecution_(false),
			wasRestarted_(false) {
}

SharedResources::~SharedResources() {
	while (!events_.empty()) {
		delete events_.back();
		events_.pop_back();
	}
}

void SharedResources::startExecutionWorkLoop() throw (evf::Exception) {

	try {
		LOG4CPLUS_INFO(logger(), "Start 'EXECUTION' workloop");

		setWlExecuting(
				toolbox::task::getWorkLoopFactory()->getWorkLoop(
						sourceId() + "Executing", "waiting"));

		if (!wlExecuting()->isActive())
			wlExecuting()->activate();

		setAsExecuting(
				toolbox::task::bind(this, &SharedResources::execute,
						sourceId() + "Executing"));

		if (restarted()) {
			// FIXME necessary?
			// a timeout to allow the resource broker to enable
			// TAKE messages might be thrown away otherwise
			::sleep(3);
		}

		wlExecuting()->submit(asExecuting());

	} catch (xcept::Exception& e) {
		string msg = "Failed to start workloop 'EXECUTING'.";
		XCEPT_RETHROW(evf::Exception, msg, e);
	}

}

//  executing workloop DISPATCHING SIGNATURE
bool SharedResources::execute(toolbox::task::WorkLoop* workLoop) {
	try {
		return fsm_->getCurrentState().execute();
	} catch (std::bad_cast) {
		cout << "execute: State not constructed!!!" << endl;
		::sleep(1);
		return true;
	}
}

Logger SharedResources::logger() const {
	return log_;
}
void SharedResources::setLogger(Logger log) {
	log_ = log;
}
BU* SharedResources::bu() const {
	return bu_;
}
void SharedResources::setBu(BU* bu) {
	bu_ = bu;
}
xdaq::ApplicationDescriptor* SharedResources::fuAppDesc() const {
	return fuAppDesc_;
}
void SharedResources::setFuAppDesc(xdaq::ApplicationDescriptor* fuAppDesc) {
	fuAppDesc_ = fuAppDesc;
}
xdaq::ApplicationDescriptor* SharedResources::buAppDesc() const {
	return buAppDesc_;
}
void SharedResources::setBuAppDesc(xdaq::ApplicationDescriptor* buAppDesc) {
	buAppDesc_ = buAppDesc;
}
std::vector<unsigned int>* SharedResources::validFedIds() {
	return &validFedIds_;
}
std::queue<unsigned int>* SharedResources::freeIds() {
	return &freeIds_;
}

xdata::UnsignedInteger32& SharedResources::queueSize() {
	return queueSize_;
}
xdata::UnsignedInteger32& SharedResources::taskQueueSize() {
	return taskQueueSize_;
}
void SharedResources::decreaseTaskQueueSize() {
	taskQueueSize_--;
}
xdata::UnsignedInteger32& SharedResources::nbEventsBuilt() {
	return nbEventsBuilt_;
}
void SharedResources::increaseEventsBuilt() {
	nbEventsBuilt_++;
}
xdata::UnsignedInteger32& SharedResources::rtsQSize() {
	return readyToSendQueueSize_;
}
xdata::UnsignedInteger32& SharedResources::firstEvent() {
	return firstEvent_;
}
xdata::UnsignedInteger32& SharedResources::avgTaskQueueSize() {
	return avgTaskQueueSize_;
}
xdata::Boolean& SharedResources::replay() {
	return replay_;
}
xdata::String& SharedResources::SharedResources::mode() {
	return mode_;
}
xdata::String& SharedResources::msgChainCreationMode() {
	return msgChainCreationMode_;
}
xdata::Boolean& SharedResources::overwriteEvtId() {
	return overwriteEvtId_;
}
void SharedResources::increaseRTSQS() {
	readyToSendQueueSize_++;
}
void SharedResources::decreaseRTSQS() {
	readyToSendQueueSize_--;
}
xdata::UnsignedInteger32& SharedResources::nbEventsInBU() {
	return nbEventsInBU_;
}
void SharedResources::decreaseEventsInBU() {
	nbEventsInBU_--;
}
void SharedResources::increaseEventsInBU() {
	nbEventsInBU_++;
}
xdata::UnsignedInteger32& SharedResources::nbEventsSent() {
	return nbEventsSent_;
}
void SharedResources::increaseEventsSent() {
	nbEventsSent_++;
}
xdata::UnsignedInteger32& SharedResources::msgBufferSize() {
	return msgBufferSize_;
}
xdata::UnsignedInteger32& SharedResources::nbEventsRequested() {
	return nbEventsRequested_;
}
void SharedResources::increaseEventsRequested() {
	nbEventsRequested_++;
}
xdata::UnsignedInteger32& SharedResources::nbEventsDiscarded() {
	return nbEventsDiscarded_;
}
void SharedResources::increaseEventsDiscarded() {
	nbEventsDiscarded_.value_++;
}
xdata::UnsignedInteger32& SharedResources::fedSizeMax() {
	return fedSizeMax_;
}
xdata::UnsignedInteger32& SharedResources::fedSizeMean() {
	return fedSizeMean_;
}
xdata::UnsignedInteger32& SharedResources::fedSizeWidth() {
	return fedSizeWidth_;
}
std::queue<BUTask>* SharedResources::taskQueue() {
	return &taskQueue_;
}
std::vector<ABUEvent*>* SharedResources::events() {
	return &events_;
}
std::queue<unsigned int>* SharedResources::builtIds() {
	return &builtIds_;
}
std::queue<unsigned int>* SharedResources::readyIds() {
	return &readyIds_;
}
std::queue<unsigned int>* SharedResources::rqstIds() {
	return &rqstIds_;
}
std::set<unsigned int>* SharedResources::sentIds() {
	return &sentIds_;
}
unsigned int& SharedResources::eventNumber() {
	return evtNumber_;
}
toolbox::mem::Pool* SharedResources::i2oPool() {
	return i2oPool_;
}
void SharedResources::setI2OPool(toolbox::mem::Pool* pool) {
	i2oPool_ = pool;
}

ABUEvent* SharedResources::eventAt(unsigned int pos) const {
	return events_[pos];
}
unsigned int SharedResources::validFedIdAt(unsigned int pos) const {
	return validFedIds_[pos];
}
void SharedResources::setWlExecuting(toolbox::task::WorkLoop* wlEx) {
	wlExecuting_ = wlEx;
}
toolbox::task::WorkLoop* SharedResources::wlExecuting() const {
	return wlExecuting_;
}
void SharedResources::setAsExecuting(toolbox::task::ActionSignature* asEx) {
	asExecuting_ = asEx;
}
toolbox::task::ActionSignature* SharedResources::asExecuting() const {
	return asExecuting_;
}
std::string SharedResources::sourceId() const {
	return sourceId_;
}
void SharedResources::setSourceId(std::string sourceId) {
	sourceId_ = sourceId;
}

bool& SharedResources::stopExecution() {
	return stopExecution_;
}

bool& SharedResources::restarted() {
	return wasRestarted_;
}

//______________________________________________________________________________

void SharedResources::reset() {

	taskQueueSize_ = 0;
	readyToSendQueueSize_ = 0;

	while (events_.size()) {
		delete events_.back();
		events_.pop_back();
	}
	while (!validFedIds_.empty())
		validFedIds_.pop_back();
	while (!rqstIds_.empty())
		rqstIds_.pop();
	while (!freeIds_.empty())
		freeIds_.pop();
	while (!builtIds_.empty())
		builtIds_.pop();
	sentIds_.clear();

	sem_init(&lock_, 0, 1);
	sem_init(&qLock_, 0, 1);
	//sem_init(&evQLock_, 0, 1);

	// get an event configuration
	ABUConfig config;

	for (unsigned int i = 0; i < queueSize_; i++) {
		events_.push_back(new ABUEvent(i, config));
		freeIds_.push(i);
	}

	nbEventsBuilt_ = 0;
	evtNumber_ = 0;
	wasRestarted_ = false;

}

//______________________________________________________________________________

bool SharedResources::pushTask(unsigned char type) {
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

void SharedResources::lock() {

	while (0 != sem_wait(&lock_)) {
		if (errno != EINTR) {
			LOG4CPLUS_ERROR(log_, "Cannot obtain lock on sem LOCK!");
		}
	}
}

//______________________________________________________________________________

void SharedResources::qLock() {

	while (0 != sem_wait(&qLock_)) {
		if (errno != EINTR) {
			LOG4CPLUS_ERROR(log_, "Cannot obtain lock on sem QLOCK!");
		}
	}
}

//______________________________________________________________________________

void SharedResources::evQLock() {

	while (0 != sem_wait(&evQLock_)) {
		if (errno != EINTR) {
			LOG4CPLUS_ERROR(log_, "Cannot obtain lock on sem EVQLOCK!");
		}
	}

}

//______________________________________________________________________________

void SharedResources::registerSize() {
	if (qSizeIter_ < AVG_TASK_QUEUE_STAT_WIDTH) {
		qSizes_[qSizeIter_] = taskQueueSize_;
		qSizeIter_++;
	} else {
		qSizeIter_ = 0;
	}
	avgTaskQueueSize_ = getAverageSize();
}

//______________________________________________________________________________

unsigned int SharedResources::getAverageSize() const {
	unsigned int sum = 0;
	for (unsigned int i = 0; i < AVG_TASK_QUEUE_STAT_WIDTH; i++) {
		sum += qSizes_[i];
	}
	return sum / AVG_TASK_QUEUE_STAT_WIDTH;
}

//______________________________________________________________________________

void SharedResources::initialiseQSizes() {
	// initialize array values for qSize
	for (unsigned int i = 0; i < AVG_TASK_QUEUE_STAT_WIDTH; i++)
		qSizes_[i] = 0;
}

//______________________________________________________________________________

void SharedResources::enqEvent(EventPtr evType) {
	evQLock();
	eventQueue_.push(evType);
	evQUnlock();
}

//______________________________________________________________________________

EventPtr SharedResources::deqEvent() {
	EventPtr eventPtr;
	evQLock();
	if (eventQueue_.size() > 0) {
		eventPtr = eventQueue_.front();
		eventQueue_.pop();
	}
	evQUnlock();
	return eventPtr;
}


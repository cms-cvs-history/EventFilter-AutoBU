/*
 * SharedResources.cc
 *
 *  Created on: Sep 22, 2011
 *      \author A. Spataru - andrei.cristian.spataru@cern.ch
 */

#include "EventFilter/AutoBU/interface/SharedResources.h"

using namespace evf::autobu_statemachine;

SharedResources::SharedResources() :
	gui_(0), queueSize_(1), taskQueueSize_(0), nbEventsBuilt_(0),
			readyToSendQueueSize_(0), nbEventsInBU_(0), nbEventsSent_(0),
			replay_(false), firstEvent_(1), mode_("RANDOM"),
			overwriteEvtId_(true), crc_(false),
			msgChainCreationMode_("NORMAL"), avgTaskQueueSize_(0),
			nbEventsRequested_(0), nbEventsDiscarded_(0), fedSizeMax_(65536),
			fedSizeMean_(16), fedSizeWidth_(1024), useFixedFedSize_(false),
			evtNumber_(0), buTaskId_(0), stopExecution_(false) {
}

SharedResources::~SharedResources() {
	while (!events_.empty()) {
		delete events_.back();
		events_.pop_back();
	}
}

void SharedResources::startExecutionWorkLoop() throw (evf::Exception) {

	try {
		LOG4CPLUS_INFO(log_, "Start 'EXECUTION' workloop");

		wlExecuting_ = toolbox::task::getWorkLoopFactory()->getWorkLoop(
				sourceId_ + "Executing", "waiting");

		if (!wlExecuting_->isActive())
			wlExecuting_->activate();

		asExecuting_ = toolbox::task::bind(this, &SharedResources::execute,
				sourceId_ + "Executing");

		wlExecuting_->submit(asExecuting_);

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
		::usleep(50000);
		return true;
	}
}

//______________________________________________________________________________

void SharedResources::resetCounters() {
	gui_->resetCounters();
	taskQueueSize_ = 0;
	readyToSendQueueSize_ = 0;
	nbEventsBuilt_ = 0;
	evtNumber_ = 0;
}

void SharedResources::initSemaphores() {
	sem_init(&lock_, 0, 1);
	sem_init(&qLock_, 0, 1);
}

void SharedResources::emptyQueues() {
	lock();
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
	unlock();
}

void SharedResources::populateEvents() {
	// get an event configuration
	lock();
	ABUConfig config;

	for (unsigned int i = 0; i < queueSize_; i++) {
		events_.push_back(new ABUEvent(i, config));
		freeIds_.push(i);
	}
	unlock();
}

//______________________________________________________________________________

bool SharedResources::pushTask(unsigned char type) {
	qLock();
	if (taskQueue_.size() <= MAX_TASK_QUEUE_SIZE) {
		//buTaskId_ = buTaskId_++ % 0x1000000;
		buTaskId_++;
	       	buTaskId_%=0x1000000;
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


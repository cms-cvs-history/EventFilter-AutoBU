/*
 * SharedResources.h
 *
 *  Created on: Sep 21, 2011
 *      Author: aspataru - andrei.cristian.spataru@cern.ch
 */

#ifndef SHAREDRESOURCES_H_
#define SHAREDRESOURCES_H_

// required for binding
#include "xdaq/Application.h"

#include "EventFilter/BUFUInterface/interface/BUFUInterface.h"
#include "EventFilter/Playback/interface/PlaybackRawDataProvider.h"
#include "EventFilter/AutoBU/interface/ABUEvent.h"
#include "EventFilter/AutoBU/interface/BUTask.h"
#include "EventFilter/BUFUInterface/interface/BaseBU.h"

#include "toolbox/task/WorkLoopFactory.h"
#include "toolbox/task/WaitingWorkLoop.h"
#include "toolbox/task/Action.h"

#include "boost/statechart/event_base.hpp"

#include "xdata/InfoSpace.h"
#include "xdata/UnsignedInteger32.h"
#include "xdata/Double.h"
#include "xdata/Boolean.h"
#include "xdata/String.h"

#include <semaphore.h>
#include <iostream>
#include <queue>

using std::string;
using namespace toolbox;

namespace evf {

typedef boost::shared_ptr<boost::statechart::event_base> EventPtr;

class SharedResources {

public:
	SharedResources();
	~SharedResources();

	void reset();
	/// Adds a task to the task queue
	bool pushTask(unsigned char type);
	/// Registers the current size of the task queue
	void registerSize();
	/// Returns the average size of the task queue, over a fixed number of measurements
	unsigned int getAverageSize() const;
	void initialiseQSizes();
	// event queue access (thread safe)
	void enqEvent(EventPtr evType);
	EventPtr deqEvent();
	// flags
	bool& stopExecution();
	bool& restarted();

	/*
	 * CONCURRENT ACCESS
	 */
	void lock();
	void unlock() {
		sem_post(&lock_);
	}
	void qLock();
	void qUnlock() {
		sem_post(&qLock_);
	}
	void evQLock();
	void evQUnlock() {
		sem_post(&evQLock_);
	}
	void initEvQSem() {
		sem_init(&evQLock_, 0, 1);
	}

	/*
	 * ACCESS TO SHARED VARIABLES
	 */
	Logger logger() const;
	void setLogger(Logger log);
	BUFUInterface* interface() const;
	void setInterface(BUFUInterface* bufu);
	BaseBU* bu() const;
	void setBu(BaseBU* bu);
	xdaq::ApplicationDescriptor* fuAppDesc() const;
	void setFuAppDesc(xdaq::ApplicationDescriptor* fuAppDesc);
	xdaq::ApplicationDescriptor* buAppDesc() const;
	void setBuAppDesc(xdaq::ApplicationDescriptor* buAppDesc);
	std::vector<unsigned int>* validFedIds();
	std::queue<unsigned int>* freeIds();
	xdata::UnsignedInteger32& queueSize();
	xdata::UnsignedInteger32& taskQueueSize();
	void decreaseTaskQueueSize();
	xdata::UnsignedInteger32& nbEventsBuilt();
	void increaseEventsBuilt();
	xdata::UnsignedInteger32& rtsQSize();
	xdata::UnsignedInteger32& firstEvent();
	xdata::UnsignedInteger32& avgTaskQueueSize();
	xdata::Boolean& replay();
	xdata::String& mode();
	xdata::String& msgChainCreationMode();
	xdata::Boolean& overwriteEvtId();
	void increaseRTSQS();
	void decreaseRTSQS();
	xdata::UnsignedInteger32& nbEventsInBU();
	void decreaseEventsInBU();
	void increaseEventsInBU();
	xdata::UnsignedInteger32& nbEventsSent();
	void increaseEventsSent();
	xdata::UnsignedInteger32& msgBufferSize();
	xdata::UnsignedInteger32& maxFedSizeGen();
	xdata::UnsignedInteger32& nbEventsRequested();
	void increaseEventsRequested();
	xdata::UnsignedInteger32& nbEventsDiscarded();
	void increaseEventsDiscarded();
	xdata::UnsignedInteger32& fedSizeMax();
	std::queue<BUTask>* taskQueue();
	std::vector<ABUEvent*>* events();
	std::queue<unsigned int>* builtIds();
	std::queue<unsigned int>* readyIds();
	std::queue<unsigned int>* rqstIds();
	std::set<unsigned int>* sentIds();
	unsigned int& eventNumber();
	toolbox::mem::Pool* i2oPool();
	void setI2OPool(toolbox::mem::Pool* pool);
	// convenience method
	ABUEvent* eventAt(unsigned int pos) const;
	// convenience method
	unsigned int validFedIdAt(unsigned int pos) const;
	void setWlExecuting(toolbox::task::WorkLoop* wlEx);
	toolbox::task::WorkLoop* wlExecuting() const;
	void setAsExecuting(toolbox::task::ActionSignature* asEx);
	toolbox::task::ActionSignature* asExecuting() const;
	std::string sourceId() const;
	void setSourceId(std::string sourceId);

private:
	/*
	 * variables
	 */
	// Pointer to BU instance
	BaseBU* bu_;
	/// BUFU Interface
	BUFUInterface* bufu_;
	// BU message logger
	Logger log_;
	// BU application descriptor
	xdaq::ApplicationDescriptor *buAppDesc_;
	// FU application descriptor
	xdaq::ApplicationDescriptor *fuAppDesc_;

	std::vector<unsigned int> validFedIds_;
	std::queue<unsigned int> freeIds_;
	std::vector<evf::ABUEvent*> events_;
	std::queue<unsigned int> builtIds_;
	std::queue<unsigned int> rqstIds_;
	std::set<unsigned int> sentIds_;

	// workloop / action signature for executing
	toolbox::task::WorkLoop *wlExecuting_;
	toolbox::task::ActionSignature *asExecuting_;

	// event queue size in BU
	xdata::UnsignedInteger32 queueSize_;
	xdata::UnsignedInteger32 taskQueueSize_;
	xdata::UnsignedInteger32 nbEventsBuilt_;
	xdata::UnsignedInteger32 readyToSendQueueSize_;
	xdata::UnsignedInteger32 nbEventsInBU_;
	xdata::UnsignedInteger32 nbEventsSent_;
	xdata::Boolean replay_;
	xdata::UnsignedInteger32 firstEvent_;
	xdata::String mode_;
	xdata::Boolean overwriteEvtId_;
	xdata::String msgChainCreationMode_;
	xdata::UnsignedInteger32 msgBufferSize_;
	xdata::UnsignedInteger32 maxFedSizeGenerated_;
	xdata::UnsignedInteger32 avgTaskQueueSize_;
	xdata::UnsignedInteger32 nbEventsRequested_;
	xdata::UnsignedInteger32 nbEventsDiscarded_;
	xdata::UnsignedInteger32 fedSizeMax_;

	// TASK QUEUE
	std::queue<BUTask> taskQueue_;
	// RESOURCES READY TO BE SENT
	std::queue<unsigned int> readyIds_;

	// locks
	sem_t lock_;
	sem_t qLock_;
	sem_t evQLock_;

	unsigned int evtNumber_;
	// memory pool for i20 communication
	toolbox::mem::Pool* i2oPool_;
	unsigned int buTaskId_;
	std::string sourceId_;

	/**
	 * Used to compute average task queue size
	 * over last AVG_TASK_QUEUE_WIDTH modifications
	 */
	unsigned int qSizes_[AVG_TASK_QUEUE_STAT_WIDTH];
	unsigned int qSizeIter_;

	std::queue<EventPtr> eventQueue_;

	// flags
	bool stopExecution_;
	bool wasRestarted_;

};

typedef boost::shared_ptr<SharedResources> SharedResourcesPtr;

} //end namespace evf

#endif /* SHAREDRESOURCES_H_ */

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
#include "EventFilter/AutoBU/interface/BStateMachine.h"
#include "EventFilter/Playback/interface/PlaybackRawDataProvider.h"
#include "EventFilter/AutoBU/interface/ABUEvent.h"
#include "EventFilter/AutoBU/interface/BUTask.h"
#include "EventFilter/Utilities/interface/IndependentWebGUI.h"

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

class BU;

namespace autobu_statemachine {

typedef boost::shared_ptr<boost::statechart::event_base> EventPtr;

class SharedResources: public toolbox::lang::Class {

public:

	SharedResources();
	~SharedResources();

	/*
	 * WORKLOOPS
	 */
	virtual void startExecutionWorkLoop() throw (evf::Exception);
	bool execute(toolbox::task::WorkLoop* wl);

	void setFsmPointer(BStateMachine* const fsm) {
		fsm_ = fsm;
	}

	unsigned int buResIdInUse_;

	void resetCounters();
	void initSemaphores();
	void emptyQueues();
	void populateEvents();
	void updateGUIExternalState(string newState) {
		//lock();
		gui_->updateExternalState(newState);
		//unlock();
	}
	void updateGUIInternalState(string newState) {
		//lock();
		gui_->updateInternalState(newState);
		//unlock();
	}
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

public:
	// BU web interface
	IndependentWebGUI *gui_;

private:
	/*
	 * variables
	 */
	// pointer to FSM
	BStateMachine* fsm_;
	// Pointer to BU instance
	BU* bu_;

	// BU message logger
	Logger log_;
	// BU application descriptor
	xdaq::ApplicationDescriptor *buAppDesc_;
	// FU application descriptor
	xdaq::ApplicationDescriptor *fuAppDesc_;

	/*
	 * The all-important gang of resources
	 */
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
	xdata::Boolean crc_;
	xdata::String msgChainCreationMode_;
	xdata::UnsignedInteger32 msgBufferSize_;
	xdata::UnsignedInteger32 avgTaskQueueSize_;
	xdata::UnsignedInteger32 nbEventsRequested_;
	xdata::UnsignedInteger32 nbEventsDiscarded_;
	xdata::UnsignedInteger32 fedSizeMax_;
	xdata::UnsignedInteger32 fedSizeMean_;
	xdata::UnsignedInteger32 fedSizeWidth_;
	xdata::Boolean useFixedFedSize_;

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

	/*
	 * FRIENDS
	 */
	friend class evf::BU;

	friend class Configuring;
	friend class Enabled;
	friend class Enabling;
	friend class Executing;
	friend class Halting;
	friend class Ready;
	friend class Stopping;

};

typedef boost::shared_ptr<SharedResources> SharedResourcesPtr;

} // end namespace autobu_statemachine

} //end namespace evf

#endif /* SHAREDRESOURCES_H_ */

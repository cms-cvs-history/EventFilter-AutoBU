/** \class BU
 * 	Application that emulates the CMS Builder Unit.
 * 	Generates events and sends them to the Filter Unit.
 */

#ifndef AUTOBU_BU_H
#define AUTOBU_BU_H 1

#include "EventFilter/BUFUInterface/interface/BaseBU.h"
#include "EventFilter/BUFUInterface/interface/BUFUInterface.h"

#include "EventFilter/AutoBU/interface/ABUEvent.h"
#include "EventFilter/AutoBU/interface/BUTask.h"

#include "EventFilter/Utilities/interface/StateMachine.h"
#include "EventFilter/Utilities/interface/WebGUI.h"
#include "EventFilter/Utilities/interface/Exception.h"

#include "EventFilter/Playback/interface/PlaybackRawDataProvider.h"

#include "xdaq/Application.h"

#include "toolbox/mem/HeapAllocator.h"
#include "toolbox/mem/MemoryPoolFactory.h"
#include "toolbox/net/URN.h"
#include "toolbox/fsm/exception/Exception.h"

#include "xdata/InfoSpace.h"
#include "xdata/UnsignedInteger32.h"
#include "xdata/Double.h"
#include "xdata/Boolean.h"
#include "xdata/String.h"

#include "interface/evb/i2oEVBMsgs.h"
#include "interface/shared/i2oXFunctionCodes.h"

#include "i2o/Method.h"
#include "i2o/utils/AddressMap.h"

#include <vector>
#include <queue>
#include <cmath>
#include <semaphore.h>
#include <sys/time.h>


namespace evf {


  class BU : public xdaq::Application,
	     public xdata::ActionListener, public BaseBU
  {
  public:
    //
    // xdaq instantiator macro
    //
    XDAQ_INSTANTIATOR();

    //
    // construction/destruction
    //
    BU(xdaq::ApplicationStub *s);
    virtual ~BU();

    //
    // public member functions
    //

    // work loop functions to be executed during transitional states (async)
    bool configuring(toolbox::task::WorkLoop* wl);
    bool enabling(toolbox::task::WorkLoop* wl);
    bool stopping(toolbox::task::WorkLoop* wl);
    bool halting(toolbox::task::WorkLoop* wl);

    // fsm soap command callback
    xoap::MessageReference fsmCallback(xoap::MessageReference msg)
      throw (xoap::exception::Exception);

    // i2o callbacks
    void I2O_BU_ALLOCATE_Callback(toolbox::mem::Reference *bufRef);
    void I2O_BU_DISCARD_Callback(toolbox::mem::Reference *bufRef);

    // direct calls
	void DIRECT_BU_ALLOCATE(const UIntVec_t& fuResourceIds,
			xdaq::ApplicationDescriptor* fuAppDesc);
	void DIRECT_BU_DISCARD(UInt_t buResourceId);

    // xdata::ActionListener callback
    void actionPerformed(xdata::Event& e);

    // Hyper DAQ web interface [see Utilities/WebGUI]
    void webPageRequest(xgi::Input *in,xgi::Output *out)
      throw (xgi::exception::Exception);
    void customWebPage(xgi::Input*in,xgi::Output*out)
      throw (xgi::exception::Exception);

    // execution workloop
    void startExecutionWorkLoop() throw (evf::Exception);
    bool executing(toolbox::task::WorkLoop* wl);

    // performance monitoring workloop
    void startPerfMonitorWorkloop() throw (evf::Exception);
    bool monitoring(toolbox::task::WorkLoop* wl);

	// BUFU
	void postI2OFrame(xdaq::ApplicationDescriptor* fuAppDesc,
			toolbox::mem::Reference* bufRef);


  private:
    //
    // private member functions
    //
    void   lock();
    void   unlock()     { sem_post(&lock_); }
    void   qLock();
    void   qUnlock()    { sem_post(&qLock_); }

    void   exportParameters();
    void   reset();


    ///Generates FED data for an event given by the pointer
    bool   generateEvent(evf::ABUEvent* evt);
    /// Creates a message chain for the Resource Broker
    toolbox::mem::Reference *createMsgChain(evf::ABUEvent *evt,
					    unsigned int fuResourceId);
    /// Adds a task to the task queue
    bool pushTask(unsigned char type);
    /// Registers the current size of the task queue
    void registerSize();
    /// Returns the average size of the task queue, over a fixed number of measurements
    unsigned int getAverageSize() const;
    /// Updates the maximum FED size generated
    void updateMaxFedGen(unsigned int testSize);


  private:
    //
    // member data
    //

    /// BUFU Interface
    BUFUInterface* 					bufu_;

    // BU message logger
    Logger                          log_;

    // BU application descriptor
    xdaq::ApplicationDescriptor    *buAppDesc_;

    // FU application descriptor
    xdaq::ApplicationDescriptor    *fuAppDesc_;

    // BU application context
    xdaq::ApplicationContext       *buAppContext_;

    // BU state machine
    StateMachine                    fsm_;

    // BU web interface
    WebGUI                         *gui_;

    // resource management
    std::vector<evf::ABUEvent*>      events_;

    std::vector<evf::SuperFragment*> sFragments_;

    std::queue<unsigned int>        rqstIds_;
    std::queue<unsigned int>        freeIds_;
    std::queue<unsigned int>        builtIds_;
    std::set<unsigned int>          sentIds_;
    unsigned int                    evtNumber_;
    unsigned int 					buTaskId_;
    std::vector<unsigned int>       validFedIds_;

    bool                            isExecuting_;
    bool                            isHalting_;

    // workloop / action signature for executing
    toolbox::task::WorkLoop        *wlExecuting_;
    toolbox::task::ActionSignature *asExecuting_;

    // workloop / action signature for performance monitoring
    toolbox::task::WorkLoop        *wlMonitoring_;
    toolbox::task::ActionSignature *asMonitoring_;

    std::string                     sourceId_;

    // monitored parameters
    xdata::String                   url_;
    xdata::String                   class_;
    xdata::UnsignedInteger32        instance_;
    xdata::String                   hostname_;
    xdata::UnsignedInteger32        runNumber_;
    xdata::Double                   memUsedInMB_;

    xdata::UnsignedInteger32        taskQueueSize_;
    xdata::UnsignedInteger32        readyToSendQueueSize_;
    xdata::UnsignedInteger32        avgTaskQueueSize_;
    xdata::UnsignedInteger32        maxFedSizeGenerated_;

    // monitored counters
    xdata::UnsignedInteger32        nbEventsInBU_;
    xdata::UnsignedInteger32        nbEventsRequested_;
    xdata::UnsignedInteger32        nbEventsBuilt_;
    xdata::UnsignedInteger32        nbEventsSent_;
    xdata::UnsignedInteger32        nbEventsDiscarded_;

    // standard parameters
    /*
     * Modes:
     * FIXED
     * RANDOM-SIMPLE
     * PLAYBACK
     */
    xdata::String                   mode_;
    xdata::Boolean                  replay_;
    xdata::Boolean                  crc_;
    xdata::Boolean                  overwriteEvtId_;
    xdata::UnsignedInteger32        firstEvent_;
    xdata::UnsignedInteger32        queueSize_;
    xdata::UnsignedInteger32        eventBufferSize_;
    xdata::UnsignedInteger32        msgBufferSize_;
    xdata::UnsignedInteger32        fedSizeMax_;
    xdata::UnsignedInteger32        fedSizeMean_;
    xdata::UnsignedInteger32        fedSizeWidth_;
    xdata::Boolean                  useFixedFedSize_;
    xdata::UnsignedInteger32        monSleepSec_;
    xdata::UnsignedInteger32        initialSFBuffer_;
    xdata::String        			msgChainCreationMode_;


    // memory pool for i20 communication
    toolbox::mem::Pool*             i2oPool_;

    // synchronization
    sem_t                           lock_;
    sem_t							qLock_;

    // TASK QUEUE
    std::queue<BUTask> 				taskQueue_;

    // RESOURCES READY TO BE SENT
    std::queue<unsigned int>        readyIds_;

    /**
     * Used to compute average task queue size
     * over last AVG_TASK_QUEUE_WIDTH modifications
     */
    unsigned int qSizes_[AVG_TASK_QUEUE_STAT_WIDTH];
    unsigned int qSizeIter_;

    // timer to mark app start time
    time_t startTime_;

    // last time difference to app start (in seconds)
    unsigned int lastDiff_;


  }; // class BU

} // namespace evf

#endif

////////////////////////////////////////////////////////////////////////////////
//
// BU
// --
//
//                                         Emilio Meschi <emilio.meschi@cern.ch>
//                       Philipp Schieferdecker <philipp.schieferdecker@cern.ch>
////////////////////////////////////////////////////////////////////////////////


#include "EventFilter/AutoBU/interface/BU.h"

#include "EventFilter/Utilities/interface/Crc.h"

#include "DataFormats/FEDRawData/interface/FEDNumbering.h"
#include "DataFormats/FEDRawData/interface/FEDRawDataCollection.h"

#include "xoap/include/xoap/SOAPEnvelope.h"
#include "xoap/include/xoap/SOAPBody.h"
#include "xoap/include/xoap/domutils.h"


#include <netinet/in.h>
#include <sstream>


using namespace std;
using namespace evf;


////////////////////////////////////////////////////////////////////////////////
// construction/destruction
////////////////////////////////////////////////////////////////////////////////

//______________________________________________________________________________
BU::BU(xdaq::ApplicationStub *s) 
  : xdaq::Application(s)
  , log_(getApplicationLogger())
  , buAppDesc_(getApplicationDescriptor())
  , fuAppDesc_(0)
  , buAppContext_(getApplicationContext())
  , fsm_(this)
  , gui_(0)
  , evtNumber_(0)
  , wlBuilding_(0)
  , asBuilding_(0)
  , wlSending_(0)
  , asSending_(0)
  , wlMonitoring_(0)
  , asMonitoring_(0)
  , instance_(0)
  , runNumber_(0)
  , memUsedInMB_(0.0)
  , deltaT_(0.0)
  , deltaN_(0)
  , deltaSumOfSquares_(0)
  , deltaSumOfSizes_(0)
  , throughput_(0.0)
  , average_(0.0)
  , rate_(0.0)
  , rms_(0.0)
  , nbEventsInBU_(0)
  , nbEventsBuilt_(0)
  , nbEventsDiscarded_(0)
  , mode_("RANDOM")
  , replay_(false)
  , crc_(true)
  , queueSize_(32)
  , eventBufferSize_(0x400000)
  , msgBufferSize_(32768)
  , fedSizeMax_(65536)
  , fedSizeMean_(1024)
  , fedSizeWidth_(1024)
  , useFixedFedSize_(false)
  , monSleepSec_(1)
  , gaussianMean_(0.0)
  , gaussianWidth_(1.0)
  , monLastN_(0)
  , monLastSumOfSquares_(0)
  , monLastSumOfSizes_(0)
  , sumOfSquares_(0)
  , sumOfSizes_(0)
  , i2oPool_(0)
{
  // initialize state machine
  fsm_.initialize<evf::BU>(this);
  
  // initialize application info
  url_     =
    getApplicationDescriptor()->getContextDescriptor()->getURL()+"/"+
    getApplicationDescriptor()->getURN();
  class_   =getApplicationDescriptor()->getClassName();
  instance_=getApplicationDescriptor()->getInstance();
  hostname_=getApplicationDescriptor()->getContextDescriptor()->getURL();
  sourceId_=class_.toString()+instance_.toString();
  
  // i2o callbacks
  i2o::bind(this,&BU::I2O_BU_ALLOCATE_Callback,I2O_BU_ALLOCATE,XDAQ_ORGANIZATION_ID);
  i2o::bind(this,&BU::I2O_BU_DISCARD_Callback, I2O_BU_DISCARD, XDAQ_ORGANIZATION_ID);
  
  // allocate i2o memery pool
  string i2oPoolName=sourceId_+"_i2oPool";
  try {
    toolbox::mem::HeapAllocator *allocator=new toolbox::mem::HeapAllocator();
    toolbox::net::URN urn("toolbox-mem-pool",i2oPoolName);
    toolbox::mem::MemoryPoolFactory* poolFactory=
      toolbox::mem::getMemoryPoolFactory();
    i2oPool_=poolFactory->createPool(urn,allocator);
  }
  catch (toolbox::mem::exception::Exception& e) {
    string s="Failed to create pool: "+i2oPoolName;
    LOG4CPLUS_FATAL(log_,s);
    XCEPT_RETHROW(xcept::Exception,s,e);
  }
  
  // web interface
  xgi::bind(this,&evf::BU::webPageRequest,"Default");
  gui_=new WebGUI(this,&fsm_);
  gui_->setSmallAppIcon("/daq/evb/bu/images/bu32x32.gif");
  gui_->setLargeAppIcon("/daq/evb/bu/images/bu64x64.gif");
  
  vector<toolbox::lang::Method*> methods=gui_->getMethods();
  vector<toolbox::lang::Method*>::iterator it;
  for (it=methods.begin();it!=methods.end();++it) {
    if ((*it)->type()=="cgi") {
      string name=static_cast<xgi::MethodSignature*>(*it)->name();
      xgi::bind(this,&evf::BU::webPageRequest,name);
    }
  }
  
  // determine valid fed ids
  for (unsigned int i=0;i<(unsigned int)FEDNumbering::lastFEDId()+1;i++)
    if (FEDNumbering::inRange(i)) validFedIds_.push_back(i);
  
  // export parameters to info space(s)
  exportParameters();

  // compute parameters for fed size generation (a la Emilio)
  gaussianMean_ =std::log((double)fedSizeMean_);
  gaussianWidth_=std::sqrt(std::log
			   (0.5*
			    (1+std::sqrt
			     (1.0+4.0*
			      fedSizeWidth_.value_*fedSizeWidth_.value_/
			      fedSizeMean_.value_/fedSizeMean_.value_))));

  // start monitoring thread, once and for all
  startMonitoringWorkLoop();
  
  // propagate crc flag to BUEvent
  BUEvent::setComputeCrc(crc_.value_);
}


//______________________________________________________________________________
BU::~BU()
{
  while (!events_.empty()) { delete events_.back(); events_.pop_back(); }
}


////////////////////////////////////////////////////////////////////////////////
// implementation of member functions
////////////////////////////////////////////////////////////////////////////////

//______________________________________________________________________________
bool BU::configuring(toolbox::task::WorkLoop* wl)
{
  try {
    LOG4CPLUS_INFO(log_,"Start configuring ...");
    reset();
    startBuildingWorkLoop();
    LOG4CPLUS_INFO(log_,"Finished configuring!");
    fsm_.fireEvent("ConfigureDone",this);
  }
  catch (xcept::Exception &e) {
    string msg = "configuring FAILED: " + (string)e.what();
    fsm_.fireFailed(msg,this);
  }

  return false;
}


//______________________________________________________________________________
bool BU::enabling(toolbox::task::WorkLoop* wl)
{
  try {
    LOG4CPLUS_INFO(log_,"Start enabling ...");
    startSendingWorkLoop();
    LOG4CPLUS_INFO(log_,"Finished enabling!");
    fsm_.fireEvent("EnableDone",this);
  }
  catch (xcept::Exception &e) {
    string msg = "enabling FAILED: " + (string)e.what();
    fsm_.fireFailed(msg,this);
  }
  
  return false;
}


//______________________________________________________________________________
bool BU::stopping(toolbox::task::WorkLoop* wl)
{
  try {
    LOG4CPLUS_INFO(log_,"Start stopping :) ...");
    lock();
    freeIds_.push(events_.size());
    builtIds_.push(events_.size());
    unlock();
    while (!sentIds_.empty()) {
      LOG4CPLUS_INFO(log_,"wait to flush ...");
      ::sleep(1);
    }
    LOG4CPLUS_INFO(log_,"Finished stopping!");
    fsm_.fireEvent("StopDone",this);
  }
  catch (xcept::Exception &e) {
    string msg = "stopping FAILED: " + (string)e.what();
    fsm_.fireFailed(msg,this);
  }
  
  return false;
}


//______________________________________________________________________________
bool BU::halting(toolbox::task::WorkLoop* wl)
{
  try {
    LOG4CPLUS_INFO(log_,"Start halting ...");
    lock();
    freeIds_.push(events_.size());
    builtIds_.push(events_.size());
    unlock();
    while (!sentIds_.empty()) {
      LOG4CPLUS_INFO(log_,"wait to flush ...");
      ::sleep(1);
    }
    LOG4CPLUS_INFO(log_,"Finished halting!");
    fsm_.fireEvent("HaltDone",this);
  }
  catch (xcept::Exception &e) {
    string msg = "halting FAILED: " + (string)e.what();
    fsm_.fireFailed(msg,this);
  }
  
  return false;
}


//______________________________________________________________________________
xoap::MessageReference BU::fsmCallback(xoap::MessageReference msg)
  throw (xoap::exception::Exception)
{
  return fsm_.commandCallback(msg);
}


//______________________________________________________________________________
void BU::I2O_BU_ALLOCATE_Callback(toolbox::mem::Reference *bufRef)
{
  I2O_MESSAGE_FRAME             *stdMsg;
  I2O_BU_ALLOCATE_MESSAGE_FRAME *msg;
  
  stdMsg=(I2O_MESSAGE_FRAME*)bufRef->getDataLocation();
  msg   =(I2O_BU_ALLOCATE_MESSAGE_FRAME*)stdMsg;
  
  if (0==fuAppDesc_) {
    I2O_TID fuTid=stdMsg->InitiatorAddress;
    fuAppDesc_=i2o::utils::getAddressMap()->getApplicationDescriptor(fuTid);
  }
  
  for (unsigned int i=0;i<msg->n;i++) {
    unsigned int fuResourceId=msg->allocate[i].fuTransactionId;
    lock();
    rqstIds_.push(fuResourceId);
    postRqst();
    nbEventsInBU_++;
    unlock();
  }

  bufRef->release();
}


//______________________________________________________________________________
void BU::I2O_BU_DISCARD_Callback(toolbox::mem::Reference *bufRef)
{
  I2O_MESSAGE_FRAME           *stdMsg=(I2O_MESSAGE_FRAME*)bufRef->getDataLocation();
  I2O_BU_DISCARD_MESSAGE_FRAME*msg   =(I2O_BU_DISCARD_MESSAGE_FRAME*)stdMsg;
  unsigned int buResourceId=msg->buResourceId[0];

  lock();
  int result=sentIds_.erase(buResourceId);
  unlock();
  
  if (!result) {
    LOG4CPLUS_ERROR(log_,"can't discard unknown buResourceId '"<<buResourceId<<"'");
  }
  else {
    lock();
    freeIds_.push(buResourceId);
    nbEventsDiscarded_.value_++;
    unlock();
    postBuild();
  }
  
  bufRef->release();
}


//______________________________________________________________________________
void BU::actionPerformed(xdata::Event& e)
{
  gui_->lockInfoSpaces();
  if (e.type()=="ItemRetrieveEvent") {
    string item=dynamic_cast<xdata::ItemRetrieveEvent&>(e).itemName();
    if (item=="mode") {
      mode_=(0==PlaybackRawDataProvider::instance())?"RANDOM":"PLAYBACK";
    }
    if (item=="memUsedInMB") {
      if (0!=i2oPool_) memUsedInMB_=i2oPool_->getMemoryUsage().getUsed()*9.53674e-07;
      else             memUsedInMB_=0.0;
    }
  }
  else if (e.type()=="ItemChangedEvent") {
    string item=dynamic_cast<xdata::ItemChangedEvent&>(e).itemName();
    if (item=="crc") BUEvent::setComputeCrc(crc_.value_);
  }
  gui_->unlockInfoSpaces();
}


//______________________________________________________________________________
void BU::webPageRequest(xgi::Input *in,xgi::Output *out)
  throw (xgi::exception::Exception)
{
  string name=in->getenv("PATH_INFO");
  if (name.empty()) name="defaultWebPage";
  static_cast<xgi::MethodSignature*>(gui_->getMethod(name))->invoke(in,out);
}


//______________________________________________________________________________
void BU::startBuildingWorkLoop() throw (evf::Exception)
{
  try {
    wlBuilding_=
      toolbox::task::getWorkLoopFactory()->getWorkLoop(sourceId_+
						       "Building",
						       "waiting");
    if (!wlBuilding_->isActive()) wlBuilding_->activate();
    asBuilding_=toolbox::task::bind(this,&BU::building,sourceId_+"Building");
    wlBuilding_->submit(asBuilding_);
  }
  catch (xcept::Exception& e) {
    string msg = "Failed to start workloop 'Building'.";
    XCEPT_RETHROW(evf::Exception,msg,e);
  }
}


//______________________________________________________________________________
bool BU::building(toolbox::task::WorkLoop* wl)
{
  waitBuild();
  lock();
  unsigned int buResourceId=freeIds_.front(); freeIds_.pop();
  unlock();
  
  if (buResourceId>=events_.size()) {
    LOG4CPLUS_INFO(log_,"shutdown 'building' workloop.");
    return false;
  }

  BUEvent* evt=events_[buResourceId];
  generateEvent(evt);
  
  lock();
  nbEventsBuilt_++;
  builtIds_.push(buResourceId);
  unlock();
  
  postSend();
  
  return true;
}


//______________________________________________________________________________
void BU::startSendingWorkLoop() throw (evf::Exception)
{
  try {
    wlSending_=toolbox::task::getWorkLoopFactory()->getWorkLoop(sourceId_+
								"Sending",
								"waiting");
    if (!wlSending_->isActive()) wlSending_->activate();
    asSending_=toolbox::task::bind(this,&BU::sending,sourceId_+"Sending");
    wlSending_->submit(asSending_);
  }
  catch (xcept::Exception& e) {
    string msg = "Failed to start workloop 'Sending'.";
    XCEPT_RETHROW(evf::Exception,msg,e);
  }
}


//______________________________________________________________________________
bool BU::sending(toolbox::task::WorkLoop* wl)
{
  waitSend();
  lock();
  unsigned int buResourceId=builtIds_.front(); builtIds_.pop();
  unlock();
  
  if (buResourceId>=events_.size()) {
    LOG4CPLUS_INFO(log_,"shutdown 'sending' workloop.");
    return false;
  }

  waitRqst();
  lock();
  unsigned int fuResourceId=rqstIds_.front(); rqstIds_.pop();
  unlock();
  
  BUEvent* evt=events_[buResourceId];
  toolbox::mem::Reference* msg=createMsgChain(evt,fuResourceId);
  
  lock();
  sumOfSquares_+=(uint64_t)evt->evtSize()*(uint64_t)evt->evtSize();
  sumOfSizes_  +=evt->evtSize();
  nbEventsInBU_--;
  sentIds_.insert(buResourceId);
  unlock();

  buAppContext_->postFrame(msg,buAppDesc_,fuAppDesc_);  

  return true;
}


//______________________________________________________________________________
void BU::startMonitoringWorkLoop() throw (evf::Exception)
{
  struct timezone timezone;
  gettimeofday(&monStartTime_,&timezone);
  
  try {
    wlMonitoring_=
      toolbox::task::getWorkLoopFactory()->getWorkLoop(sourceId_+
						       "Monitoring",
						       "waiting");
    if (!wlMonitoring_->isActive()) wlMonitoring_->activate();
    asMonitoring_=toolbox::task::bind(this,&BU::monitoring,sourceId_+"Monitoring");
    wlMonitoring_->submit(asMonitoring_);
  }
  catch (xcept::Exception& e) {
    string msg = "Failed to start workloop 'Monitoring'.";
    XCEPT_RETHROW(evf::Exception,msg,e);
  }
}


//______________________________________________________________________________
bool BU::monitoring(toolbox::task::WorkLoop* wl)
{
  struct timeval  monEndTime;
  struct timezone timezone;
  
  gettimeofday(&monEndTime,&timezone);
  
  lock();
  unsigned int monN           =nbEventsBuilt_.value_;
  uint64_t     monSumOfSquares=sumOfSquares_;
  unsigned int monSumOfSizes  =sumOfSizes_;
  uint64_t     deltaSumOfSquares;
  unlock();
  
  gui_->lockInfoSpaces();
  
  deltaT_.value_=deltaT(&monStartTime_,&monEndTime);
  monStartTime_=monEndTime;
  
  deltaN_.value_=monN-monLastN_;
  monLastN_=monN;

  deltaSumOfSquares=monSumOfSquares-monLastSumOfSquares_;
  deltaSumOfSquares_.value_=(double)deltaSumOfSquares;
  monLastSumOfSquares_=monSumOfSquares;
  
  deltaSumOfSizes_.value_=monSumOfSizes-monLastSumOfSizes_;
  monLastSumOfSizes_=monSumOfSizes;
  
  gui_->unlockInfoSpaces();
  
  if (deltaT_.value_!=0) {
    throughput_=deltaSumOfSizes_.value_/deltaT_.value_;
    rate_      =deltaN_.value_/deltaT_.value_;
  }
  else {
    throughput_=0.0;
    rate_      =0.0;
  }
  
  double meanOfSquares,mean,squareOfMean,variance;
  
  if(deltaN_.value_!=0) {
    meanOfSquares=deltaSumOfSquares_.value_/((double)(deltaN_.value_));
    mean=((double)(deltaSumOfSizes_.value_))/((double)(deltaN_.value_));
    squareOfMean=mean*mean;
    variance=meanOfSquares-squareOfMean;
    average_=deltaSumOfSizes_.value_/deltaN_.value_;
    rms_    =std::sqrt(variance);
  }
  else {
    average_=0.0;
    rms_    =0.0;
  }
  
  ::sleep(monSleepSec_.value_);

  return true;
}
    


////////////////////////////////////////////////////////////////////////////////
// implementation of private member functions
////////////////////////////////////////////////////////////////////////////////

//______________________________________________________________________________
void BU::exportParameters()
{
  if (0==gui_) {
    LOG4CPLUS_ERROR(log_,"No GUI, can't export parameters");
    return;
  }
  
  gui_->addMonitorParam("url",                &url_);
  gui_->addMonitorParam("class",              &class_);
  gui_->addMonitorParam("instance",           &instance_);
  gui_->addMonitorParam("hostname",           &hostname_);
  gui_->addMonitorParam("runNumber",          &runNumber_);
  gui_->addMonitorParam("stateName",          fsm_.stateName());
  gui_->addMonitorParam("memUsedInMB",        &memUsedInMB_);
  gui_->addMonitorParam("deltaT",             &deltaT_);
  gui_->addMonitorParam("deltaN",             &deltaN_);
  gui_->addMonitorParam("deltaSumOfSquares",  &deltaSumOfSquares_);
  gui_->addMonitorParam("deltaSumOfSizes",    &deltaSumOfSizes_);
  gui_->addMonitorParam("throughput",         &throughput_);
  gui_->addMonitorParam("average",            &average_);
  gui_->addMonitorParam("rate",               &rate_);
  gui_->addMonitorParam("rms",                &rms_);

  gui_->addMonitorCounter("nbEvtsInBU",       &nbEventsInBU_);
  gui_->addMonitorCounter("nbEvtsBuilt",      &nbEventsBuilt_);
  gui_->addMonitorCounter("nbEvtsDiscarded",  &nbEventsDiscarded_);

  gui_->addStandardParam("mode",              &mode_);
  gui_->addStandardParam("replay",            &replay_);
  gui_->addStandardParam("crc",               &crc_);
  gui_->addStandardParam("queueSize_",        &queueSize_);
  gui_->addStandardParam("eventBufferSize",   &eventBufferSize_);
  gui_->addStandardParam("msgBufferSize",     &msgBufferSize_);
  gui_->addStandardParam("fedSizeMax",        &fedSizeMax_);
  gui_->addStandardParam("fedSizeMean",       &fedSizeMean_);
  gui_->addStandardParam("fedSizeWidth",      &fedSizeWidth_);
  gui_->addStandardParam("useFixedFedSize",   &useFixedFedSize_);

  
  gui_->exportParameters();

  gui_->addItemRetrieveListener("mode",       this);
  gui_->addItemRetrieveListener("memUsedInMB",this);
  gui_->addItemChangedListener("crc",         this);
  
}


//______________________________________________________________________________
void BU::reset()
{
  gui_->resetCounters();
  
  deltaT_             =0.0;
  deltaN_             =  0;
  deltaSumOfSquares_  =  0;
  deltaSumOfSizes_    =  0;
  
  throughput_         =0.0;
  average_            =  0;
  rate_               =  0;
  rms_                =  0;

  monLastN_           =  0;
  monLastSumOfSquares_=  0;
  monLastSumOfSizes_  =  0;
  
  while (events_.size()) {
    delete events_.back();
    events_.pop_back();
  }
  
  while (!rqstIds_.empty())  rqstIds_.pop();
  while (!freeIds_.empty())  freeIds_.pop();
  while (!builtIds_.empty()) builtIds_.pop();
  sentIds_.clear();
  
  sem_init(&lock_,0,1);
  sem_init(&buildSem_,0,queueSize_);
  sem_init(&sendSem_,0,0);
  sem_init(&rqstSem_,0,0);
  
  for (unsigned int i=0;i<queueSize_;i++) {
    events_.push_back(new BUEvent(i,eventBufferSize_));
    freeIds_.push(i);
  }
}


//______________________________________________________________________________
double BU::deltaT(const struct timeval *start,const struct timeval *end)
{
  unsigned int  sec;
  unsigned int  usec;
  
  sec = end->tv_sec - start->tv_sec;
  
  if(end->tv_usec > start->tv_usec) {
    usec = end->tv_usec - start->tv_usec;
  }
  else {
    sec--;
    usec = 1000000 - ((unsigned int )(start->tv_usec - end->tv_usec));
  }
  
  return ((double)sec) + ((double)usec) / 1000000.0;
}


//______________________________________________________________________________
bool BU::generateEvent(BUEvent* evt)
{
  // replay?
  if (replay_.value_&&nbEventsBuilt_>=events_.size()) return true;
  
  // PLAYBACK mode
  if (0!=PlaybackRawDataProvider::instance()) {
    unsigned int runNumber,evtNumber;
    FEDRawDataCollection* event=
      PlaybackRawDataProvider::instance()->getFEDRawData(runNumber,evtNumber);
    evt->initialize(evtNumber);
    
    for (unsigned int i=0;i<validFedIds_.size();i++) {
      unsigned int   fedId  =validFedIds_[i];
      unsigned int   fedSize=event->FEDData(fedId).size();
      unsigned char* fedAddr=event->FEDData(fedId).data();
      if (fedSize>0) evt->writeFed(fedId,fedAddr,fedSize);
    }
    delete event;
  }
  // RANDOM mode
  else {
    unsigned int evtNumber=(++evtNumber_)%0x1000000;
    evt->initialize(evtNumber);
    unsigned int fedSizeMin=fedHeaderSize_+fedTrailerSize_;
    for (unsigned int i=0;i<validFedIds_.size();i++) {
      unsigned int fedId(validFedIds_[i]);
      unsigned int fedSize(fedSizeMean_);
      if (!useFixedFedSize_) {
	double logFedSize=RandGauss::shoot(gaussianMean_,gaussianWidth_);
	fedSize=(unsigned int)(std::exp(logFedSize));
	if (fedSize<fedSizeMin)  fedSize=fedSizeMin;
	if (fedSize>fedSizeMax_) fedSize=fedSizeMax_;
	fedSize-=fedSize%8;
      }
      
      evt->writeFed(fedId,0,fedSize);
      evt->writeFedHeader(i);
      evt->writeFedTrailer(i);
    }
    
  }  

  return true;
}


//______________________________________________________________________________
toolbox::mem::Reference *BU::createMsgChain(BUEvent* evt,
					    unsigned int fuResourceId)
{
  unsigned int msgHeaderSize =sizeof(I2O_EVENT_DATA_BLOCK_MESSAGE_FRAME);
  unsigned int msgPayloadSize=msgBufferSize_-msgHeaderSize;

  if((msgPayloadSize%4)!=0) LOG4CPLUS_ERROR(log_,"Invalid Payload Size.");
  
  toolbox::mem::Reference *head  =0;
  toolbox::mem::Reference *tail  =0;
  toolbox::mem::Reference *bufRef=0;
  
  I2O_MESSAGE_FRAME                  *stdMsg=0;
  I2O_PRIVATE_MESSAGE_FRAME          *pvtMsg=0;
  I2O_EVENT_DATA_BLOCK_MESSAGE_FRAME *block =0;
  
  unsigned int iFed            =0;
  unsigned int nSuperFrag      =64;
  unsigned int nFedPerSuperFrag=validFedIds_.size()/nSuperFrag;
  unsigned int nBigSuperFrags  =validFedIds_.size()%nSuperFrag;
  
  if (evt->nFed()<nSuperFrag) {
    nSuperFrag=evt->nFed();
    nFedPerSuperFrag=1;
    nBigSuperFrags=0;
  }
  
  // loop over all super fragments
  for (unsigned int iSuperFrag=0;iSuperFrag<nSuperFrag;iSuperFrag++) {
    
    // compute index of last fed in this super fragment
    unsigned int nFed=iFed+nFedPerSuperFrag;
    if (iSuperFrag<nBigSuperFrags) ++nFed;
    
    // compute number of blocks in this super fragment
    unsigned int nBlock  =0;
    unsigned int curbSize=frlHeaderSize_;
    unsigned int totSize =curbSize;
    for (unsigned int i=iFed;i<nFed;i++) {
      curbSize+=evt->fedSize(i);
      totSize+=evt->fedSize(i);
      if (curbSize>msgPayloadSize) {
	curbSize+=frlHeaderSize_*(curbSize/msgPayloadSize);
	if(curbSize%msgPayloadSize)totSize+=frlHeaderSize_*(curbSize/msgPayloadSize);
	else totSize+=frlHeaderSize_*((curbSize/msgPayloadSize)-1);
	curbSize=curbSize%msgPayloadSize;
      }
    }
    nBlock=totSize/msgPayloadSize+(totSize%msgPayloadSize>0 ? 1 : 0);
    
    
    // loop over all blocks (msgs) in the current super fragment
    unsigned int   remainder     =0;
    bool           fedTrailerLeft=false;
    bool           last          =false;
    bool           warning       =false;
    unsigned char *startOfPayload=0;
    U32            payload(0);
    
    for(unsigned int iBlock=0;iBlock<nBlock;iBlock++) {
      
      // If last block and its partial (there can be only 0 or 1 partial)
      payload=msgPayloadSize;
      
      // Allocate memory for a fragment block / message
      try {
	bufRef=toolbox::mem::getMemoryPoolFactory()->getFrame(i2oPool_,
							      msgBufferSize_);
      }
      catch(xcept::Exception &e) {
	LOG4CPLUS_FATAL(log_,"xdaq::frameAlloc failed");
      }
      
      // Fill in the fields of the fragment block / message
      stdMsg=(I2O_MESSAGE_FRAME*)bufRef->getDataLocation();
      pvtMsg=(I2O_PRIVATE_MESSAGE_FRAME*)stdMsg;
      block =(I2O_EVENT_DATA_BLOCK_MESSAGE_FRAME*)stdMsg;
      
      pvtMsg->XFunctionCode   =I2O_FU_TAKE;
      pvtMsg->OrganizationID  =XDAQ_ORGANIZATION_ID;
      
      stdMsg->MessageSize     =(msgHeaderSize + payload) >> 2;
      stdMsg->Function        =I2O_PRIVATE_MESSAGE;
      stdMsg->VersionOffset   =0;
      stdMsg->MsgFlags        =0;
      stdMsg->InitiatorAddress=i2o::utils::getAddressMap()->getTid(buAppDesc_);
      stdMsg->TargetAddress   =i2o::utils::getAddressMap()->getTid(fuAppDesc_);
      
      block->buResourceId           =evt->buResourceId();
      block->fuTransactionId        =fuResourceId;
      block->blockNb                =iBlock;
      block->nbBlocksInSuperFragment=nBlock;
      block->superFragmentNb        =iSuperFrag;
      block->nbSuperFragmentsInEvent=nSuperFrag;
      block->eventNumber            =evt->evtNumber();
      
      // Fill in payload 
      startOfPayload   =(unsigned char*)block+msgHeaderSize;
      frlh_t* frlHeader=(frlh_t*)startOfPayload;
      frlHeader->trigno=evt->evtNumber();
      frlHeader->segno =iBlock;
      
      unsigned char *startOfFedBlocks=startOfPayload+frlHeaderSize_;
      payload              -=frlHeaderSize_;
      frlHeader->segsize    =payload;
      unsigned int leftspace=payload;
      
      // a fed trailer was left over from the previous block
      if(fedTrailerLeft) {
	memcpy(startOfFedBlocks,
	       evt->fedAddr(iFed)+evt->fedSize(iFed)-fedTrailerSize_,
	       fedTrailerSize_);
	
	startOfFedBlocks+=fedTrailerSize_;
	leftspace       -=fedTrailerSize_;
	remainder        =0;
	fedTrailerLeft   =false;
	
	// if this is the last fed, adjust block (msg) size and set last=true
	if((iFed==nFed-1) && !last) {
	  frlHeader->segsize-=leftspace;
	  int msgSize=stdMsg->MessageSize << 2;
	  msgSize   -=leftspace;
	  bufRef->setDataSize(msgSize);
	  stdMsg->MessageSize = msgSize >> 2;
	  frlHeader->segsize=frlHeader->segsize | FRL_LAST_SEGM;
	  last=true;
	}
	
	// !! increment iFed !!
	iFed++;
      }
      
      //!
      //! remainder>0 means that a partial fed is left over from the last block
      //!
      if (remainder>0) {
	
	// the remaining fed fits entirely into the new block
	if(payload>=remainder) {
	  memcpy(startOfFedBlocks,
		 evt->fedAddr(iFed)+evt->fedSize(iFed)-remainder,
		 remainder);
	  
	  startOfFedBlocks+=remainder;
	  leftspace       -=remainder;
	  
	  // if this is the last fed in the superfragment, earmark it
	  if(iFed==nFed-1) {
	    frlHeader->segsize-=leftspace;
	    int msgSize=stdMsg->MessageSize << 2;
	    msgSize   -=leftspace;
	    bufRef->setDataSize(msgSize);
	    stdMsg->MessageSize = msgSize >> 2;
	    frlHeader->segsize=frlHeader->segsize | FRL_LAST_SEGM;
	    last=true;
	  }
	  
	  // !! increment iFed !!
	  iFed++;
	  
	  // start new fed -> set remainder to 0!
	  remainder=0;
	}
	// the remaining payload fits, but not the fed trailer
	else if (payload>=(remainder-fedTrailerSize_)) {
	  memcpy(startOfFedBlocks,
		 evt->fedAddr(iFed)+evt->fedSize(iFed)-remainder,
		 remainder-fedTrailerSize_);
	  
	  frlHeader->segsize=remainder-fedTrailerSize_;
	  fedTrailerLeft    =true;
	  leftspace        -=(remainder-fedTrailerSize_);
	  remainder         =fedTrailerSize_;
	}
	// the remaining payload fits only partially, fill whole block
	else {
	  memcpy(startOfFedBlocks,
		 evt->fedAddr(iFed)+evt->fedSize(iFed)-remainder,payload);
	  remainder-=payload;
	  leftspace =0;
	}
      }
      
      //!
      //! no remaining fed data
      //!
      if(remainder==0) {
	
	// loop on feds
	while(iFed<nFed) {
	  
	  // if the next header does not fit, jump to following block
	  if((int)leftspace<fedHeaderSize_) {
	    frlHeader->segsize-=leftspace;
	    break;
	  }
	  
	  memcpy(startOfFedBlocks,evt->fedAddr(iFed),fedHeaderSize_);
	  
	  leftspace       -=fedHeaderSize_;
	  startOfFedBlocks+=fedHeaderSize_;
	  
	  // fed fits with its trailer
	  if(evt->fedSize(iFed)-fedHeaderSize_<=leftspace) {
	    memcpy(startOfFedBlocks,
		   evt->fedAddr(iFed)+fedHeaderSize_,
		   evt->fedSize(iFed)-fedHeaderSize_);
	    
	    leftspace       -=(evt->fedSize(iFed)-fedHeaderSize_);
	    startOfFedBlocks+=(evt->fedSize(iFed)-fedHeaderSize_);
	  }
	  // fed payload fits only without fed trailer
	  else if(evt->fedSize(iFed)-fedHeaderSize_-fedTrailerSize_<=leftspace) {
	    memcpy(startOfFedBlocks,
		   evt->fedAddr(iFed)+fedHeaderSize_,
		   evt->fedSize(iFed)-fedHeaderSize_-fedTrailerSize_);
	    
	    leftspace         -=(evt->fedSize(iFed)-fedHeaderSize_-fedTrailerSize_);
	    frlHeader->segsize-=leftspace;
	    fedTrailerLeft     =true;
	    remainder          =fedTrailerSize_;
	    
	    break;
	  }
	  // fed payload fits only partially
	  else {
	    memcpy(startOfFedBlocks,evt->fedAddr(iFed)+fedHeaderSize_,leftspace);
	    remainder=evt->fedSize(iFed)-fedHeaderSize_-leftspace;
	    leftspace=0;
	    
	    break;
	  }
	  
	  // !! increase iFed !!
	  iFed++;
	  
	} // while (iFed<fedN_)
	
	// earmark the last block
	if (iFed==nFed && remainder==0 && !last) {
	  frlHeader->segsize-=leftspace;
	  int msgSize=stdMsg->MessageSize << 2;
	  msgSize   -=leftspace;
	  bufRef->setDataSize(msgSize);
	  stdMsg->MessageSize=msgSize >> 2;
	  frlHeader->segsize =frlHeader->segsize | FRL_LAST_SEGM;
	  last=true;
	}
	
      } // if (remainder==0)
      
      if(iSuperFrag==0&&iBlock==0) { // This is the first fragment block / message
	head=bufRef;
	tail=bufRef;
      }
      else {
	tail->setNextReference(bufRef);
	tail=bufRef;
      }
      
      if((iBlock==nBlock-1) && remainder!=0) {
	nBlock++;
	warning=true;
      }
      
    } // for (iBlock)
    
    // fix case where block estimate was wrong
    if(warning) {
      toolbox::mem::Reference* next=head;
      do {
	block =(I2O_EVENT_DATA_BLOCK_MESSAGE_FRAME*)next->getDataLocation();
	if (block->superFragmentNb==iSuperFrag)
	  block->nbBlocksInSuperFragment=nBlock;		
      } while((next=next->getNextReference()));
    }
  
  } // iSuperFrag < nSuperFrag
  
  return head; // return the top of the chain
}

//______________________________________________________________________________
void BU::dumpFrame(unsigned char* data,unsigned int len)
{
  char left1[20];
  char left2[20];
  char right1[20];
  char right2[20];

  printf("Byte  0  1  2  3  4  5  6  7\n");
  
  int c(0);
  int pos(0);
  
  for (unsigned int i=0;i<(len/8);i++) {
    int rpos(0);
    int off(3);
    for (pos=0;pos<12;pos+=3) {
      sprintf(&left1[pos],"%2.2x ",
	      ((unsigned char*)data)[c+off]);
      sprintf(&right1[rpos],"%1c",
	      ((data[c+off] > 32)&&(data[c+off] < 127)) ? data[c+off] : '.');
      sprintf (&left2[pos],"%2.2x ",
	       ((unsigned char*)data)[c+off+4]);
      sprintf (&right2[rpos],"%1c",
	       ((data[c+off+4] > 32)&&(data[c+off+4]<127)) ? data[c+off+4] : '.');
      rpos++;
      off--;
    }
    c+=8;
    
    printf ("%4d: %s%s ||  %s%s  %x\n",
	    c-8, left1, left2, right1, right2, (int)&data[c-8]);
  }
  
  fflush(stdout);	
}



////////////////////////////////////////////////////////////////////////////////
// xdaq instantiator implementation macro
////////////////////////////////////////////////////////////////////////////////

XDAQ_INSTANTIATOR_IMPL(BU)

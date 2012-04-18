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
#include "EventFilter/AutoBU/interface/BUMessageCutter.h"
#include "EventFilter/AutoBU/interface/SoapUtils.h"
#include "EventFilter/Utilities/interface/IndependentWebGUI.h"
#include "EventFilter/AutoBU/interface/ABUConfig.h"

#include "xoap/SOAPEnvelope.h"
#include "xoap/SOAPBody.h"
#include "xoap/domutils.h"
#include "xoap/Method.h"
#include "xdaq/NamespaceURI.h"

#include <netinet/in.h>
#include <sstream>
#include <errno.h>
#include <typeinfo>

using std::string;
using std::cout;
using std::endl;
using namespace evf;
using namespace evf::autobu_statemachine;

////////////////////////////////////////////////////////////////////////////////
// construction/destruction
////////////////////////////////////////////////////////////////////////////////

//______________________________________________________________________________

BU::BU(xdaq::ApplicationStub *s) :
	xdaq::Application(s), resources_(new SharedResources()), instance_(0),
			runNumber_(0), memUsedInMB_(0.0), eventBufferSize_(0x400000),
			initialSFBuffer_(INITIAL_SUPERFRAGMENT_SIZE_BYTES) {

	bindStateMachineCallbacks();

	resources_->gui_ = new IndependentWebGUI(this);
	resources_->gui_->setVersionString("Changeset: 06.04.2012->V1.08(rework)");

	initialiseSharedResources(resources_);

	// create state machine with shared resources
	fsm_.reset(new BStateMachine(this, resources_));

	// initialise state machine
	fsm_->initiate();

	// initialise event queue semaphore
	// before starting event processing workloop
	resources_->initEvQSem();

	//start event processing workloop
	startEventProcessingWorkloop();

	// initialise application info
	url_ = getApplicationDescriptor()->getContextDescriptor()->getURL() + "/"
			+ getApplicationDescriptor()->getURN();
	class_ = getApplicationDescriptor()->getClassName();
	instance_ = getApplicationDescriptor()->getInstance();
	hostname_ = getApplicationDescriptor()->getContextDescriptor()->getURL();

	// i2o callbacks
	i2o::bind(this, &BU::I2O_BU_ALLOCATE_Callback, I2O_BU_ALLOCATE,
			XDAQ_ORGANIZATION_ID);
	i2o::bind(this, &BU::I2O_BU_DISCARD_Callback, I2O_BU_DISCARD,
			XDAQ_ORGANIZATION_ID);

	// web interface
	xgi::bind(this, &evf::BU::webPageRequest, "Default");
	resources_->gui_->setSmallAppIcon("/rubuilder/bu/images/bu32x32.gif");
	resources_->gui_->setLargeAppIcon("/rubuilder/bu/images/bu64x64.gif");

	vector<toolbox::lang::Method*> methods = resources_->gui_->getMethods();
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

	fsm_->findRcmsStateListener(this);

}

//______________________________________________________________________________

BU::~BU() {
	//delete fsm_;
	//delete resources_;
}
////////////////////////////////////////////////////////////////////////////////
// implementation of member functions
////////////////////////////////////////////////////////////////////////////////

//______________________________________________________________________________

void BU::initialiseSharedResources(SharedResourcesPtr res) {
	res->bu_ = this;
	res->sourceId_ = class_.toString() + instance_.toString();
	res->log_ = getApplicationLogger();
	res->buAppDesc_ = getApplicationDescriptor();
	res->fuAppDesc_ = 0;
	res->evtNumber_ = 0;
	res->wlExecuting_ = 0;
	res->asExecuting_ = 0;
	res->taskQueueSize_ = 0;
	res->readyToSendQueueSize_ = 0;
	res->avgTaskQueueSize_ = 0;
	res->nbEventsInBU_ = 0;
	res->nbEventsRequested_ = 0;
	res->nbEventsBuilt_ = 0;
	res->nbEventsSent_ = 0;
	res->nbEventsDiscarded_ = 0;
	res->mode_ = "RANDOM";
	res->replay_ = false;
	res->overwriteEvtId_ = true;
	res->firstEvent_ = 1;
	res->queueSize_ = 256;
	res->msgChainCreationMode_ = "NORMAL";
	res->msgBufferSize_ = 32768;
	res->fedSizeMax_ = 65536;
	res->fedSizeMean_ = 16;
	res->fedSizeWidth_ = 1024;

	// allocate i2o memory pool
	string i2oPoolName = res->sourceId_ + "_i2oPool";
	try {
		toolbox::mem::HeapAllocator *allocator =
				new toolbox::mem::HeapAllocator();
		toolbox::net::URN urn("toolbox-mem-pool", i2oPoolName);
		toolbox::mem::MemoryPoolFactory* poolFactory =
				toolbox::mem::getMemoryPoolFactory();
		res->i2oPool_ = poolFactory->createPool(urn, allocator);
	} catch (toolbox::mem::exception::Exception& e) {
		string s = "Failed to create pool: " + i2oPoolName;
		LOG4CPLUS_FATAL(res->log_, s);
		XCEPT_RETHROW(xcept::Exception, s, e);
	}

	res->initialiseQSizes();
}

//______________________________________________________________________________

void BU::bindStateMachineCallbacks() {
	xoap::bind(this, &BU::handleFSMSoapMessage, "Configure", XDAQ_NS_URI);
	xoap::bind(this, &BU::handleFSMSoapMessage, "Enable", XDAQ_NS_URI);
	xoap::bind(this, &BU::handleFSMSoapMessage, "Stop", XDAQ_NS_URI);
	xoap::bind(this, &BU::handleFSMSoapMessage, "Halt", XDAQ_NS_URI);
}

///////////////////////////////////////
// State Machine call back functions //
///////////////////////////////////////
//______________________________________________________________________________

xoap::MessageReference BU::handleFSMSoapMessage(xoap::MessageReference msg)
		throw (xoap::exception::Exception) {

	string errorMsg;
	xoap::MessageReference returnMsg;

	try {
		errorMsg
				= "Failed to extract FSM event and parameters from SOAP message: ";
		string command = soaputils::extractParameters(msg, this);

		errorMsg = "Failed to put a '" + command
				+ "' state machine event into command queue: ";
		if (command == "Configure") {

			EventPtr stMachEvent(new Configure());
			resources_->enqEvent(stMachEvent);

		} else if (command == "Enable") {

			EventPtr stMachEvent(new Enable());
			resources_->enqEvent(stMachEvent);

		} else if (command == "Stop") {

			EventPtr stMachEvent(new Stop());
			resources_->enqEvent(stMachEvent);

		} else if (command == "Halt") {

			EventPtr stMachEvent(new Halt());
			resources_->enqEvent(stMachEvent);
		}

		else {
			XCEPT_RAISE(
					xcept::Exception,
					"Received an unknown state machine event '" + command
							+ "'.");
		}

		errorMsg = "Failed to create FSM SOAP reply message: ";
		::usleep(50000);
		returnMsg = soaputils::createFsmSoapResponseMsg(command,
				fsm_->getExternallyVisibleState());

	} catch (xcept::Exception& e) {
		string s = "Exception on FSM Callback!";
		LOG4CPLUS_FATAL(resources_->log_, s);
		XCEPT_RETHROW(xcept::Exception, s, e);
	}

	return returnMsg;
}

//______________________________________________________________________________

void BU::I2O_BU_ALLOCATE_Callback(toolbox::mem::Reference *bufRef) {
	try {
		handleI2OAllocate(bufRef);
	} catch (xcept::Exception& e) {
		string s = "Exception in I2O_BU_ALLOCATE_Callback: "
				+ (string) e.what();
		LOG4CPLUS_FATAL(resources_->log_, s);
		XCEPT_RETHROW(xcept::Exception, s, e);
	} catch (...) {
		LOG4CPLUS_FATAL(resources_->log_,
				"Exception in I2O_BU_ALLOCATE_Callback");
	}
}

//______________________________________________________________________________

void BU::I2O_BU_DISCARD_Callback(toolbox::mem::Reference *bufRef) {
	try {
		handleI2ODiscard(bufRef);
	} catch (xcept::Exception& e) {
		string s = "Exception in I2O_BU_DISCARD_Callback: " + (string) e.what();
		LOG4CPLUS_FATAL(resources_->log_, s);
		XCEPT_RETHROW(xcept::Exception, s, e);
	} catch (...) {
		LOG4CPLUS_FATAL(resources_->log_,
				"Exception in I2O_BU_DISCARD_Callback");
	}
}

//______________________________________________________________________________

void BU::actionPerformed(xdata::Event& e) {

	resources_->gui_->monInfoSpace()->lock();
	if (e.type() == "urn:xdata-event:ItemGroupRetrieveEvent") {
		if (0 != PlaybackRawDataProvider::instance())
			resources_->mode_ = "PLAYBACK";

		if (0 != resources_->i2oPool_)
			memUsedInMB_ = resources_->i2oPool_->getMemoryUsage().getUsed()
					* 9.53674e-07;
		else
			memUsedInMB_ = 0.0;
	}
	resources_->gui_->monInfoSpace()->unlock();
}

//______________________________________________________________________________

void BU::webPageRequest(xgi::Input *in, xgi::Output *out)
		throw (xgi::exception::Exception) {
	string name = in->getenv("PATH_INFO");
	if (name.empty())
		name = "defaultWebPage";
	static_cast<xgi::MethodSignature*> (resources_->gui_->getMethod(name))->invoke(
			in, out);
}

//______________________________________________________________________________

void BU::customWebPage(xgi::Input*in, xgi::Output*out)
		throw (xgi::exception::Exception) {
	*out << "<html></html>" << endl;
}

//______________________________________________________________________________

void BU::startEventProcessingWorkloop() throw (evf::Exception) {

	try {
		LOG4CPLUS_INFO(resources_->log_, "Start 'EVENT PROCESSING' workloop");
		wlProcessingEvents_ = toolbox::task::getWorkLoopFactory()->getWorkLoop(
				resources_->sourceId_ + "Processing Events", "waiting");
		if (!wlProcessingEvents_->isActive())
			wlProcessingEvents_->activate();
		asProcessingEvents_ = toolbox::task::bind(this, &BU::processing,
				resources_->sourceId_ + "Processing");
		wlProcessingEvents_->submit(asProcessingEvents_);
	} catch (xcept::Exception& e) {
		string msg = "Failed to start workloop 'EVENT PROCESSING'.";
		XCEPT_RETHROW(evf::Exception, msg, e);
	}
}

//______________________________________________________________________________

bool BU::processing(toolbox::task::WorkLoop* wl) {
	EventPtr topEvent = resources_->deqEvent();

	if (topEvent != 0) {

		string type(typeid(*topEvent).name());

		fsm_->process_event(*topEvent);
		try {
			fsm_->getCurrentState().do_stateAction();
		} catch (std::bad_cast) {
			cout
					<< "AutoBU state machine: STATE SPECIFIC ACTION: NOT CONSTRUCTED!"
					<< endl;
			//::sleep(1);
		}
	}

	else {
		// improve semaphore here
		::sleep(1);
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

void BU::exportParameters() {
	SharedResourcesPtr res = fsm_->getSharedResources();
	if (0 == resources_->gui_) {
		LOG4CPLUS_ERROR(res->log_, "No GUI, can't export parameters");
		return;
	}

	resources_->gui_->addMonitorParam("url", &url_);
	resources_->gui_->addMonitorParam("class", &class_);
	resources_->gui_->addMonitorParam("instance", &instance_);
	resources_->gui_->addMonitorParam("hostname", &hostname_);
	resources_->gui_->addMonitorParam("runNumber", &runNumber_);
	resources_->gui_->addMonitorParam("stateName",
			fsm_->getExternallyVisibleStatePtr());
	resources_->gui_->addMonitorParam("memUsedInMB", &memUsedInMB_);

	resources_->gui_->addMonitorCounter("taskQueueSize", &res->taskQueueSize_);
	resources_->gui_->addMonitorCounter("readyToSendQueueSize",
			&res->readyToSendQueueSize_);
	resources_->gui_->addMonitorCounter("avgTaskQueueSize",
			&res->avgTaskQueueSize_);
	resources_->gui_->addMonitorCounter("nbEvtsInBU", &res->nbEventsInBU_);
	resources_->gui_->addMonitorCounter("nbEvtsRequested",
			&res->nbEventsRequested_);
	resources_->gui_->addMonitorCounter("nbEvtsBuilt", &res->nbEventsBuilt_);
	resources_->gui_->addMonitorCounter("nbEvtsSent", &res->nbEventsSent_);
	resources_->gui_->addMonitorCounter("nbEvtsDiscarded",
			&res->nbEventsDiscarded_);

	resources_->gui_->addStandardParam("mode", &res->mode_);
	resources_->gui_->addStandardParam("replay", &res->replay_);
	resources_->gui_->addStandardParam("overwriteEvtId", &res->overwriteEvtId_);
	resources_->gui_->addStandardParam("crc", &res->crc_);
	resources_->gui_->addStandardParam("firstEvent", &res->firstEvent_);
	resources_->gui_->addStandardParam("queueSize", &res->queueSize_);
	resources_->gui_->addStandardParam("eventBufferSize", &eventBufferSize_);
	resources_->gui_->addStandardParam("msgBufferSize", &res->msgBufferSize_);
	resources_->gui_->addStandardParam("fedSizeMax", &res->fedSizeMax_);
	resources_->gui_->addStandardParam("fedSizeMean", &res->fedSizeMean_);
	resources_->gui_->addStandardParam("fedSizeWidth", &res->fedSizeWidth_);
	resources_->gui_->addStandardParam("useFixedFedSize",
			&res->useFixedFedSize_);
	resources_->gui_->addStandardParam("initialSuperFragmentSize",
			&initialSFBuffer_);
	resources_->gui_->addStandardParam("msgChainCreationMode",
			&res->msgChainCreationMode_);

	resources_->gui_->addStandardParam("rcmsStateListener",
			fsm_->rcmsStateListener());
	resources_->gui_->addStandardParam("foundRcmsStateListener",
			fsm_->foundRcmsStateListener());

	resources_->gui_->exportParameters();

}

//______________________________________________________________________________

void BU::handleI2OAllocate(toolbox::mem::Reference *bufRef) const {

	I2O_MESSAGE_FRAME *stdMsg;
	I2O_BU_ALLOCATE_MESSAGE_FRAME *msg;

	stdMsg = (I2O_MESSAGE_FRAME*) bufRef->getDataLocation();
	msg = (I2O_BU_ALLOCATE_MESSAGE_FRAME*) stdMsg;

	if (0 == resources_->fuAppDesc_) {
		I2O_TID fuTid = stdMsg->InitiatorAddress;
		resources_->fuAppDesc_
				= i2o::utils::getAddressMap()->getApplicationDescriptor(fuTid);
	}

	for (unsigned int i = 0; i < msg->n; i++) {
		unsigned int fuResourceId = msg->allocate[i].fuTransactionId;

		resources_->lock();
		// TASK
		resources_->pushTask('r');
		resources_->rqstIds_.push(fuResourceId);
		resources_->nbEventsRequested_++;
		resources_->nbEventsInBU_++;
		resources_->unlock();
	}

	bufRef->release();
}

//______________________________________________________________________________

void BU::handleI2ODiscard(toolbox::mem::Reference *bufRef) const {

	I2O_MESSAGE_FRAME *stdMsg = (I2O_MESSAGE_FRAME*) bufRef->getDataLocation();
	I2O_BU_DISCARD_MESSAGE_FRAME*msg = (I2O_BU_DISCARD_MESSAGE_FRAME*) stdMsg;
	unsigned int buResourceId = msg->buResourceId[0];

	resources_->lock();
	int result = resources_->sentIds_.erase(buResourceId);
	resources_->unlock();

	if (!result) {
		LOG4CPLUS_ERROR(resources_->log_,
				"can't discard unknown buResourceId '" << buResourceId << "'");
	} else {
		resources_->lock();
		resources_->freeIds_.push(buResourceId);
		resources_->nbEventsDiscarded_.value_++;
		resources_->unlock();

		// TASK
		resources_->pushTask('b');
	}

	bufRef->release();
}

////////////////////////////////////////////////////////////////////////////////
// xdaq instantiator implementation macro
////////////////////////////////////////////////////////////////////////////////

XDAQ_INSTANTIATOR_IMPL(BU)

////////////////////////////////////////////////////////////////////////////////
//
// BU
// --
//
//                                         Emilio Meschi <emilio.meschi@cern.ch>
//                       Philipp Schieferdecker <philipp.schieferdecker@cern.ch>
//							    Andrei Spataru <andrei.cristian.spataru@cern.ch>
////////////////////////////////////////////////////////////////////////////////

//TODO replace do_stateName with stateName()

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
	resources_->gui_->setVersionString("Last modified: 22.02.2012");

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
	res->setBu(this);
	res->setSourceId(class_.toString() + instance_.toString());
	res->setLogger(getApplicationLogger());
	res->setBuAppDesc(getApplicationDescriptor());
	res->setFuAppDesc(0);
	res->eventNumber() = 0;
	res->setWlExecuting(0);
	res->setAsExecuting(0);
	res->taskQueueSize() = 0;
	res->rtsQSize() = 0;
	res->avgTaskQueueSize() = 0;
	res->nbEventsInBU() = 0;
	res->nbEventsRequested() = 0;
	res->nbEventsBuilt() = 0;
	res->nbEventsSent() = 0;
	res->nbEventsDiscarded() = 0;
	res->mode() = "RANDOM-SIMPLE";
	res->replay() = false;
	res->overwriteEvtId() = true;
	res->firstEvent() = 1;
	res->queueSize() = 256;
	res->msgChainCreationMode() = "NORMAL";
	res->msgBufferSize() = 32768;
	res->fedSizeMax() = 65536;
	res->fedSizeMean() = 16;
	res->fedSizeWidth() = 1024;

	// allocate i2o memory pool
	string i2oPoolName = res->sourceId() + "_i2oPool";
	try {
		toolbox::mem::HeapAllocator *allocator =
				new toolbox::mem::HeapAllocator();
		toolbox::net::URN urn("toolbox-mem-pool", i2oPoolName);
		toolbox::mem::MemoryPoolFactory* poolFactory =
				toolbox::mem::getMemoryPoolFactory();
		res->setI2OPool(poolFactory->createPool(urn, allocator));
	} catch (toolbox::mem::exception::Exception& e) {
		string s = "Failed to create pool: " + i2oPoolName;
		LOG4CPLUS_FATAL(res->logger(), s);
		XCEPT_RETHROW(xcept::Exception, s, e);
	}

	res->initialiseQSizes();
	res->restarted() = false;
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

			// UPDATE: BU:stop = halt + configure
			/*
			 EventPtr stMachEvent(new Stop());
			 resources_->enqEvent(stMachEvent);
			 */
			EventPtr halt(new Halt());
			resources_->enqEvent(halt);

			while (fsm_->getExternallyVisibleState().compare("Halted") != 0)
				::sleep(1);

			EventPtr configure(new Configure());
			resources_->enqEvent(configure);

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
		LOG4CPLUS_FATAL(resources_->logger(), s);
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
		LOG4CPLUS_FATAL(resources_->logger(), s);
		XCEPT_RETHROW(xcept::Exception, s, e);
	} catch (...) {
		LOG4CPLUS_FATAL(resources_->logger(),
				"Exception in I2O_BU_ALLOCATE_Callback");
	}
}

//______________________________________________________________________________

void BU::I2O_BU_DISCARD_Callback(toolbox::mem::Reference *bufRef) {
	try {
		handleI2ODiscard(bufRef);
	} catch (xcept::Exception& e) {
		string s = "Exception in I2O_BU_DISCARD_Callback: " + (string) e.what();
		LOG4CPLUS_FATAL(resources_->logger(), s);
		XCEPT_RETHROW(xcept::Exception, s, e);
	} catch (...) {
		LOG4CPLUS_FATAL(resources_->logger(),
				"Exception in I2O_BU_DISCARD_Callback");
	}
}

//______________________________________________________________________________

void BU::actionPerformed(xdata::Event& e) {

	resources_->gui_->monInfoSpace()->lock();
	if (e.type() == "urn:xdata-event:ItemGroupRetrieveEvent") {
		if (0 != PlaybackRawDataProvider::instance())
			resources_->mode() = "PLAYBACK";

		if (0 != resources_->i2oPool())
			memUsedInMB_ = resources_->i2oPool()->getMemoryUsage().getUsed()
					* 9.53674e-07;
		else
			memUsedInMB_ = 0.0;
	} else if (e.type() == "ItemChangedEvent") {
		string item = dynamic_cast<xdata::ItemChangedEvent&> (e).itemName();
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
		LOG4CPLUS_INFO(resources_->logger(),
				"Start 'EVENT PROCESSING' workloop");
		wlProcessingEvents_ = toolbox::task::getWorkLoopFactory()->getWorkLoop(
				resources_->sourceId() + "Processing Events", "waiting");
		if (!wlProcessingEvents_->isActive())
			wlProcessingEvents_->activate();
		asProcessingEvents_ = toolbox::task::bind(this, &BU::processing,
				resources_->sourceId() + "Processing");
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
		string procMsg = "AutoBU-->Processing event: " + type;

		LOG4CPLUS_INFO(resources_->logger(), procMsg);

		// if event is ConfigureDone or HaltDone, also reset gui counters
		size_t foundC, foundH;
		foundC = type.rfind("ConfigureDone");
		foundH = type.rfind("HaltDone");

		if (foundC != string::npos || foundH != string::npos) {
			LOG4CPLUS_INFO(resources_->logger(), "Resetting counters");
			resources_->gui_->resetCounters();
		}

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
		LOG4CPLUS_ERROR(res->logger(), "No GUI, can't export parameters");
		return;
	}

	resources_->gui_->addMonitorParam("url", &url_);
	resources_->gui_->addMonitorParam("class", &class_);
	resources_->gui_->addMonitorParam("instance", &instance_);
	resources_->gui_->addMonitorParam("hostname", &hostname_);
	resources_->gui_->addMonitorParam("runNumber", &runNumber_);
	resources_->gui_->addMonitorParam("stateName", fsm_->getExternallyVisibleStatePtr());
	resources_->gui_->addMonitorParam("memUsedInMB", &memUsedInMB_);

	resources_->gui_->addMonitorCounter("taskQueueSize", &res->taskQueueSize());
	resources_->gui_->addMonitorCounter("readyToSendQueueSize",
			&res->rtsQSize());
	resources_->gui_->addMonitorCounter("avgTaskQueueSize",
			&res->avgTaskQueueSize());
	resources_->gui_->addMonitorCounter("nbEvtsInBU", &res->nbEventsInBU());
	resources_->gui_->addMonitorCounter("nbEvtsRequested",
			&res->nbEventsRequested());
	resources_->gui_->addMonitorCounter("nbEvtsBuilt", &res->nbEventsBuilt());
	resources_->gui_->addMonitorCounter("nbEvtsSent", &res->nbEventsSent());
	resources_->gui_->addMonitorCounter("nbEvtsDiscarded",
			&res->nbEventsDiscarded());

	resources_->gui_->addStandardParam("mode", &res->mode());
	resources_->gui_->addStandardParam("replay", &res->replay());
	resources_->gui_->addStandardParam("overwriteEvtId", &res->overwriteEvtId());
	resources_->gui_->addStandardParam("firstEvent", &res->firstEvent());
	resources_->gui_->addStandardParam("queueSize", &res->queueSize());
	resources_->gui_->addStandardParam("eventBufferSize", &eventBufferSize_);
	resources_->gui_->addStandardParam("msgBufferSize", &res->msgBufferSize());
	resources_->gui_->addStandardParam("fedSizeMax", &res->fedSizeMax());
	resources_->gui_->addStandardParam("fedSizeMean", &res->fedSizeMean());
	resources_->gui_->addStandardParam("fedSizeWidth", &res->fedSizeWidth());
	resources_->gui_->addStandardParam("initialSuperFragmentSize",
			&initialSFBuffer_);
	resources_->gui_->addStandardParam("msgChainCreationMode",
			&res->msgChainCreationMode());

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

	if (0 == resources_->fuAppDesc()) {
		I2O_TID fuTid = stdMsg->InitiatorAddress;
		resources_->setFuAppDesc(
				i2o::utils::getAddressMap()->getApplicationDescriptor(fuTid));
	}

	for (unsigned int i = 0; i < msg->n; i++) {
		unsigned int fuResourceId = msg->allocate[i].fuTransactionId;

		// TASK
		resources_->pushTask('r');

		resources_->lock();
		resources_->rqstIds()->push(fuResourceId);
		resources_->increaseEventsRequested();
		resources_->increaseEventsInBU();
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
	int result = resources_->sentIds()->erase(buResourceId);
	resources_->unlock();

	if (!result) {
		LOG4CPLUS_ERROR(resources_->logger(),
				"can't discard unknown buResourceId '" << buResourceId << "'");
	} else {
		resources_->lock();
		resources_->freeIds()->push(buResourceId);
		resources_->increaseEventsDiscarded();
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

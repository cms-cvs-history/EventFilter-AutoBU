/** \class BU
 *
 * 	Application that emulates the CMS Builder Unit.
 * 	Generates events and sends them to the Filter Unit.
 */

#ifndef AUTOBU_BU_H
#define AUTOBU_BU_H 1

#include "EventFilter/AutoBU/interface/BStateMachine.h"
#include "EventFilter/AutoBU/interface/SharedResources.h"

#include "EventFilter/Utilities/interface/Exception.h"
#include "EventFilter/Playback/interface/PlaybackRawDataProvider.h"

#include "xdaq/Application.h"
#include "interface/evb/i2oEVBMsgs.h"
#include "interface/shared/i2oXFunctionCodes.h"
#include "i2o/Method.h"
#include "i2o/utils/AddressMap.h"

#include <vector>
#include <cmath>
#include <semaphore.h>
#include <sys/time.h>

namespace evf {

class BU: public xdaq::Application, public xdata::ActionListener {
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

	// fsm soap command callback
	xoap::MessageReference handleFSMSoapMessage(xoap::MessageReference msg)
			throw (xoap::exception::Exception);

	// i2o callbacks
	void I2O_BU_ALLOCATE_Callback(toolbox::mem::Reference *bufRef);
	void I2O_BU_DISCARD_Callback(toolbox::mem::Reference *bufRef);

	// I2O & Direct Call handling
	void handleI2OAllocate(toolbox::mem::Reference *bufRef) const;
	void handleI2ODiscard(toolbox::mem::Reference *bufRef) const;

	// workloop / action signature for event processing
	toolbox::task::WorkLoop *wlProcessingEvents_;
	toolbox::task::ActionSignature *asProcessingEvents_;

	// xdata::ActionListener callback
	void actionPerformed(xdata::Event& e);

	// Hyper DAQ web interface [see Utilities/WebGUI]
	void webPageRequest(xgi::Input *in, xgi::Output *out)
			throw (xgi::exception::Exception);
	void customWebPage(xgi::Input*in, xgi::Output*out)
			throw (xgi::exception::Exception);

	// event workloop
	void startEventProcessingWorkloop() throw (evf::Exception);
	bool processing(toolbox::task::WorkLoop* wl);

	void postI2OFrame(xdaq::ApplicationDescriptor* fuAppDesc,
			toolbox::mem::Reference* bufRef);

private:
	//
	// private member functions
	//
	void initialiseSharedResources(autobu_statemachine::SharedResourcesPtr res);
	void exportParameters();

	void bindStateMachineCallbacks();

private:
	//
	// member data
	//

	// Shared Resources
	autobu_statemachine::SharedResourcesPtr resources_;

	// BU state machine
	boost::shared_ptr<autobu_statemachine::BStateMachine> fsm_;

	// monitored parameters
	xdata::String url_;
	xdata::String class_;
	xdata::UnsignedInteger32 instance_;
	xdata::String hostname_;
	xdata::UnsignedInteger32 runNumber_;
	xdata::Double memUsedInMB_;

	// standard parameters
	xdata::UnsignedInteger32 eventBufferSize_;
	xdata::UnsignedInteger32 initialSFBuffer_;

}; // class BU

} // namespace evf

#endif

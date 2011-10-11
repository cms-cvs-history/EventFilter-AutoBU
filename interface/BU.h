/** \class BU
 *
 * 	Application that emulates the CMS Builder Unit.
 * 	Generates events and sends them to the Filter Unit.
 */

#ifndef AUTOBU_BU_H
#define AUTOBU_BU_H 1

#include "EventFilter/BUFUInterface/interface/BaseBU.h"
#include "EventFilter/BUFUInterface/interface/BUFUInterface.h"
#include "EventFilter/AutoBU/interface/BStateMachine.h"
#include "EventFilter/AutoBU/interface/SharedResources.h"
#include "EventFilter/AutoBU/interface/WebGUI2.h"

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

class BU: public xdaq::Application, public xdata::ActionListener, public BaseBU {
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

	// direct calls
	void DIRECT_BU_ALLOCATE(const UIntVec_t& fuResourceIds,
			xdaq::ApplicationDescriptor* fuAppDesc);
	void DIRECT_BU_DISCARD(UInt_t buResourceId);

	// I2O & Direct Call handling
	void handleI2OAllocate(toolbox::mem::Reference *bufRef) const;
	void handleI2ODiscard(toolbox::mem::Reference *bufRef) const;
	void handleDirectAllocate(const UIntVec_t& fuResourceIds,
			xdaq::ApplicationDescriptor* fuAppDesc) const;
	void handleDirectDiscard(UInt_t buResourceId) const;

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

	// BUFU
	void postI2OFrame(xdaq::ApplicationDescriptor* fuAppDesc,
			toolbox::mem::Reference* bufRef);

private:
	//
	// private member functions
	//
	void initialiseSharedResources(SharedResourcesPtr res);
	void exportParameters();

	void bindStateMachineCallbacks();

private:
	//
	// member data
	//

	// Shared Resources
	SharedResourcesPtr resources_;

	// BU state machine
	boost::shared_ptr<BStateMachine> fsm_;

	// BU web interface
	// modified to work with boost::statechart
	WebGUI2 *gui_;

	std::vector<evf::SuperFragment*> sFragments_;

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

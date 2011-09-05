/** \class BUMessageCutter
 *  Creates a message chain using an event.
 *  Used by the Builder Unit to send event data to
 *  the Resource Broker.
 *
 *  \author A. Spataru
 */

#ifndef MESSAGECUTTER_H_
#define MESSAGECUTTER_H_

#include "EventFilter/AutoBU/interface/ABUEvent.h"

#include "toolbox/mem/HeapAllocator.h"
#include "toolbox/mem/MemoryPoolFactory.h"
#include "toolbox/net/URN.h"
#include "toolbox/fsm/exception/Exception.h"

#include "xdata/UnsignedInteger32.h"
#include "xdaq/Application.h"

using namespace evf;

class BUMessageCutter {

public:
	/**
	 * Constructs a message cutter object using the I2O message
	 * buffer size, a pointer to the application logger,
	 * builder unit application descriptor, filter unit application descriptor,
	 * and a pointer to the I2O memory pool.
	 */
			BUMessageCutter(xdata::UnsignedInteger32 msgBufferSize,
					Logger* log, xdaq::ApplicationDescriptor* buAppDesc,
					xdaq::ApplicationDescriptor* fuAppDesc,
					toolbox::mem::Pool* i2oPool);
	/**
	 * Creates a message chain without splitting FED frames
	 * into different I2O message blocks. Does not work for FED frames
	 * larger than the I2O message payload size.
	 */
	toolbox::mem::Reference* createMessageChainSimple(ABUEvent* evt,
			unsigned int fuResourceId);
	/**
	 * Creates a message chain for all possible situations.
	 */
	toolbox::mem::Reference* createMessageChain(ABUEvent* evt,
			unsigned int fuResourceId);

private:
	xdata::UnsignedInteger32 msgBufferSize_;
	Logger* log_;
	xdaq::ApplicationDescriptor* buAppDesc_;
	xdaq::ApplicationDescriptor* fuAppDesc_;
	toolbox::mem::Pool* i2oPool_;

	//
	// static member data
	//
	static const int frlHeaderSize_ = sizeof(frlh_t);
	static const int fedHeaderSize_ = sizeof(fedh_t);
	static const int fedTrailerSize_ = sizeof(fedt_t);
};

#endif /* MESSAGECUTTER_H_ */

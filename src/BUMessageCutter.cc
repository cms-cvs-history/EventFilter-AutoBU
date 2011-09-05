/*
 * MessageCutter.cc
 *
 *  Created on: Jul 28, 2011
 *      Author: aspataru
 */

#include "EventFilter/AutoBU/interface/BUMessageCutter.h"

#include "DataFormats/FEDRawData/interface/FEDNumbering.h"
#include "DataFormats/FEDRawData/interface/FEDRawDataCollection.h"

#include "xoap/SOAPEnvelope.h"
#include "xoap/SOAPBody.h"
#include "xoap/domutils.h"

#include "i2o/Method.h"
#include "i2o/utils/AddressMap.h"

#include "interface/evb/i2oEVBMsgs.h"
#include "interface/shared/i2oXFunctionCodes.h"

//#define DEBUG_CUT


BUMessageCutter::BUMessageCutter(xdata::UnsignedInteger32 msgBufferSize,
		Logger* log, xdaq::ApplicationDescriptor* buAppDesc,
		xdaq::ApplicationDescriptor* fuAppDesc, toolbox::mem::Pool* i2oPool) :
	msgBufferSize_(msgBufferSize), log_(log), buAppDesc_(buAppDesc),
			fuAppDesc_(fuAppDesc), i2oPool_(i2oPool) {

}

toolbox::mem::Reference* BUMessageCutter::createMessageChainSimple(
		ABUEvent* evt, unsigned int fuResourceId) {
	unsigned int msgHeaderSize = sizeof(I2O_EVENT_DATA_BLOCK_MESSAGE_FRAME);
	unsigned int msgPayloadSize = msgBufferSize_ - msgHeaderSize;

	if ((msgPayloadSize % 4) != 0)
		LOG4CPLUS_ERROR(*log_, "Invalid Payload Size.");

	toolbox::mem::Reference *head = 0;
	toolbox::mem::Reference *tail = 0;
	toolbox::mem::Reference *bufRef = 0;

	I2O_MESSAGE_FRAME *stdMsg = 0;
	I2O_PRIVATE_MESSAGE_FRAME *pvtMsg = 0;
	I2O_EVENT_DATA_BLOCK_MESSAGE_FRAME *block = 0;

	// ??? getUsedFrames()
	unsigned int nSuperFrag = evt->getCapacity();

	for (unsigned int iSF = 0; iSF < nSuperFrag; iSF++) {

		unsigned int iFed = 0;

		SuperFragment* sf = evt->sfAt(iSF);

		// ?? needed?
		if (sf->getUsedFrames() == 0)
			continue;

		// compute index of last fed in this super fragment
		unsigned int nFed = sf->getUsedFrames();

		// compute number of blocks in this super fragment
		unsigned int nBlock = 1;
		unsigned int sizeThisBlock = 0;

		// frame size check
		for (unsigned int i = 0; i < sf->getUsedFrames(); i++) {
			sizeThisBlock += sf->frameAt(i).size();
			// issue en error if frame size is greater than message size
			if (sf->frameAt(i).size() > msgPayloadSize)
				LOG4CPLUS_ERROR(
						*log_,
						"FED Frame: " << i << " from super fragment " << iSF
								<< " of event " << evt->getEvtNumber()
								<< " is too large for I2O transmission! Size = "
								<< sf->frameAt(i).size() << std::endl);
			if (sizeThisBlock >= msgPayloadSize) {
				nBlock++;
				// new block takes only the last fed that did not fit
				sizeThisBlock = sf->frameAt(i).size();
			}
		}

#ifdef DEBUG_CUT
		std::cout << std::endl;
		std::cout << "nblocks " << nBlock << " for sf " << iSF << " of event "
		<< evt->getEvtNumber() << std::endl;
#endif

		bool warning = false;
		unsigned char *startOfPayload = 0;
		U32 payload(0);

		// loop over all blocks (msgs) in the current super fragment
		for (unsigned int iBlock = 0; iBlock < nBlock; iBlock++) {

			// If last block and its partial (there can be only 0 or 1 partial)
			payload = msgPayloadSize;

			// Allocate memory for a fragment block / message
			try {
				bufRef = toolbox::mem::getMemoryPoolFactory()->getFrame(
						i2oPool_, msgBufferSize_);
			} catch (xcept::Exception &e) {
				LOG4CPLUS_FATAL(*log_, "xdaq::frameAlloc failed");
			}

			// I20 block/message fields fill

			// Fill in the fields of the fragment block / message
			stdMsg = (I2O_MESSAGE_FRAME*) bufRef->getDataLocation();
			pvtMsg = (I2O_PRIVATE_MESSAGE_FRAME*) stdMsg;
			block = (I2O_EVENT_DATA_BLOCK_MESSAGE_FRAME*) stdMsg;

			pvtMsg->XFunctionCode = I2O_FU_TAKE;
			pvtMsg->OrganizationID = XDAQ_ORGANIZATION_ID;

			stdMsg->MessageSize = (msgHeaderSize + payload) >> 2;
			stdMsg->Function = I2O_PRIVATE_MESSAGE;
			stdMsg->VersionOffset = 0;
			stdMsg->MsgFlags = 0;
			stdMsg->InitiatorAddress = i2o::utils::getAddressMap()->getTid(
					buAppDesc_);
			stdMsg->TargetAddress = i2o::utils::getAddressMap()->getTid(
					fuAppDesc_);

			block->buResourceId = evt->getBuResourceId();
			block->fuTransactionId = fuResourceId;
			block->blockNb = iBlock;
			block->nbBlocksInSuperFragment = nBlock;
			block->superFragmentNb = iSF;
			// ? actual superfragments used?
			block->nbSuperFragmentsInEvent = nSuperFrag;
			block->eventNumber = evt->getEvtNumber();

			// I20 Fill in payload
			startOfPayload = (unsigned char*) block + msgHeaderSize;
			frlh_t* frlHeader = (frlh_t*) startOfPayload;
			frlHeader->trigno = evt->getEvtNumber();
			frlHeader->segno = iBlock;

			unsigned char *startOfFedBlocks = startOfPayload + frlHeaderSize_;
			payload -= frlHeaderSize_;
			frlHeader->segsize = payload;
			unsigned int leftspace = payload;

			//!
			//! no remaining fed data
			//!
			//if (remainder == 0) {
			if (true) {

				// loop on feds
				while (iFed < nFed) {

					// fed fits with its trailer (entirely)
					if (sf->fedSizeAt(iFed) <= leftspace) {

						memcpy(startOfFedBlocks,
								sf->frameAt(iFed).getBufferStart(),
								sf->fedSizeAt(iFed));
#ifdef DEBUG_CUT
						std::cout << "1 - words:" << sf->frameAt(iFed).getTrailer().lenght() << std::endl;
#endif
						leftspace -= sf->fedSizeAt(iFed);
						startOfFedBlocks += (sf->fedSizeAt(iFed));
					}

					// else new block required
					else {
#ifdef DEBUG_CUT
						std::cout << "2 - new block required" << std::endl;
#endif
						frlHeader->segsize -= leftspace;
						int msgSize = stdMsg->MessageSize << 2;
						msgSize -= leftspace;
						bufRef->setDataSize(msgSize);
						stdMsg->MessageSize = msgSize >> 2;
						break;
					}

					// !! increase iFed !!
					iFed++;
				} // while (iFed < nFed)
#ifdef DEBUG_CUT
				std::cout << "BREAK:: SF " << iSF << " of "
				<< nSuperFrag << " block " << iBlock << " of "
				<< nBlock << " ifed / nfed " << iFed << " / "
				<< nFed << " leftspace " << leftspace << std::endl;
#endif
				// earmark the last block
				if (iFed == nFed) {
					frlHeader->segsize -= leftspace;
					int msgSize = stdMsg->MessageSize << 2;
					msgSize -= leftspace;
					bufRef->setDataSize(msgSize);
					stdMsg->MessageSize = msgSize >> 2;
					frlHeader->segsize = frlHeader->segsize | FRL_LAST_SEGM;
#ifdef DEBUG_CUT
					std::cout << "Marked last segment for SF " << iSF << " of "
					<< nSuperFrag << " block " << iBlock << " of "
					<< nBlock << " ifed / nfed " << iFed << " / "
					<< nFed << std::endl;
#endif
				}

			} // if (remainder==0)

			if (iSF == 0 && iBlock == 0) { // This is the first fragment block / message
				head = bufRef;
				tail = bufRef;
			} else {
				tail->setNextReference(bufRef);
				tail = bufRef;
			}

			if ((iBlock == nBlock - 1) && iFed != nFed) {
				nBlock++;
				warning = true;
			}

		} // for (iBlock)

		// fix case where block estimate was wrong

		if (warning) {
			toolbox::mem::Reference* next = head;
			do {
				block
						= (I2O_EVENT_DATA_BLOCK_MESSAGE_FRAME*) next->getDataLocation();
				if (block->superFragmentNb == iSF)
					block->nbBlocksInSuperFragment = nBlock;
			} while ((next = next->getNextReference()));
		}

	} // iSuperFrag < nSuperFrag

	return head; // return the top of the chain
}

//______________________________________________________________________________

// a la Emilio

toolbox::mem::Reference* BUMessageCutter::createMessageChain(ABUEvent* evt,
		unsigned int fuResourceId) {

	unsigned int msgHeaderSize = sizeof(I2O_EVENT_DATA_BLOCK_MESSAGE_FRAME);
	unsigned int msgPayloadSize = msgBufferSize_ - msgHeaderSize;

	if ((msgPayloadSize % 4) != 0)
		LOG4CPLUS_ERROR(*log_, "Invalid Payload Size.");

	toolbox::mem::Reference *head = 0;
	toolbox::mem::Reference *tail = 0;
	toolbox::mem::Reference *bufRef = 0;

	I2O_MESSAGE_FRAME *stdMsg = 0;
	I2O_PRIVATE_MESSAGE_FRAME *pvtMsg = 0;
	I2O_EVENT_DATA_BLOCK_MESSAGE_FRAME *block = 0;

	unsigned int nSuperFrag = evt->getCapacity();

	// loop over all super fragments
	for (unsigned int iSuperFrag = 0; iSuperFrag < nSuperFrag; iSuperFrag++) {

		unsigned int iFed = 0;
		SuperFragment* sf = evt->sfAt(iSuperFrag);

		// compute index of last fed in this super fragment
		unsigned int nFed = sf->getUsedFrames();

		// compute number of blocks in this super fragment
		unsigned int nBlock = 0;
		unsigned int curbSize = frlHeaderSize_;
		unsigned int totSize = curbSize;
		for (unsigned int i = iFed; i < nFed; i++) {
			curbSize += sf->fedSizeAt(i);
			totSize += sf->fedSizeAt(i);
			if (curbSize > msgPayloadSize) {
				curbSize += frlHeaderSize_ * (curbSize / msgPayloadSize);
				if (curbSize % msgPayloadSize)
					totSize += frlHeaderSize_ * (curbSize / msgPayloadSize);
				else
					totSize += frlHeaderSize_ * ((curbSize / msgPayloadSize)
							- 1);
				curbSize = curbSize % msgPayloadSize;
			}
		}
		nBlock = totSize / msgPayloadSize + (totSize % msgPayloadSize > 0 ? 1
				: 0);

		// loop over all blocks (msgs) in the current super fragment
		unsigned int remainder = 0;
		bool fedTrailerLeft = false;
		bool last = false;
		bool warning = false;
		unsigned char *startOfPayload = 0;
		U32 payload(0);

		for (unsigned int iBlock = 0; iBlock < nBlock; iBlock++) {

			// If last block and its partial (there can be only 0 or 1 partial)
			payload = msgPayloadSize;

			// Allocate memory for a fragment block / message
			try {
				bufRef = toolbox::mem::getMemoryPoolFactory()->getFrame(
						i2oPool_, msgBufferSize_);
			} catch (xcept::Exception &e) {
				LOG4CPLUS_FATAL(*log_, "xdaq::frameAlloc failed");
			}

			// Fill in the fields of the fragment block / message
			stdMsg = (I2O_MESSAGE_FRAME*) bufRef->getDataLocation();
			pvtMsg = (I2O_PRIVATE_MESSAGE_FRAME*) stdMsg;
			block = (I2O_EVENT_DATA_BLOCK_MESSAGE_FRAME*) stdMsg;

			pvtMsg->XFunctionCode = I2O_FU_TAKE;
			pvtMsg->OrganizationID = XDAQ_ORGANIZATION_ID;

			stdMsg->MessageSize = (msgHeaderSize + payload) >> 2;
			stdMsg->Function = I2O_PRIVATE_MESSAGE;
			stdMsg->VersionOffset = 0;
			stdMsg->MsgFlags = 0;
			stdMsg->InitiatorAddress = i2o::utils::getAddressMap()->getTid(
					buAppDesc_);
			stdMsg->TargetAddress = i2o::utils::getAddressMap()->getTid(
					fuAppDesc_);

			block->buResourceId = evt->getBuResourceId();
			block->fuTransactionId = fuResourceId;
			block->blockNb = iBlock;
			block->nbBlocksInSuperFragment = nBlock;
			block->superFragmentNb = iSuperFrag;
			block->nbSuperFragmentsInEvent = nSuperFrag;
			block->eventNumber = evt->getEvtNumber();

			// Fill in payload
			startOfPayload = (unsigned char*) block + msgHeaderSize;
			frlh_t* frlHeader = (frlh_t*) startOfPayload;
			frlHeader->trigno = evt->getEvtNumber();
			frlHeader->segno = iBlock;

			unsigned char *startOfFedBlocks = startOfPayload + frlHeaderSize_;
			payload -= frlHeaderSize_;
			frlHeader->segsize = payload;
			unsigned int leftspace = payload;

			// a fed trailer was left over from the previous block
			if (fedTrailerLeft) {
				memcpy(
						startOfFedBlocks,
						sf->frameAt(iFed).getBufferStart()
								+ sf->fedSizeAt(iFed) - fedTrailerSize_,
						fedTrailerSize_);

				startOfFedBlocks += fedTrailerSize_;
				leftspace -= fedTrailerSize_;
				remainder = 0;
				fedTrailerLeft = false;

				// if this is the last fed, adjust block (msg) size and set last=true
				if ((iFed == nFed - 1) && !last) {
					frlHeader->segsize -= leftspace;
					int msgSize = stdMsg->MessageSize << 2;
					msgSize -= leftspace;
					bufRef->setDataSize(msgSize);
					stdMsg->MessageSize = msgSize >> 2;
					frlHeader->segsize = frlHeader->segsize | FRL_LAST_SEGM;
					last = true;
				}

				// !! increment iFed !!
				iFed++;
			}

			//!
			//! remainder>0 means that a partial fed is left over from the last block
			//!
			if (remainder > 0) {

				// the remaining fed fits entirely into the new block
				if (payload >= remainder) {
					memcpy(
							startOfFedBlocks,
							sf->frameAt(iFed).getBufferStart() + sf->fedSizeAt(
									iFed) - remainder, remainder);

					startOfFedBlocks += remainder;
					leftspace -= remainder;

					// if this is the last fed in the superfragment, earmark it
					if (iFed == nFed - 1) {
						frlHeader->segsize -= leftspace;
						int msgSize = stdMsg->MessageSize << 2;
						msgSize -= leftspace;
						bufRef->setDataSize(msgSize);
						stdMsg->MessageSize = msgSize >> 2;
						frlHeader->segsize = frlHeader->segsize | FRL_LAST_SEGM;
						last = true;
					}

					// !! increment iFed !!
					iFed++;

					// start new fed -> set remainder to 0!
					remainder = 0;
				}
				// the remaining payload fits, but not the fed trailer
				else if (payload >= (remainder - fedTrailerSize_)) {
					memcpy(
							startOfFedBlocks,
							sf->frameAt(iFed).getBufferStart() + sf->fedSizeAt(
									iFed) - remainder,
							remainder - fedTrailerSize_);

					frlHeader->segsize = remainder - fedTrailerSize_;
					fedTrailerLeft = true;
					leftspace -= (remainder - fedTrailerSize_);
					remainder = fedTrailerSize_;
				}
				// the remaining payload fits only partially, fill whole block
				else {
					memcpy(
							startOfFedBlocks,
							sf->frameAt(iFed).getBufferStart() + sf->fedSizeAt(
									iFed) - remainder, payload);
					remainder -= payload;
					leftspace = 0;
				}
			}

			//!
			//! no remaining fed data
			//!
			if (remainder == 0) {

				// loop on feds
				while (iFed < nFed) {

					// if the next header does not fit, jump to following block
					if ((int) leftspace < fedHeaderSize_) {
						frlHeader->segsize -= leftspace;
						break;
					}

					memcpy(startOfFedBlocks,
							sf->frameAt(iFed).getBufferStart(), fedHeaderSize_);

					leftspace -= fedHeaderSize_;
					startOfFedBlocks += fedHeaderSize_;

					// fed fits with its trailer
					if (sf->fedSizeAt(iFed) - fedHeaderSize_ <= leftspace) {
						memcpy(
								startOfFedBlocks,
								sf->frameAt(iFed).getBufferStart()
										+ fedHeaderSize_,
								sf->fedSizeAt(iFed) - fedHeaderSize_);

						leftspace -= (sf->fedSizeAt(iFed) - fedHeaderSize_);
						startOfFedBlocks += (sf->fedSizeAt(iFed)
								- fedHeaderSize_);
					}
					// fed payload fits only without fed trailer
					else if (sf->fedSizeAt(iFed) - fedHeaderSize_
							- fedTrailerSize_ <= leftspace) {
						memcpy(
								startOfFedBlocks,
								sf->frameAt(iFed).getBufferStart()
										+ fedHeaderSize_,
								sf->fedSizeAt(iFed) - fedHeaderSize_
										- fedTrailerSize_);

						leftspace -= (sf->fedSizeAt(iFed) - fedHeaderSize_
								- fedTrailerSize_);
						frlHeader->segsize -= leftspace;
						fedTrailerLeft = true;
						remainder = fedTrailerSize_;

						break;
					}
					// fed payload fits only partially
					else {
						memcpy(
								startOfFedBlocks,
								sf->frameAt(iFed).getBufferStart()
										+ fedHeaderSize_, leftspace);
						remainder = sf->fedSizeAt(iFed) - fedHeaderSize_
								- leftspace;
						leftspace = 0;

						break;
					}

					// !! increase iFed !!
					iFed++;

				} // while (iFed<fedN_)

				// earmark the last block
				if (iFed == nFed && remainder == 0 && !last) {
					frlHeader->segsize -= leftspace;
					int msgSize = stdMsg->MessageSize << 2;
					msgSize -= leftspace;
					bufRef->setDataSize(msgSize);
					stdMsg->MessageSize = msgSize >> 2;
					frlHeader->segsize = frlHeader->segsize | FRL_LAST_SEGM;
					last = true;

#ifdef DEBUG_CUT
					std::cout << "EM: Marked last segment for SF " << iSuperFrag << " of "
					<< nSuperFrag << " block " << iBlock << " of "
					<< nBlock << " ifed / nfed " << iFed << " / "
					<< nFed << std::endl;
#endif
				}

			} // if (remainder==0)

			if (iSuperFrag == 0 && iBlock == 0) { // This is the first fragment block / message
				head = bufRef;
				tail = bufRef;
			} else {
				tail->setNextReference(bufRef);
				tail = bufRef;
			}

			if ((iBlock == nBlock - 1) && remainder != 0) {
				nBlock++;
				warning = true;
			}

		} // for (iBlock)

		// fix case where block estimate was wrong
		if (warning) {
			toolbox::mem::Reference* next = head;
			do {
				block
						= (I2O_EVENT_DATA_BLOCK_MESSAGE_FRAME*) next->getDataLocation();
				if (block->superFragmentNb == iSuperFrag)
					block->nbBlocksInSuperFragment = nBlock;
			} while ((next = next->getNextReference()));
		}

	} // iSuperFrag < nSuperFrag

	return head; // return the top of the chain

}


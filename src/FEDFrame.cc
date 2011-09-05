/*
 * FEDFrame.cc
 *
 *  Created on: Jun 28, 2011
 *      Author: aspataru
 */

#include "EventFilter/AutoBU/interface/FEDFrame.h"
#include "EventFilter/AutoBU/interface/Constants.h"
#include "EventFilter/AutoBU/interface/CRC16.h"

#include <cstring>

using namespace evf;
using std::memcpy;

FEDFrame::FEDFrame(int evt_ty, int lvl1_ID, int bx_ID, int source_ID,
		int version, bool H, int evt_lgth, int crc, int evt_stat, int tts,
		bool T) :
			// bufferSize is in bytes
			bufferStart_(new unsigned char[evt_lgth * BYTES_IN_WORD]),
			trailerStart_(
					&bufferStart_[evt_lgth * BYTES_IN_WORD - TRAILER_SIZE]) {

	// compute CRC16
	unsigned short crcResult = evf::compute_crc(bufferStart_,
			evt_lgth * BYTES_IN_WORD);

	// set header and trailer, with correct CRC
	FEDHeader::set(bufferStart_, evt_ty, lvl1_ID, bx_ID, source_ID, version, H);
	FEDTrailer::set(trailerStart_, evt_lgth, crcResult, evt_stat, tts, T);
}

FEDFrame::FEDFrame(int lvl1_ID, int source_ID, int evt_lgth, int crc) {
	FEDFrame(1, lvl1_ID, 0, source_ID, 0, 0, evt_lgth, crc, 0, 0, 0);
}

// attempts to retrieve a FEDFrame object from an array
FEDFrame::FEDFrame(unsigned char* source, int evt_lgth) :
			bufferStart_(new unsigned char[evt_lgth * BYTES_IN_WORD]),
			trailerStart_(
					&bufferStart_[evt_lgth * BYTES_IN_WORD - TRAILER_SIZE]) {

	memcpy(bufferStart_, source, evt_lgth * 8);

}

FEDFrame::~FEDFrame() {
	delete[] bufferStart_;
}

FEDHeader FEDFrame::getHeader() const {
	FEDHeader header(bufferStart_);
	return header;
}

FEDTrailer FEDFrame::getTrailer() const {
	FEDTrailer trailer(trailerStart_);
	return trailer;
}

unsigned char * FEDFrame::getPayload() const {
	return (unsigned char *) (bufferStart_ + HEADER_SIZE);
}

unsigned int FEDFrame::size() const {
	return getTrailer().lenght() * BYTES_IN_WORD;
}

unsigned char* FEDFrame::getBufferStart() const {
	return bufferStart_;
}

unsigned char* FEDFrame::getTrailerStart() const {
	return trailerStart_;
}


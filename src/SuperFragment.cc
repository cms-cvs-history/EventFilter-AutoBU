/*
 * SuperFragment.cc
 *
 *  Created on: Jun 29, 2011
 *      Author: aspataru
 */

#include "EventFilter/AutoBU/interface/SuperFragment.h"
#include "FWCore/Utilities/interface/CRC16.h"

#include <cstring>
#include <iostream>
#include <algorithm>

using std::memcpy;
using namespace evf;

SuperFragment::SuperFragment(unsigned int fedSize) :
	bufferStart_(new unsigned char[INITIAL_SUPERFRAGMENT_SIZE_BYTES]),
			bufferPos_(bufferStart_),
			bufferEnd_(&bufferStart_[INITIAL_SUPERFRAGMENT_SIZE_BYTES]),
			maxFrames_(fedSize), usedFrames_(0),
			trueByteSize_(INITIAL_SUPERFRAGMENT_SIZE_BYTES), used_(false),
			computeCRC_(false) {

	frlHeader_.trigno = 0;
	frlHeader_.segno = 0;
	frlHeader_.segsize = 0;
}

SuperFragment::SuperFragment(unsigned int byteSize, unsigned int fedSize) :
	bufferStart_(new unsigned char[byteSize]), bufferPos_(bufferStart_),
			bufferEnd_(&bufferStart_[byteSize]), maxFrames_(fedSize),
			usedFrames_(0), trueByteSize_(byteSize), used_(false) {

	frlHeader_.trigno = 0;
	frlHeader_.segno = 0;
	frlHeader_.segsize = 0;
}

SuperFragment::~SuperFragment() {
	delete[] bufferStart_;
}

bool SuperFragment::pushFrame(int evt_ty, int lvl1_ID, int bx_ID,
		int source_ID, int version, bool H, int evt_lgth, int crc,
		int evt_stat, int tts, bool T) {

	int bufferSize = evt_lgth * BYTES_IN_WORD;

	if ((bufferPos_ + bufferSize) <= bufferEnd_ && usedFrames_ < maxFrames_) {

		FEDFrame frameToPush(evt_ty, lvl1_ID, bx_ID, source_ID, version, H,
				evt_lgth, crc, evt_stat, tts, T);

		if (computeCRC_) {
			int newCRC = evf::compute_crc(frameToPush.getBufferStart(),
					frameToPush.size());
			// set computed CRC in trailer
			frameToPush.getTrailer().set(frameToPush.getTrailerStart(),
					evt_lgth, newCRC, evt_stat, tts, T);
		}

		memcpy(bufferPos_, frameToPush.getBufferStart(), frameToPush.size());

		bufferPos_ += bufferSize;
		usedFrames_++;

		increaseFrlSegsize(frameToPush.size());

		return true;
	} else {
		// "Adding this frame will exceed superfragment buffer! Cancelling..."
		return false;
	}
}

bool SuperFragment::pushFrame(int lvl1_ID, int source_ID, int evt_lgth, int crc) {
	return pushFrame(1, lvl1_ID, 0, source_ID, 0, 0, evt_lgth, crc, 0, 0, 0);
}

bool SuperFragment::pushFrame(unsigned char* data, unsigned int size) {
	if ((bufferPos_ + size) <= bufferEnd_ && usedFrames_ < maxFrames_) {
		memcpy(bufferPos_, data, size);
		bufferPos_ += size;
		usedFrames_++;
		increaseFrlSegsize(size);
		return true;
	} else {
		// "Adding this frame will exceed superfragment buffer! Cancelling..."
		return false;
	}
}

FEDFrame SuperFragment::frameAt(unsigned int index) const {

	// idea: decrease bufferPos

	unsigned char* dBufferPos = bufferPos_;

	unsigned int i = 0;
	unsigned char* headerPosition;
	int currentEvtLength;
	unsigned int steps;

	//check if looking beyond first frame
	if (index >= usedFrames_) {
		std::cout << "No frame at that index! Returning first frame!"
				<< std::endl;
		steps = usedFrames_ - 1;
	} else {
		steps = usedFrames_ - index - 1;
	}

	while (i <= steps) {
		// find a trailer from the current dBufferPos
		FEDTrailer currentTrailer(dBufferPos - TRAILER_SIZE);
		// get the event length from the trailer
		currentEvtLength = currentTrailer.lenght();
		// get the header position from the length
		headerPosition = dBufferPos - currentEvtLength * BYTES_IN_WORD;
		// move the decreasing buffer "up" to look for the next trailer
		dBufferPos = headerPosition;

		i++;
	}
	return FEDFrame(headerPosition, currentEvtLength);
}

FEDFrame SuperFragment::lastFrame() const {
	FEDTrailer lastTrailer(bufferPos_ - TRAILER_SIZE);
	int currentEvtLength = lastTrailer.lenght();
	unsigned char* headerPosition = bufferPos_ - currentEvtLength
			* BYTES_IN_WORD;
	return FEDFrame(headerPosition, currentEvtLength);
}

const unsigned int SuperFragment::getUsedFrames() const {
	return usedFrames_;
}

const vector<unsigned char*> SuperFragment::getPointersToFeds() const {
	vector<unsigned char*> fedPointers;

	unsigned char* dBufferPos = bufferPos_;

	unsigned int i = 0;
	unsigned char* headerPosition;
	int currentEvtLength;

	while (i < usedFrames_) {
		// find a trailer from the current dBufferPos
		FEDTrailer currentTrailer(dBufferPos - TRAILER_SIZE);
		// get the event length from the trailer
		currentEvtLength = currentTrailer.lenght();
		// get the header position from the length
		headerPosition = dBufferPos - currentEvtLength * 8;
		// move the decreasing buffer "up" to look for the next trailer
		dBufferPos = headerPosition;
		i++;

		fedPointers.push_back(headerPosition);
	}

	// reverse vector so the frames appear in the initial order
	reverse(fedPointers.begin(), fedPointers.end());

	return fedPointers;
}

unsigned int SuperFragment::fedSizeAt(unsigned int index) const {
	return frameAt(index).size();
}

void SuperFragment::reset() {
	bufferPos_ = bufferStart_;
	usedFrames_ = 0;

	frlHeader_.trigno = 0;
	frlHeader_.segno = 0;
	frlHeader_.segsize = 0;
}

void SuperFragment::setFrlTrigno(unsigned int trigno) {
	frlHeader_.trigno = trigno;
}
void SuperFragment::setFrlSegno(unsigned int segno) {
	frlHeader_.segno = segno;
}
void SuperFragment::increaseFrlSegsize(unsigned int nBytes) {
	frlHeader_.segsize += nBytes;
}

frlh_t* SuperFragment::getFrl() {
	return &frlHeader_;
}

unsigned int SuperFragment::getTrueSize() const {
	return trueByteSize_;
}

unsigned char* SuperFragment::getFedBufferStart() const {
	return bufferStart_;
}

unsigned int SuperFragment::getMaxFrames() const {
	return maxFrames_;
}

void SuperFragment::setUsedFrames(unsigned int nFr) {
	usedFrames_ = nFr;
}

void SuperFragment::setMaxFrames(unsigned int mFr) {
	maxFrames_ = mFr;
}

void SuperFragment::setBufferPos(unsigned char* newPos) {
	bufferPos_ = newPos;
}

void SuperFragment::use() {
	used_ = true;
}

bool SuperFragment::isUsed() const {
	return used_;
}

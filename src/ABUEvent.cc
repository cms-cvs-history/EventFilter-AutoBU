/*
 * ABUEvent.cc
 *
 *  Created on: Jun 29, 2011
 *      Author: aspataru
 */

#include "EventFilter/AutoBU/interface/ABUEvent.h"

#include <iostream>

using namespace evf;

ABUEvent::ABUEvent(unsigned int buResourceId, ABUConfig config) :
	buResourceId_(buResourceId), config_(config),
			maxSize_(config_.evtCapacity()), evtNumber_(0), initialized_(false) {

}

ABUEvent::~ABUEvent() {
	while (sFragments_.size()) {
		delete sFragments_.back();
		sFragments_.pop_back();
	}
}

void ABUEvent::initialise(unsigned int evtNumber) {
	evtNumber_ = evtNumber;
	for (unsigned int i = 0; i < maxSize_; i++) {
		SuperFragment* current = new SuperFragment(config_.superFragSizeAt(i));
		current->setFrlTrigno(evtNumber_);
		sFragments_.push_back(current);
	}
	initialized_ = true;
}

const unsigned int ABUEvent::fedCount() const {
	unsigned int count = 0;
	for (unsigned int i = 0; i < maxSize_; ++i) {
		count += sFragments_[i]->getUsedFrames();
	}
	return count;
}

const unsigned int ABUEvent::getBuResourceId() const {
	return buResourceId_;
}

int ABUEvent::getEvtNumber() const {
	return evtNumber_;
}

void ABUEvent::reset(unsigned int evtNumber) {
	evtNumber_ = evtNumber;
	for (unsigned int i = 0; i < usedSf(); ++i) {
		sFragments_[i]->reset();
		sFragments_[i]->setFrlTrigno(evtNumber_);
	}
}

unsigned int ABUEvent::usedSf() const {
	unsigned int used = 0;
	for (unsigned int i = 0; i < maxSize_; i++) {
		if (sFragments_[i]->isUsed()) {
			used++;
		}
	}

	return used;
}

unsigned int ABUEvent::getCapacity() const {
	return maxSize_;
}

SuperFragment* ABUEvent::sfAt(unsigned int index) const {
	if (index >= getCapacity())
		return 0;
	return sFragments_[index];
}

void ABUEvent::replaceSuperFragment(unsigned int oldIndex,
		SuperFragment* newPointer) {
	SuperFragment* oldReference = sFragments_[oldIndex];
	sFragments_[oldIndex] = newPointer;
	delete oldReference;
}

bool ABUEvent::isInitialised() const {
	return initialized_;
}
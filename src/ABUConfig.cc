/*
 * ABUConfig.cc
 *
 *  Created on: Jul 26, 2011
 *      Author: aspataru
 */

#include "EventFilter/AutoBU/interface/ABUConfig.h"
#include "EventFilter/AutoBU/interface/Constants.h"

using namespace evf;

ABUConfig::ABUConfig() :
	nSuperFrag_(DEFAULT_EVENT_CAPACITY),
			sfSizes_(new unsigned int[nSuperFrag_]) {

	for (unsigned int i = 0; i < nSuperFrag_; i++) {
		sfSizes_[i] = DEFAULT_SUPERFRAGMENT_LENGTH;
	}
}

ABUConfig::ABUConfig(string configFilePath) :
	configFilePath_(configFilePath) {

	//improve load external values and initialise
}

ABUConfig::~ABUConfig() {
}

unsigned int ABUConfig::evtCapacity() const {
	return nSuperFrag_;
}

unsigned int ABUConfig::superFragSizeAt(unsigned int index) const {
	return sfSizes_[index];
}

void ABUConfig::setSuperFragmentSize(SuperFragment* theFragment,
		unsigned int nFeds) {
	theFragment->setMaxFrames(nFeds);
}


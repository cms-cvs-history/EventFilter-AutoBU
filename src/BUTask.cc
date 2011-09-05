/*
 * BUTask.cc
 *
 *  Created on: Aug 3, 2011
 *      Author: aspataru
 */

#include "EventFilter/AutoBU/interface/BUTask.h"

using namespace evf;

BUTask::BUTask(unsigned int id, unsigned char type) :
	id_(id), type_(type) {
}

BUTask::~BUTask() {

}

unsigned int BUTask::id() const {
	return id_;
}

unsigned char BUTask::type() const {
	return type_;
}


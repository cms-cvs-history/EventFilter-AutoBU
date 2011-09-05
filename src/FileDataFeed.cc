/*
 * FileDataFeed.cc
 *
 *  Created on: Jul 4, 2011
 *      Author: aspataru
 */

#include "EventFilter/AutoBU/interface/FileDataFeed.h"

#include <iostream>
#include <fstream>

#include <string>

using std::string;

void FileDataFeed::setSource(char* source) {
	this->source_ = source;
	this->in_ = new ifstream(source);
	if (!in_) {
		std::cout << "File not found!" << std::endl;
	}
}

FileDataFeed::~FileDataFeed() {
	delete in_;
}

int FileDataFeed::getIntData() {
	int data;
	*in_ >> data;
	return data;
}


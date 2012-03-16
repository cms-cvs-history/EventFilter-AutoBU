/*
 * FrameSizeUtils.cc
 *
 *  Created on: Jul 12, 2011
 *      Author: aspataru
 */

#include "EventFilter/AutoBU/interface/FrameSizeUtils.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

using std::log;
using std::sqrt;
using std::memcpy;

unsigned int FrameSizeUtils::boundedRandom(unsigned int max, unsigned int min) {
	return (rand() % (max - min) + min);
}

void FrameSizeUtils::dumpFrame(unsigned char* data, unsigned int len) {
	char left1[20];
	char left2[20];
	char right1[20];
	char right2[20];

	printf("Byte  0  1  2  3  4  5  6  7\n");

	int c(0);
	int pos(0);

	for (unsigned int i = 0; i < (len / 8); i++) {
		int rpos(0);
		int off(3);
		for (pos = 0; pos < 12; pos += 3) {
			sprintf(&left1[pos], "%2.2x ", ((unsigned char*) data)[c + off]);
			sprintf(
					&right1[rpos],
					"%1c",
					((data[c + off] > 32) && (data[c + off] < 127)) ? data[c
							+ off] : '.');
			sprintf(&left2[pos], "%2.2x ", ((unsigned char*) data)[c + off + 4]);
			sprintf(
					&right2[rpos],
					"%1c",
					((data[c + off + 4] > 32) && (data[c + off + 4] < 127)) ? data[c
							+ off + 4]
							: '.');
			rpos++;
			off--;
		}
		c += 8;

		printf("%4d: %s%s ||  %s%s  %p\n", c - 8, left1, left2, right1, right2,
				&data[c - 8]);
	}

	fflush(stdout);
}

// max number of frames not doubled when expanding
SuperFragment* FrameSizeUtils::expandSuperFragment(SuperFragment* initial) {
	SuperFragment* expandedSF = new SuperFragment(initial->getTrueSize() * 2,
			initial->getMaxFrames()/* * 2*/);

	expandedSF->getFrl()->segno = initial->getFrl()->segno;
	expandedSF->getFrl()->trigno = initial->getFrl()->trigno;
	expandedSF->getFrl()->segsize = initial->getFrl()->segsize;
	expandedSF->setUsedFrames(initial->getUsedFrames());

	memcpy(expandedSF->getFedBufferStart(), initial->getFedBufferStart(),
			initial->getFrl()->segsize);

	unsigned char* newBufferPos = expandedSF->getFedBufferStart()
			+ initial->getFrl()->segsize;
	expandedSF->setBufferPos(newBufferPos);

	return expandedSF;
}

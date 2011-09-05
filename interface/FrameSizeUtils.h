/** \class FrameSizeUtils
 *  Class that provides utility functions for FED Frame operations.
 *
 *  \author A. Spataru
 */

#ifndef FRAMESIZEUTILS_H_
#define FRAMESIZEUTILS_H_

#include "Constants.h"
#include "SuperFragment.h"

using evf::MIN_WORDS_IN_FRAME;
using evf::SuperFragment;

class FrameSizeUtils {
public:

	/// Returns a random number between given maximum and minimum
	static unsigned int boundedRandom(unsigned int max,
			unsigned int min = MIN_WORDS_IN_FRAME);
	/// Dumps the FED frame pointed to by pointer, of specified length
	static void dumpFrame(unsigned char* data, unsigned int len);
	/**
	 * Doubles both the allocated size of the superfragment, and the
	 * maximum number of FED frames it may hold, by copying it to another
	 * memory location. Deletion of the old memory location is left to
	 * the caller.
	 */
	static SuperFragment* expandSuperFragment(SuperFragment* initial);
};

#endif /* FRAMESIZEUTILS_H_ */

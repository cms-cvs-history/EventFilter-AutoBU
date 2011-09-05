/** \class Constants
 *	Class used to hold constants for the Builder Unit.
 *
 *  \author A. Spataru
 */

#ifndef ABU_CONSTANTS_H_
#define ABU_CONSTANTS_H_

namespace evf {

/// FED Frame header size in bytes
const unsigned int HEADER_SIZE = 8;
/// FED Frame word size in bits
const unsigned int WORD_SIZE_IN_BITS = 64;
/// FED Trailer size in bytes
const unsigned int TRAILER_SIZE = 8;
/// Initial allocated size of superfragments in event
const unsigned int INITIAL_SUPERFRAGMENT_SIZE_BYTES = 20000;
/// Number of bytes in a FED Frame word
const unsigned int BYTES_IN_WORD = 8;
/// Minimum number of words in FED Frame
const unsigned int MIN_WORDS_IN_FRAME = 3;
/// Initial value of the CRC field in FED Trailer
const unsigned int INITIAL_CRC = 0;
/// Default number of FED Frames in superfragment
const unsigned int DEFAULT_SUPERFRAGMENT_LENGTH = 16;
/// Default number of superfragments in event
const unsigned int DEFAULT_EVENT_CAPACITY = 64;
/// Maximum size of the Builder Unit task queue
const unsigned int MAX_TASK_QUEUE_SIZE = 10000;
/// Size of the array used to hold measurements of task queue size
const unsigned int AVG_TASK_QUEUE_STAT_WIDTH = 10000;

}

#endif /* CONSTANTS_H_ */


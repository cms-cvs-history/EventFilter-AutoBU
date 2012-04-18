/** \class SuperFragment
 *	Class representing a SuperFragment in the Builder Unit.
 *
 *  \author A. Spataru
 */

#ifndef SUPERFRAGMENT_H_
#define SUPERFRAGMENT_H_

#include "FEDFrame.h"
#include "frl_header.h"
#include "Constants.h"
#include <vector>

using std::vector;

namespace evf {
class SuperFragment {

public:
	/// Constructs a superfragment having the given FED frame capacity
	SuperFragment(unsigned int fedSize);
	/// Constructs a superfragment having the given byte size and FED frame capacity
	SuperFragment(unsigned int byteSize, unsigned int fedSize);
	~SuperFragment();
	/**
	 * Pushes a FEDFrame to the end of the Super Fragment buffer. The FEDFrame
	 * is generated using the given parameters.
	 */
	bool pushFrame(int evt_ty, int lvl1_ID, int bx_ID, int source_ID,
			int version, bool H, int evt_lgth, int crc, int evt_stat, int tts,
			bool T);
	/**
	 * Pushes a FEDFrame to the end of the Super Fragment buffer. The FEDFrame
	 * is generated using the given parameters.
	 */
	bool pushFrame(int lvl1_ID, int source_ID, int evt_lgth, int crc);
	/**
	 * Pushes a frame to the end of the superfragment buffer by copying
	 * from the specified pointer a segment of specified size
	 */
	bool pushFrame(unsigned char* data, unsigned int size);
	/**
	 * Attempts to retrieve a FEDFrame from the Super Fragment buffer, at the position
	 * indicated by index, taken from the end of the buffer.
	 */
	FEDFrame frameAt(unsigned int index) const;
	/// Attempts to retrieve the last FED Frame in the Super Fragment buffer.
	FEDFrame lastFrame() const;
	/// Returns the number of frames currently in the Super Fragment
	const unsigned int getUsedFrames() const;
	/// Returns a vector of pointers to FED frames in this superfragment
	const vector<unsigned char*> getPointersToFeds() const;
	/// Returns the FED frame size at the specified index
	unsigned int fedSizeAt(unsigned int index) const;
	/**
	 *  Resets the superfragment data pointers so new data can be overwritten,
	 *  without releasing and reallocating memory
	 */
	void reset();
	/// Sets the trigno field in the FRL header
	void setFrlTrigno(unsigned int trigno);
	/// Sets the segno field in the FRL header
	void setFrlSegno(unsigned int segno);
	/// Returns a pointer to the FRL header of this superfragment
	frlh_t* getFrl();
	/// Returns the size in bytes allocated for this superfragment
	unsigned int getTrueSize() const;
	/// Returns a pointer to the start of the FED frames buffer
	unsigned char* getFedBufferStart() const;
	unsigned int getMaxFrames() const;
	/// Sets the current number of used FED frames
	void setUsedFrames(unsigned int nFr);
	/// Sets the maximum number of frames
	void setMaxFrames(unsigned int mFr);
	/// Sets the current position of the iterator on the FED buffer
	/// At the position indicated by this pointer, new frames will be written
	void setBufferPos(unsigned char* newPos);
	/// Sets the <used_> flag to true, indicating that this fragment contains FED frames
	void use();
	/// Returns true if the current superfragment is used
	bool isUsed() const;
	/// Sets computeCRC_ flag to true
	void computeCRC() {
		computeCRC_ = true;
	}

private:
	frlh_t frlHeader_;
	unsigned char* bufferStart_;
	unsigned char* bufferPos_;
	unsigned char* bufferEnd_;
	unsigned int maxFrames_;
	unsigned int usedFrames_;
	unsigned int trueByteSize_;
	bool used_;
	bool computeCRC_;

	void increaseFrlSegsize(unsigned int nBytes);
};
}
#endif /* SUPERFRAGMENT_H_ */

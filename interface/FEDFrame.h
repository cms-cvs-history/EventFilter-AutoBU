/** \class FEDFrame
 * 	Used to represent a FED Frame of data, containing a header,
 * 	trailer and payload.
 *
 *  \author A. Spataru
 */


#ifndef FEDFRAME_H_
#define FEDFRAME_H_

#include "FEDHeader.h"
#include "FEDTrailer.h"

class FEDFrame {

public:
	/**
	 * Initialises the FEDFrame using the parameters for header and trailer.
	 * The frame size is taken from evt_lgth, expressed in 64-bit words.
	 */
	FEDFrame(int evt_ty, int lvl1_ID, int bx_ID, int source_ID, int version,
			bool H, int evt_lgth, int crc, int evt_stat, int tts, bool T);
	/**
	 * Initialises the FEDFrame using a reduced number of parameters,
	 * the rest being given default values.
	 */
	FEDFrame(int lvl1_ID, int source_ID, int evt_lgth, int crc);
	/**
	 * Attempts to initialise the FEDFrame using a pointer to the start of
	 * a sequence of bytes, of a given size.
	 */
	FEDFrame(unsigned char* source, int size);
	~FEDFrame();

	/// returns a copy of the header for this frame
	FEDHeader getHeader() const;
	/// returns a copy of the trailer for this frahosen as default.me
	FEDTrailer getTrailer() const;
	/// returns a pointer to the payload of this
	unsigned char * getPayload() const;
	/// returns the current size of the frame
	unsigned int size() const;
	/// returns a pointer to the start of the frame buffer
	unsigned char* getBufferStart() const;
	/// returns a pointer to the start of frame's trailer
	unsigned char* getTrailerStart() const;

private:
	unsigned char * bufferStart_;
	unsigned char * trailerStart_;
};

#endif /* FEDFRAME_H_ */

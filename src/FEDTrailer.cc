/** \file
 *
 *  $Date: 2008/07/01 21:50:36 $
 *  $Revision: 1.1 $
 *  \author N. Amapane - CERN
 */

#include "EventFilter/AutoBU/interface/FEDTrailer.h"

#define FED_MORE_TRAILERS  0x00000008
#define FED_TCTRLID        0xA0000000

FEDTrailer::FEDTrailer(const unsigned char* trailer) :
	theTrailer_(reinterpret_cast<const fedt_t*> (trailer)) {
}

FEDTrailer::FEDTrailer(char trailer[]) :
	theTrailer_(reinterpret_cast<const fedt_t*> (trailer)) {
}

FEDTrailer::~FEDTrailer() {
}

unsigned int FEDTrailer::lenght() {
	return (theTrailer_->eventsize & FED_EVSZ_MASK) >> FED_EVSZ_SHIFT;
}

int FEDTrailer::crc() {
	return ((theTrailer_->conscheck & FED_CRCS_MASK) >> FED_CRCS_SHIFT);
}

int FEDTrailer::evtStatus() {
	return ((theTrailer_->conscheck & FED_STAT_MASK) >> FED_STAT_SHIFT);
}

int FEDTrailer::ttsBits() {
	return ((theTrailer_->conscheck & FED_TTSI_MASK) >> FED_TTSI_SHIFT);
}

bool FEDTrailer::moreTrailers() {
	return ((theTrailer_->conscheck & FED_MORE_TRAILERS) != 0);
}

void FEDTrailer::set(unsigned char* trailer, int evt_lgth, int crc,
		int evt_stat, int tts, bool T) {
	// should check that input ranges are OK!!!
	fedt_t* t = reinterpret_cast<fedt_t*> (trailer);

	t->eventsize = FED_TCTRLID | evt_lgth << FED_EVSZ_SHIFT;

	t->conscheck = crc << FED_CRCS_SHIFT | evt_stat << FED_STAT_SHIFT | tts
			<< FED_TTSI_SHIFT;

	if (T)
		t->conscheck |= FED_MORE_TRAILERS;
}

bool FEDTrailer::check() {
	// ...may report with finer detail
	bool result = true;
	result &= ((theTrailer_->eventsize & FED_TCTRLID_MASK) == FED_TCTRLID);

	return result;
}

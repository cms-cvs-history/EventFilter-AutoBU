/** \file
 *
 *  $Date: 2008/07/01 21:50:36 $
 *  $Revision: 1.1 $
 *  \author N. Amapane - CERN
 */

#include "EventFilter/AutoBU/interface/FEDHeader.h"

#define FED_MORE_HEADERS  0x00000008
#define FED_HCTRLID       0x50000000

FEDHeader::FEDHeader(const unsigned char* header) :
	theHeader_(reinterpret_cast<const fedh_t*> (header)) {
}

FEDHeader::FEDHeader(char header[]) :
	theHeader_(reinterpret_cast<const fedh_t*> (header)) {
}

FEDHeader::~FEDHeader() {
}

int FEDHeader::triggerType() {
	return ((theHeader_->eventid & FED_EVTY_MASK) >> FED_EVTY_SHIFT);
}

int FEDHeader::lvl1ID() {
	return (theHeader_->eventid & FED_LVL1_MASK);
}

int FEDHeader::bxID() {
	return ((theHeader_->sourceid & FED_BXID_MASK) >> FED_BXID_SHIFT);
}

int FEDHeader::sourceID() {
	return ((theHeader_->sourceid & FED_SOID_MASK) >> FED_SOID_SHIFT);
}

int FEDHeader::version() {
	return ((theHeader_->sourceid & FED_VERSION_MASK) >> FED_VERSION_SHIFT);
}

bool FEDHeader::moreHeaders() {
	return ((theHeader_->sourceid & FED_MORE_HEADERS) != 0);
}

void FEDHeader::set(unsigned char* header, int evt_ty, int lvl1_ID, int bx_ID,
		int source_ID, int version, bool H) {

	// should check that input ranges are OK!!!
	fedh_t* h = reinterpret_cast<fedh_t*> (header);
	h->eventid = FED_HCTRLID | evt_ty << FED_EVTY_SHIFT | lvl1_ID
			<< FED_LVL1_SHIFT;

	h->sourceid = bx_ID << FED_BXID_SHIFT | source_ID << FED_SOID_SHIFT
			| version << FED_VERSION_SHIFT;

	if (H)
		h->sourceid |= FED_MORE_HEADERS;

}

bool FEDHeader::check() {
	// ...may report with finer detail
	// TODO complete header check
	bool result = true;
	result &= ((theHeader_->eventid & FED_HCTRLID_MASK) == FED_HCTRLID);

	return result;
}

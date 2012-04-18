/** \class ABUEvent
 *  Class representing an Event in the Builder Unit.
 *
 *  \author A. Spataru
 */

#ifndef ABUEVENT_H_
#define ABUEVENT_H_

#include "SuperFragment.h"
#include "ABUConfig.h"

using std::vector;

namespace evf {
class ABUEvent {

public:
	/**
	 * Constructs an event using the given BU resource ID
	 * and configuration object
	 */
	ABUEvent(unsigned int buResourceId, ABUConfig config);
	~ABUEvent();

	/// Initialises and event with the given number
	void initialise(unsigned int evtNumber);
	/// Returns the number of FED frames in this event
	const unsigned int fedCount() const;
	/// Returns the BU resource ID associated to this event
	const unsigned int getBuResourceId() const;
	/// Returns the event number
	int getEvtNumber() const;
	/// Resets all superfragments in this event
	void reset(unsigned int evtNumber);
	/// Returns the number of superfragments holding FED frames
	unsigned int usedSf() const;
	/// Returns the maximum number of superfragments in this event
	unsigned int getCapacity() const;
	/// Returns a pointer to a superfragment at the specified index
	SuperFragment* sfAt(unsigned int index) const;
	/**
	 * Replaces the superfragment at the specified index with another one,
	 * given by the pointer. Used when expanding superfragments.
	 */
	void replaceSuperFragment(unsigned int oldIndex, SuperFragment* newPointer);
	/// Returns true if the event has been initialised
	bool isInitialised() const;

private:
	unsigned int buResourceId_;
	ABUConfig config_;
	unsigned int maxSize_;
	std::vector<SuperFragment*> sFragments_;
	int evtNumber_;
	bool initialized_;

};
}
#endif /* ABUEVENT_H_ */

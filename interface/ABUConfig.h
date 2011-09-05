/** \class ABUConfig
 *  Class used to establish the configuration of an Event.
 *
 *  \author A. Spataru
 */

#ifndef ABUCONFIG_H_
#define ABUCONFIG_H_

#include "SuperFragment.h"
#include <string>

using std::string;

namespace evf {
class ABUConfig {
public:
	// default configuration, no input file
	ABUConfig();
	ABUConfig(string configFilePath);
	~ABUConfig();

	unsigned int evtCapacity() const;
	unsigned int superFragSizeAt(unsigned int index) const;

	static void setSuperFragmentSize(SuperFragment* theFragment,
			unsigned int nFeds);

private:
	string configFilePath_;
	unsigned int nSuperFrag_;
	unsigned int* sfSizes_;

};
}

#endif /* ABUCONFIG_H_ */

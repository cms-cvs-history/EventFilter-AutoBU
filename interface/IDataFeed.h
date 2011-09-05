/** \class IDataFeed
 *  NOT USED
 *
 *  \author A. Spataru
 */

#ifndef IDATAFEED_H_
#define IDATAFEED_H_

#include <string>

using std::string;


class IDataFeed {
public:
	virtual ~IDataFeed(){}
	/// sets the source of data
	virtual void setSource(char* source) = 0;
	/// gets an integer parameter
	virtual int getIntData() = 0;
};

#endif /* IDATAFEED_H_ */

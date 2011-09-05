/** \class FileDataFeed
 *  NOT USED
 *
 *  \author A. Spataru
 */

#ifndef FILEDATAFEED_H_
#define FILEDATAFEED_H_

#include "IDataFeed.h"
#include <fstream>

using std::ifstream;


class FileDataFeed : public IDataFeed
{
public:
	~FileDataFeed();
	/// sets data source
	void setSource(char* source);
	/// returns a parameter read from source
	int getIntData();

private:
	char* source_;
	ifstream * in_;
};

#endif /* FILEDATAFEED_H_ */

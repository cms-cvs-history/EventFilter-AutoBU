/** \class BUTask
 *   Represents a Builder Unit task for the task queue.
 *
 *  \author A. Spataru
 */

#ifndef BUTASK_H_
#define BUTASK_H_

namespace evf {

class BUTask {

public:
	/**
	 * Main constructor taking as parameters
	 * the task id and type.
	 */
	BUTask(unsigned int id, unsigned char type);
	~BUTask();

	unsigned int id() const;
	/**
	 * Types of task:
	 * b - build
	 * s - send
	 * r - request
	 */
	unsigned char type() const;

private:
	unsigned int id_;
	unsigned char type_;

};
}

#endif /* BUTASK_H_ */

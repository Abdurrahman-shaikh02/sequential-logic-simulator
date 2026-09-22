#pragma once

#include "operations/Operation.hpp"

class OperationQueue {
private:
	//Need an appropriate data structure...
public:
	void enqueue(Operation op);
	Operation dequeue();
	void cancel(Operation op);
	bool isEmpty();
	void reorder();		//need to detect if the same ff has both set port and update state 
				//if it does then issue warning.
				//then all change states first... then set ports
				//obviously dequeueing will automatically cause the commits to be at the end
};

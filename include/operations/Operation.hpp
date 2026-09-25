#pragma once

#include "core/Node.hpp"
#include "core/Port.hpp"
#include "core/Signal.hpp"

class Operation {
protected:
	Node &target;
	Port p;
	Signal value;

	Operation(Node &target, Port p, Signal value);
public:
	bool operator==(const Operation &op);
	virtual void run() = 0;
	virtual ~Operation() = default;
	
	// helpful in operationqueue.cpp
	Node& getTarget() const { 
		return target; 
	}
};

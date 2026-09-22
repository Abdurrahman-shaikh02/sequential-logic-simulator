#pragma once

#include "core/Node.hpp"
#include "core/Port.hpp"
#include "core/Signal.hpp"

class Operation {
	Node &target;
	Port p;
	Signal value;

protected:
	Operation(Node &target, Port p, Signal value);
public:
	bool operator==(const Operation &op);
	virtual void run() = 0;
};

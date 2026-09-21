#pragma once

#include "operations/Operation.hpp"
#include "core/Node.hpp"
#include "core/Signal.hpp"
#include "core/Port.hpp"

class SetPortOperation : private Operation {
public:
	SetPortOperation(Node &target, Port p, Signal value);
	void run() override;
};

#pragma once

#include "operations/Operation.hpp"
#include "core/Node.hpp"
#include "core/Signal.hpp"
#include "core/Port.hpp"

class CommitOperation : public Operation {
public:
	CommitOperation(Node &target, Port p, Signal value);
	void run() override;
};

#pragma once

#include "core/Port.hpp"
#include "core/Signal.hpp"
#include "primary/PrimaryNode.hpp"

class InputNode : public PrimaryNode {
public:
	void evaluate(Port p, Signal value) override;
};

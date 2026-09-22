#pragma once

#include "core/Port.hpp"
#include "core/Signal.hpp"
#include "sequential/SequentialNode.hpp"

class DFF_P : public SequentialNode {
	Signal d;
	Signal clk;
	void evaluate(Port p, Signal value);
	void setPort(Port p, Signal value);
	void changeState();
};

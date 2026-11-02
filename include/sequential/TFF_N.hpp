#pragma once

#include "core/Port.hpp"
#include "core/Signal.hpp"
#include "sequential/SequentialNode.hpp"

class TFF_N : public SequentialNode {
	Signal t;
	Signal clk;
public:
	TFF_N();
	void evaluate(Port p, Signal value) override;
	void setPort(Port p, Signal value) override;
	void changeState() override;
};

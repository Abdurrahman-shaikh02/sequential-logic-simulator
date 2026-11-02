#pragma once

#include "core/Port.hpp"
#include "core/Signal.hpp"
#include "sequential/SequentialNode.hpp"

class TFF_P : public SequentialNode {
	Signal t;
	Signal clk;
public:
	TFF_P();
	void evaluate(Port p, Signal value) override;
	void setPort(Port p, Signal value) override;
	void changeState() override;
};

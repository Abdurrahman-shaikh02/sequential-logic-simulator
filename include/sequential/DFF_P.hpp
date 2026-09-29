#pragma once

#include "core/Port.hpp"
#include "core/Signal.hpp"
#include "sequential/SequentialNode.hpp"

class DFF_P : public SequentialNode {
	Signal d;
	Signal clk;
public:
	DFF_P();
	void evaluate(Port p, Signal value) override;
	void setPort(Port p, Signal value) override;
	void changeState() override;
};

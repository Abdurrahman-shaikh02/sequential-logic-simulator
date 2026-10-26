#pragma once

#include "core/Port.hpp"
#include "core/Signal.hpp"
#include "sequential/SequentialNode.hpp"

class JKFF_N : public SequentialNode {
    Signal j;
    	Signal k;
    	Signal clk;
public:
    JKFF_N();
    void evaluate(Port p, Signal value) override;
    void setPort(Port p, Signal value) override;
    void changeState() override;
};

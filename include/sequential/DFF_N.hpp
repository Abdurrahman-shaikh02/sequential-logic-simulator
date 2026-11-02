#pragma once

#include "core/Port.hpp"
#include "core/Signal.hpp"
#include "sequential/SequentialNode.hpp"

class DFF_N : public SequentialNode {
    Signal d;
    Signal clk;
public:
    DFF_N();
    void evaluate(Port p, Signal value) override;
    void setPort(Port p, Signal value) override;
    void changeState() override;
};

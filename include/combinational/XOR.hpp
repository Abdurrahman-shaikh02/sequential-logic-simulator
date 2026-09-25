#pragma once

#include "combinational/CombinationalNode.hpp"

class XOR : public CombinationalNode {
    Signal A = Signal::LOW, B = Signal::LOW;
public:
    void evaluate(Port p, Signal value) override;
};
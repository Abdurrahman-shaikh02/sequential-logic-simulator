#pragma once

#include "combinational/CombinationalNode.hpp"

class MUX : public CombinationalNode {
    Signal A = Signal::LOW, B = Signal::LOW, S = Signal::LOW;
public:
    void evaluate(Port p, Signal value) override;
};
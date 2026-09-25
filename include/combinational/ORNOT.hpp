#pragma once

#include "combinational/CombinationalNode.hpp"

class ORNOT : public CombinationalNode {
    Signal A = Signal::LOW, B = Signal::LOW;
public:
    void evaluate(Port p, Signal value) override;
};
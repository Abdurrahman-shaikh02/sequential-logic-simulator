#pragma once

#include "combinational/CombinationalNode.hpp"

class NAND : public CombinationalNode {
    Signal A = Signal::LOW, B = Signal::LOW;
public:
    void evaluate(Port p, Signal value) override;
};
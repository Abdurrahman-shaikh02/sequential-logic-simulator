#pragma once

#include "combinational/CombinationalNode.hpp"

class NOT : public CombinationalNode {
    Signal A = Signal::LOW;
public:
    void evaluate(Port p, Signal value) override;
};
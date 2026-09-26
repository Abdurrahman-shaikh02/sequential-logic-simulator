#pragma once
#include "CombinationalNode.hpp"

class And : public CombinationalNode {
private:
    Signal A = Signal::LOW;
    Signal B = Signal::LOW;

public:
    void evaluate(Port p, Signal value) override {
        if (p == Port::A) A = value;
        else if (p == Port::B) B = value;

        result = (A == Signal::HIGH && B == Signal::HIGH) ? Signal::HIGH : Signal::LOW;
    }
};
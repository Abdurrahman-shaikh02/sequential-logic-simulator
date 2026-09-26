#include "combinational/ANDNOT.hpp"

void ANDNOT::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else if (p == Port::B) B = value;
    result = (A == Signal::HIGH && B == Signal::LOW) ? Signal::HIGH : Signal::LOW;
    this->commit(result);
}
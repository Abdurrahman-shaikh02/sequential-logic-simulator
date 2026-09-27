#include "combinational/OR.hpp"

void OR::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else if (p == Port::B) B = value;
    result = (A == Signal::HIGH || B == Signal::HIGH) ? Signal::HIGH : Signal::LOW;
    this->commit(result);
}

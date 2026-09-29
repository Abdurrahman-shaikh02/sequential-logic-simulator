#include "combinational/XOR.hpp"

void XOR::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else if (p == Port::B) B = value;
    result = (A != B) ? Signal::HIGH : Signal::LOW;
    this->commit(result);
}

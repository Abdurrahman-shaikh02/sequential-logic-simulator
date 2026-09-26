#include "combinational/MUX.hpp"

void MUX::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else if (p == Port::B) B = value;
    else if (p == Port::S) S = value;
    result = (S == Signal::HIGH) ? B : A;
    this->commit(result);
}
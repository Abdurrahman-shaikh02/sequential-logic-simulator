#include "combinational/NAND.hpp"

void NAND::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else if (p == Port::B) B = value;
    result = (A == Signal::HIGH && B == Signal::HIGH) ? Signal::LOW : Signal::HIGH;
    this->commit(result);
}

#include "combinational/NOT.hpp"

void NOT::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    result = (A == Signal::LOW) ? Signal::HIGH : Signal::LOW;
}
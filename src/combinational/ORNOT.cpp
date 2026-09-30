#include "combinational/ORNOT.hpp"
#include <iostream>

void ORNOT::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else if (p == Port::B) B = value;
    else std::cerr << "port does not exist on ORNOT gate\n";

    Signal temp = (A == Signal::HIGH || B == Signal::LOW) ? Signal::HIGH : Signal::LOW;
    if(temp != result){
        result = temp;
        this->commit(result);
    }
}
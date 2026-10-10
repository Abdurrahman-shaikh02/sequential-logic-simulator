#include "combinational/OR.hpp"
#include <iostream>

void OR::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else if (p == Port::B) B = value;
    else std::cerr << "port does not exist on OR gate\n";

    Signal temp = (A == Signal::HIGH || B == Signal::HIGH) ? Signal::HIGH : Signal::LOW;
    if(temp != result){
        result = temp;
        this->commit(result);
    }
}

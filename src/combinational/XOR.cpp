#include "combinational/XOR.hpp"
#include <iostream>

void XOR::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else if (p == Port::B) B = value;
    else std::cerr << "port does not exist on XOR gate\n";

    Signal temp = (A != B) ? Signal::HIGH : Signal::LOW;
    if(temp != result){
        result = temp;
        this->commit(result);
    }
}
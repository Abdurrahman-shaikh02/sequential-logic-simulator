#include "combinational/MUX.hpp"
#include <iostream>

void MUX::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else if (p == Port::B) B = value;
    else if (p == Port::S) S = value;
    else std::cerr << "port does not exist on MUX\n";
    
    Signal temp = (S == Signal::HIGH) ? B : A;
    if(temp != result){
        result = temp;
        this->commit(result);
    }
}
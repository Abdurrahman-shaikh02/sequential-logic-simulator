#include "combinational/NAND.hpp"
#include <iostream>

void NAND::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else if (p == Port::B) B = value;
    else std::cerr << "port does not exist on NAND gate\n";

    Signal temp = (A == Signal::HIGH && B == Signal::HIGH) ? Signal::LOW : Signal::HIGH;
    if(temp != result){
        result = temp;
        this->commit(result);
    }
}

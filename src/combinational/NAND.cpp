#include "operations/CommitOperation.hpp"
#include "simulation/Simulator.hpp"
#include "combinational/NAND.hpp"
#include <iostream>

void NAND::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else if (p == Port::B) B = value;
    else std::cerr << "port does not exist on NAND gate\n";

    Signal temp = (A == Signal::HIGH && B == Signal::HIGH) ? Signal::HIGH : Signal::LOW;
if(temp != result){
    result = temp;
    // Enqueue the commit operation instead of calling commit() directly
    Simulator::combinationalQueue.enqueue(std::make_unique<CommitOperation>(*this, Port::O, result));
}
}

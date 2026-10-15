#include "operations/CommitOperation.hpp"
#include "simulation/Simulator.hpp"
#include "combinational/NOR.hpp"
#include <iostream>

void NOR::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else if (p == Port::B) B = value;
    else std::cerr << "port does not exist on NOR gate\n";

    Signal temp = (A == Signal::HIGH || B == Signal::HIGH) ? Signal::LOW : Signal::HIGH;
    if (temp != result) {
        result = temp;
        Simulator::combinationalQueue.enqueue(std::make_unique<CommitOperation>(*this, Port::O, result));
    }
}
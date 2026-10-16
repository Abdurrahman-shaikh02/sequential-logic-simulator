#include "operations/CommitOperation.hpp"
#include "simulation/Simulator.hpp"
#include "combinational/NOT.hpp"
#include "simulation/Simulator.hpp"
#include <iostream>

void NOT::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else std::cerr << "port does not exist on NOT gate\n";

    // Correct NOT logic: Inverts input A
    Signal temp = (A == Signal::HIGH) ? Signal::LOW : Signal::HIGH;
    
    if (temp != result) {
        result = temp;
        Simulator::topologicalQueue.enqueue(this);
    }
}

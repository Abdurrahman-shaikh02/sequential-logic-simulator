#include "combinational/NOT.hpp"
#include "simulation/Simulator.hpp"
#include <iostream>

void NOT::evaluate(Port p, Signal value) {
    if (p == Port::A) A = value;
    else std::cerr << "port does not exist on NOT gate\n";

    Signal temp = (A == Signal::LOW) ? Signal::HIGH : Signal::LOW;
    if(temp != result){
        result = temp;
        Simulator::topologicalQueue.enqueue(this);
    }
}

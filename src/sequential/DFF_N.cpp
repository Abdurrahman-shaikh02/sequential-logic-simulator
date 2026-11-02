#include <iostream>
#include <memory>
#include "sequential/DFF_N.hpp"
#include "operations/SetPortOperation.hpp"
#include "operations/ChangeStateOperation.hpp"
#include "operations/CommitOperation.hpp"
#include "simulation/Simulator.hpp"

DFF_N::DFF_N() : d(Signal::LOW), clk(Signal::HIGH) {}

void DFF_N::evaluate(Port p, Signal value) {
    switch (p) {
        case Port::D:
            if (!initialized) d = value;
            else Simulator::sequentialQueue.enqueue(std::make_unique<SetPortOperation>(*this, p, value));
            break;
        case Port::CLK:
            if (!initialized) clk = value;
            else if (clk == Signal::HIGH && value == Signal::LOW)
                Simulator::sequentialQueue.enqueue(std::make_unique<ChangeStateOperation>(*this, p, value));
            else clk = value;
            break;
        default:
            std::cerr << "port does not exist on this D flip-flop\n";
            break;
    }
}

void DFF_N::setPort(Port p, Signal value) {
    if (p == Port::D) d = value;
}

void DFF_N::changeState() {
    clk = Signal::LOW;
    if (result == d) return;
    result = d;
    Simulator::sequentialQueue.enqueue(std::make_unique<CommitOperation>(*this, Port::O, result));
}

#include <iostream>
#include <memory>

#include "sequential/SequentialNode.hpp"
#include "operations/SetPortOperation.hpp"
#include "operations/ChangeStateOperation.hpp"
#include "operations/CommitOperation.hpp"
#include "simulation/OperationQueue.hpp"
#include "simulation/Simulator.hpp"
#include "sequential/DFF_P.hpp"

DFF_P::DFF_P() : d(Signal::LOW), clk(Signal::LOW) {}

void DFF_P::evaluate(Port p, Signal value) {
    switch (p) {
        case Port::D:
            {
                SetPortOperation pending(*this, p, value);
                Simulator::sequentialQueue.cancel(pending);
                Simulator::sequentialQueue.enqueue(std::make_unique<SetPortOperation>(*this, p, value));
                break;
            }
        case Port::CLK: 
            {
                if (clk == Signal::LOW && value == Signal::HIGH) {
                    ChangeStateOperation pending(*this, p, value);
                    Simulator::sequentialQueue.cancel(pending);
                    Simulator::sequentialQueue.enqueue(std::make_unique<ChangeStateOperation>(*this, p, value));
                } else {
                    clk = value;
                    ChangeStateOperation pending(*this, p, value);
                    Simulator::sequentialQueue.cancel(pending);
                }
                break;
            }
        default:
            std::cerr << "port does not exist on this D flip-flop\n";
            break;
    }
}

void DFF_P::setPort(Port p, Signal value) {
    if (p == Port::D) {
        d = value;
    }
}

void DFF_P::changeState() {
    clk = Signal::HIGH;
    if(result == d) return;
    result = d;
    Simulator::sequentialQueue.enqueue(std::make_unique<CommitOperation>(*this, Port::O, result));
}

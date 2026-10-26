#include <iostream>
#include <memory>
#include "sequential/JKFF_P.hpp"
#include "operations/SetPortOperation.hpp"
#include "operations/ChangeStateOperation.hpp"
#include "operations/CommitOperation.hpp"
#include "simulation/Simulator.hpp"

JKFF_P::JKFF_P() : j(Signal::LOW), k(Signal::LOW), clk(Signal::LOW) {}

void JKFF_P::evaluate(Port p, Signal value) {
    switch (p) {
        case Port::J:
        case Port::K:
            if (!initialized) { if (p == Port::J) j = value; else k = value; }
            else Simulator::sequentialQueue.enqueue(std::make_unique<SetPortOperation>(*this, p, value));
            break;
        case Port::CLK:
            if (!initialized) clk = value;
            else if (clk == Signal::LOW && value == Signal::HIGH) Simulator::sequentialQueue.enqueue(std::make_unique<ChangeStateOperation>(*this, p, value));
            else clk = value;
            break;
        default: std::cerr << "port does not exist on this JKFF_P flip-flop\n"; break;
    }
}

void JKFF_P::setPort(Port p, Signal value) { if (p == Port::J) j = value; else if (p == Port::K) k = value; }

void JKFF_P::changeState() {
    clk = Signal::HIGH;
    Signal previous = result;
    if (j == Signal::LOW && k == Signal::LOW) return;
    if (j == Signal::LOW && k == Signal::HIGH) result = Signal::LOW;
    else if (j == Signal::HIGH && k == Signal::LOW) result = Signal::HIGH;
    else result = (result == Signal::HIGH) ? Signal::LOW : Signal::HIGH;
    if (result != previous) Simulator::sequentialQueue.enqueue(std::make_unique<CommitOperation>(*this, Port::O, result));
}

#include <iostream>
#include <memory>
#include "sequential/TFF_P.hpp"
#include "operations/SetPortOperation.hpp"
#include "operations/ChangeStateOperation.hpp"
#include "operations/CommitOperation.hpp"
#include "simulation/Simulator.hpp"

TFF_P::TFF_P() : t(Signal::LOW), clk(Signal::LOW) {}

void TFF_P::evaluate(Port p, Signal value) {
    switch (p) {
        case Port::T:
            if (!initialized) t = value;
            else Simulator::sequentialQueue.enqueue(std::make_unique<SetPortOperation>(*this, p, value));
            break;
        case Port::CLK:
            if (!initialized) clk = value;
            else if (clk == Signal::LOW && value == Signal::HIGH) Simulator::sequentialQueue.enqueue(std::make_unique<ChangeStateOperation>(*this, p, value));
            else clk = value;
            break;
        default: std::cerr << "port does not exist on this TFF_P flip-flop\n"; break;
    }
}

void TFF_P::setPort(Port p, Signal value) { if (p == Port::T) t = value; }

void TFF_P::changeState() {
    clk = Signal::HIGH;
    if (t == Signal::LOW) return;
    result = (result == Signal::HIGH) ? Signal::LOW : Signal::HIGH;
    Simulator::sequentialQueue.enqueue(std::make_unique<CommitOperation>(*this, Port::O, result));
}

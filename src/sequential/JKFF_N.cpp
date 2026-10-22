#include <iostream>
#include <memory>

#include "sequential/SequentialNode.hpp"
#include "operations/SetPortOperation.hpp"
#include "operations/ChangeStateOperation.hpp"
#include "operations/CommitOperation.hpp"
#include "simulation/OperationQueue.hpp"
#include "simulation/Simulator.hpp"
#include "sequential/JKFF_N.hpp"

JKFF_N::JKFF_N() : j(Signal::LOW), k(Signal::LOW), clk(Signal::LOW) {}

void JKFF_N::evaluate(Port p, Signal value) {
    switch (p) {
        case Port::J: {
            if (!initialized) {
                j = value; // just set the internal value if the node is invalid
            } else {
                Simulator::sequentialQueue.enqueue(std::make_unique<SetPortOperation>(*this, p, value));
            }
            break;
        }
        case Port::K: {
            if (!initialized) {
                k = value; // just set the internal value if the node is invalid
            } else {
                Simulator::sequentialQueue.enqueue(std::make_unique<SetPortOperation>(*this, p, value));
            }
            break;
        }
        case Port::CLK: {
            if (!initialized) {
                clk = value;
            } else if (clk == Signal::HIGH && value == Signal::LOW) {
                Simulator::sequentialQueue.enqueue(std::make_unique<ChangeStateOperation>(*this, p, value));
            } else {
                clk = value;
            }
            break;
        }

        default:
            std::cerr << "port does not exist on this JK flip-flop\n";
            break;
    }
}

void JKFF_N::setPort(Port p, Signal value) {
    if (p == Port::J) {
        j = value;
    } else if (p == Port::K) {
        k = value;
    }
}

void JKFF_N::changeState() {
    clk = Signal::LOW;
    Signal previousResult = result;

    if (j == Signal::LOW && k == Signal::LOW) { // hold state
        return;
    } else if (j == Signal::LOW && k == Signal::HIGH) { // reset state
        result = Signal::LOW;
    } else if (j == Signal::HIGH && k == Signal::LOW) { // set state
        result = Signal::HIGH;
    } else if (j == Signal::HIGH && k == Signal::HIGH) { // toggle
        if (result == Signal::HIGH) {
            result = Signal::LOW;
        } else {
            result = Signal::HIGH;
        }
    }

    if (result == previousResult) {
        return;
    }

    Simulator::sequentialQueue.enqueue(std::make_unique<CommitOperation>(*this, Port::O, result));
}

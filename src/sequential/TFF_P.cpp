#include <iostream>
#include <memory>

#include "sequential/SequentialNode.hpp"
#include "operations/SetPortOperation.hpp"
#include "operations/ChangeStateOperation.hpp"
#include "operations/CommitOperation.hpp"
#include "simulation/OperationQueue.hpp"
#include "simulation/Simulator.hpp"
#include "sequential/TFF_P.hpp"

TFF_P::TFF_P() : t(Signal::LOW), clk(Signal::LOW) {}

void TFF_P::evaluate(Port p, Signal value) {
    switch (p) {
        case Port::T: {
                          if (!initialized) {
                              t = value;        // just set the internal value if the node is invalid
                          } else {
                              Simulator::sequentialQueue.enqueue(std::make_unique<SetPortOperation>(*this, p, value));
                          }
                          break;
                      }

        case Port::CLK: {
                            if (!initialized) {
                                clk = value;
                            } else if (clk == Signal::LOW && value == Signal::HIGH) {
                                Simulator::sequentialQueue.enqueue(std::make_unique<ChangeStateOperation>(*this, p, value));
                            } else {
                                clk = value;
                            }
                            break;
                        }

        default:
            std::cerr << "port does not exist on this T flip-flop\n";
            break;
    }
}

void TFF_P::setPort(Port p, Signal value) {
    if (p == Port::T) {
        t = value;
    }
}

void TFF_P::changeState() {
    clk = Signal::HIGH;
    if(t == Signal::LOW) return; //hold state
    else { //toggle
        if(result == Signal::HIGH) {
            result = Signal::LOW;
        }
        else {
            result = Signal::HIGH;
        }
    }
    Simulator::sequentialQueue.enqueue(std::make_unique<CommitOperation>(*this, Port::O, result));
}

#include <iostream>
#include <memory>

#include "sequential/SequentialNode.hpp"
#include "operations/SetPortOperation.hpp"
#include "operations/ChangeStateOperation.hpp"
#include "operations/CommitOperation.hpp"
#include "simulation/OperationQueue.hpp"
#include "simulation/Simulator.hpp"

class DFF_P : public SequentialNode {
private:
    Signal d;
    Signal clk;

public:
    DFF_P() : d(Signal::LOW), clk(Signal::LOW) {}

    void evaluate(Port p, Signal value) override {
        switch (p) {
            case Port::D: {
                SetPortOperation pending(*this, p, value);
		Simulator::sequentialQueue.cancel(pending);
		Simulator::sequentialQueue.enqueue(std::make_unique<SetPortOperation>(*this, p, value));
                break;
            }

            case Port::CLK: {
                if (clk == Signal::LOW && value == Signal::HIGH) {
                    ChangeStateOperation pending(*this, p, value);
                    Simulator::sequentialQueue.cancel(pending);
                    Simulator::sequentialQueue.enqueue(std::make_unique<ChangeStateOperation>(*this, p, value));
                } else {
                    clk = value;
                }
                break;
            }

            default:
                std::cerr << "port does not exist on this D flip-flop\n";
                break;
        }
    }

    void setPort(Port p, Signal value) override {
        if (p == Port::D) {
            d = value;
        }
    }

    void changeState() override {
        result = d;
        clk = Signal::HIGH;
        Simulator::sequentialQueue.enqueue(std::make_unique<CommitOperation>(*this, Port::O, result));
    }
};

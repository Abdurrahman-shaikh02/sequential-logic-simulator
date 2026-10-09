#include "simulation/Simulator.hpp"
#include "synthesizer/Synthesizer.hpp"
#include <iostream>

// initialization of static class members
OperationQueue Simulator::combinationalQueue;
OperationQueue Simulator::sequentialQueue;
std::vector<std::unique_ptr<Node>> Simulator::nodes;
int Simulator::primaryInputCount = 0;
int Simulator::primaryOutputCount = 0;

int Simulator::parse(const std::string &path) {
    // replace with Parser class call when ready
    // e.g., Parser p; return p.parse(path, nodes);
    return 0;
}

// returns the path of the generated netlist ("" on error), ready for parse()
std::string Simulator::synthesize(const std::string &path) {
    return Synthesizer::synthesize(path);
}

void Simulator::run() {

    while (!combinationalQueue.isEmpty() || !sequentialQueue.isEmpty()) {
        
        // Phase 1: Drain Combinational Queue (Gate evaluations & signal propagation)
        while (!combinationalQueue.isEmpty()) {
            std::unique_ptr<Operation> op = combinationalQueue.dequeue();
            if (op) {
                // just fyi : this is called as polymorphic execution
                op->run(); 
            }
        }

        // Phase 2: Timing check & reorder S-Queue (Prioritizes CHANGE_STATE over SET_PORT on FFs)
        sequentialQueue.reorder();

        // Phase 3: Drain Sequential Queue (Flip-flop state calculations and commits)
        while (!sequentialQueue.isEmpty()) {
            std::unique_ptr<Operation> op = sequentialQueue.dequeue();
            if (op) {
                op->run();
            }
        }
    }
}

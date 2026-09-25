#include "simulation/Simulator.hpp"
#include <iostream>

// initialization of static class members
OperationQueue Simulator::combination_queue;
OperationQueue Simulator::sequential_queue;
std::vector<std::unique_ptr<Node>> Simulator::nodes;
int Simulator::primary_input_count = 0;
int Simulator::primary_output_count = 0;

int Simulator::parse(const std::string &path) {
    // replace with Parser class call when ready
    // e.g., Parser p; return p.parse(path, nodes);
    return 0;
}

std::string Simulator::synthesize(const std::string &path) {
    // replace with Synthesizer class call when ready
    // e.g., Synthesizer syn; return syn.synthesize(path);
    return "";
}

void Simulator::run() {

    while (!combination_queue.isEmpty() || !sequential_queue.isEmpty()) {
        
        // Phase 1: Drain Combinational Queue (Gate evaluations & signal propagation)
        while (!combination_queue.isEmpty()) {
            std::unique_ptr<Operation> op = combination_queue.dequeue();
            if (op) {
                // just fyi : this is called as polymorphic execution
                op->run(); 
            }
        }

        // Phase 2: Timing check & reorder S-Queue (Prioritizes CHANGE_STATE over SET_PORT on FFs)
        sequential_queue.reorder();

        // Phase 3: Drain Sequential Queue (Flip-flop state calculations and commits)
        while (!sequential_queue.isEmpty()) {
            std::unique_ptr<Operation> op = sequential_queue.dequeue();
            if (op) {
                op->run();
            }
        }
    }
}

void Simulator::clear() {
    combination_queue.clear();
    sequential_queue.clear();
    nodes.clear();
    primary_input_count = 0;
    primary_output_count = 0;
}

#include "simulation/Simulator.hpp"
#include "parser/Parser.hpp"
#include "synthesizer/Synthesizer.hpp"

#include <iostream>

// Initialization of static class members
OperationQueue Simulator::combinationalQueue;
OperationQueue Simulator::sequentialQueue;

std::vector<std::unique_ptr<Node>> Simulator::nodes;

std::unordered_map<std::string, std::vector<Node*>> Simulator::primaryInputs;
std::unordered_map<std::string, std::vector<Node*>> Simulator::primaryOutputs;

int Simulator::parse(const std::string &path) {
    return Parser::parse(path, nodes, primaryInputs, primaryOutputs);
}

std::string Simulator::synthesize(const std::string &path) {
    return Synthesizer::synthesize(path);
}

void Simulator::run() {

    while (!combinationalQueue.isEmpty() || !sequentialQueue.isEmpty()) {

        // Phase 1: Drain Combinational Queue
        while (!combinationalQueue.isEmpty()) {
            std::unique_ptr<Operation> op = combinationalQueue.dequeue();

            if (op) {
                op->run();
            }
        }

        // Phase 2: Reorder Sequential Queue
        sequentialQueue.reorder();

        // Phase 3: Drain Sequential Queue
        while (!sequentialQueue.isEmpty()) {
            std::unique_ptr<Operation> op = sequentialQueue.dequeue();

            if (op) {
                op->run();
            }
        }
    }
}

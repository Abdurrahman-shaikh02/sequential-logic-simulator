#include "simulation/Simulator.hpp"
#include <iostream>

// initialization of static class members
OperationQueue Simulator::combinationalQueue;
OperationQueue Simulator::sequentialQueue;
TopologicalQueue Simulator::topologicalQueue;
std::vector<std::unique_ptr<Node>> Simulator::nodes;
int Simulator::primaryInputCount = 0;
int Simulator::primaryOutputCount = 0;

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

void Simulator::initialize(){
    /*   
      - turn all input nodes to 0
      - enqueue ALL the nodes in the topologicalQueue
      - run Simulator::run()
    */
}

// Expects that the evaluate() functions on the input nodes have already been called
void Simulator::run() {
    while (!(combinationalQueue.isEmpty() && sequentialQueue.isEmpty() && topologicalQueue.isEmpty())) {
        
        // Drain Combinational Queue (the InputNodes will be in the combinationalQueue)
        while (!combinationalQueue.isEmpty()) {
            std::unique_ptr<Operation> op = combinationalQueue.dequeue();
            if (op) {
                op->run(); 
            }
        }
        
        while(!(combinationalQueue.isEmpty() && topologicalQueue.isEmpty())){
            // make a commit
            topologicalQueue.commitNext();
            
            // Drain Combinational Queue
            while (!combinationalQueue.isEmpty()) {
                std::unique_ptr<Operation> op = combinationalQueue.dequeue();
                if (op) {
                    op->run(); 
                }
            }
        }

        // Timing check & reorder S-Queue (Prioritizes CHANGE_STATE over SET_PORT on FFs)
        sequentialQueue.reorder();

        // Drain Sequential Queue (Flip-flop state calculations and commits)
        while (!sequentialQueue.isEmpty()) {
            std::unique_ptr<Operation> op = sequentialQueue.dequeue();
            if (op) {
                op->run();
            }
        }
    }
}

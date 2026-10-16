#include "simulation/Simulator.hpp"
#include "synthesizer/Synthesizer.hpp"
#include "parser/Parser.hpp"
#include "operations/EvaluateOperation.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>

// initialization of static class members
OperationQueue Simulator::combinationalQueue;
OperationQueue Simulator::sequentialQueue;
TopologicalQueue Simulator::topologicalQueue;
std::vector<std::unique_ptr<Node>> Simulator::nodes;
std::unordered_map<std::string, std::vector<Node*>> Simulator::primaryInputs;
std::unordered_map<std::string, std::vector<Node*>> Simulator::primaryOutputs;

int Simulator::parse(const std::string &path) {
    return Parser::parse(path, nodes, primaryInputs, primaryOutputs);
}

// returns the path of the generated netlist ("" on error), ready for parse()
std::string Simulator::synthesize(const std::string &path) {
    return Synthesizer::synthesize(path);
}

void Simulator::initialize(){
    /*   
      - turn all input nodes to 0
      - enqueue ALL the nodes in the topologicalQueue
      - call run()
    */
    for (auto& [name, nodes] : primaryInputs) {
        for (Node* node : nodes) {
            node->evaluate(Port::O, Signal::LOW);
        }
    }

    for(auto& x : Simulator::nodes){
        topologicalQueue.enqueue(x.get());
    }

    Simulator::run();
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

// turns a name -> bits map into ordered columns (names sorted alphabetically)
static void flatten(const std::unordered_map<std::string, std::vector<Node *>> &m,
                    std::vector<Node *> &nodesOut,
                    std::vector<std::string> &labelsOut) {
    std::vector<std::string> names;
    for (auto &kv : m) names.push_back(kv.first);
    std::sort(names.begin(), names.end());

    for (auto &name : names) {
        const auto &bits = m.at(name);
        for (size_t k = 0; k < bits.size(); k++) {
            nodesOut.push_back(bits[k]);
            labelsOut.push_back(bits.size() == 1
                ? name
                : name + "[" + std::to_string(bits.size() - 1 - k) + "]");
        }
    }
}

bool Simulator::generateTruthTable(const std::string &inputPath,
                                   const std::string &outputPath) {
    std::vector<Node *> inputNodes, outputNodes;
    std::vector<std::string> inputLabels, outputLabels;
    flatten(primaryInputs,  inputNodes,  inputLabels);
    flatten(primaryOutputs, outputNodes, outputLabels);

    const int n = static_cast<int>(inputNodes.size());
    if (n <= 0 || n > 20) return false;          // 2^n rows, so keep n small

    std::ofstream inFile(inputPath);
    std::ofstream outFile(outputPath);
    if (!inFile || !outFile) return false;

    // table header
    for (auto &l : inputLabels)  std::cout << l << ' ';
    std::cout << "| ";
    for (auto &l : outputLabels) std::cout << l << ' ';
    std::cout << '\n';

    // Prime: InputNode ignores a value equal to its current one, and every node
    // starts LOW, so push HIGH once so that an all-zero first row still propagates.
    for (Node *in : inputNodes)
        combinationalQueue.enqueue(std::make_unique<EvaluateOperation>(*in, Port::O, Signal::HIGH));
    run();

    const unsigned long long rows = 1ULL << n;
    for (unsigned long long r = 0; r < rows; r++) {

        // enqueue one EvaluateOperation per primary input (first input = leftmost bit)
        for (int i = 0; i < n; i++) {
            int bit = (r >> (n - 1 - i)) & 1;
            Signal s = bit ? Signal::HIGH : Signal::LOW;
            combinationalQueue.enqueue(std::make_unique<EvaluateOperation>(*inputNodes[i], Port::O, s));

            inFile << bit << (i < n - 1 ? ' ' : '\n');
            std::cout << bit << ' ';
        }

        run();

        // read every output
        std::cout << "| ";
        for (size_t o = 0; o < outputNodes.size(); o++) {
            int v = static_cast<int>(outputNodes[o]->getResult());
            outFile << v << (o + 1 < outputNodes.size() ? ' ' : '\n');
            std::cout << v << ' ';
        }
        std::cout << '\n';
    }
    return true;
}

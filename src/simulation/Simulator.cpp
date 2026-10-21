#include "simulation/Simulator.hpp"
#include "synthesizer/Synthesizer.hpp"
#include "parser/Parser.hpp"
#include "operations/EvaluateOperation.hpp"
#include "combinational/CombinationalNode.hpp"
#include "primary/PrimaryNode.hpp"
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

void Simulator::clear() {
    combinationalQueue.clear();
    sequentialQueue.clear();
    topologicalQueue.clear();
    primaryInputs.clear();
    primaryOutputs.clear();
    nodes.clear();
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

// A circuit is sequential if it has any node that is neither primary nor combinational
static bool hasSequentialNode() {
    for (auto &n : Simulator::nodes) {
        Node *p = n.get();
        if (!dynamic_cast<CombinationalNode *>(p) && !dynamic_cast<PrimaryNode *>(p))
            return true;
    }
    return false;
}

// Returns bits ordered from msb to lsb
static void flatten(const std::unordered_map<std::string, std::vector<Node *>> &m, std::vector<Node *> &nodesOut, std::vector<std::string> &labelsOut) {
    std::vector<std::string> names;
    for (auto &kv : m) names.push_back(kv.first);
    std::sort(names.begin(), names.end());

    for (auto &name : names) {
        const auto &bits = m.at(name);
        for (size_t k = bits.size(); k-- > 0; ) {
            nodesOut.push_back(bits[k]);
            labelsOut.push_back(bits.size() == 1 ? name : name + "[" + std::to_string(k) + "]");
        }
    }
}

// Outputs truth table in the file specified in path
bool Simulator::generateTruthTable(const std::string &path) {
    // Truth table is only generated for purely combinational circuits
    if (hasSequentialNode()) {
        std::cerr << "error: circuit contains sequential components\n";
        return false;
    }

    std::vector<Node *> inputNodes, outputNodes;
    std::vector<std::string> inputLabels, outputLabels;
    flatten(primaryInputs, inputNodes, inputLabels);
    flatten(primaryOutputs, outputNodes, outputLabels);
    const size_t n = inputNodes.size();

    if (n == 0) {
        std::cerr << "error: circuit has no primary inputs\n";
        return false;
    }
    if (n > 20) {
        std::cerr << "error: circuit has " << n << " primary inputs (max 20 allowed)\n";
        return false;
    }

    std::ofstream file(path);
    if (!file) {
        std::cerr << "cannot open " << path << " for writing the truth table\n";
        return false;
    }

    // Table header
    for (auto &l : inputLabels)
        file << l << ' ';
    file << "| ";
    for (auto &l : outputLabels)
        file << l << ' ';
    file << '\n';

    initialize();

    std::vector<char> bits(n, 0);
    while (true) {
        for (size_t i = 0; i < n; i++) {
            Signal s = bits[i] ? Signal::HIGH : Signal::LOW;
            combinationalQueue.enqueue(std::make_unique<EvaluateOperation>(*inputNodes[i], Port::O, s));
            file << int(bits[i]) << ' ';
        }

        run();

        file << "| ";
        for (auto *o : outputNodes)
            file << static_cast<int>(o->getResult()) << ' ';
        file << '\n';

        // Run simulator 2^n times
        size_t i = n;
        while (i > 0 && bits[i - 1])
            bits[--i] = 0;
        if (i == 0)
            break;
        bits[i - 1] = 1;
    }
    return true;
}

#include "simulation/Simulator.hpp"
#include "parser/Parser.hpp"
#include "operations/EvaluateOperation.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>

// initialization of static class members
OperationQueue Simulator::combinationalQueue;
OperationQueue Simulator::sequentialQueue;
std::vector<std::unique_ptr<Node>> Simulator::nodes;
std::vector<Node *>      Simulator::inputNodes;
std::vector<Node *>      Simulator::outputNodes;
std::vector<std::string> Simulator::inputLabels;
std::vector<std::string> Simulator::outputLabels;
int Simulator::primaryInputCount = 0;
int Simulator::primaryOutputCount = 0;

// turns the parser's name -> bits map into ordered columns (names sorted alphabetically)
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

int Simulator::parse(const std::string &path) {
    std::unordered_map<std::string, std::vector<Node *>> pi, po;
    if (Parser::parse(path, nodes, pi, po) != 0)
        return -1;

    flatten(pi, inputNodes, inputLabels);
    flatten(po, outputNodes, outputLabels);
    primaryInputCount  = static_cast<int>(inputNodes.size());
    primaryOutputCount = static_cast<int>(outputNodes.size());
    return 0;
}

std::string Simulator::synthesize(const std::string &path) {
    // replace with Synthesizer class call when ready
    // e.g., Synthesizer syn; return syn.synthesize(path);
    return "";
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

bool Simulator::generateTruthTable(const std::string &inputPath,
                                   const std::string &outputPath) {
    const int n = primaryInputCount;
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
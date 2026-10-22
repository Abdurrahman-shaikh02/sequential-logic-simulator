#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "core/Node.hpp"
#include "simulation/OperationQueue.hpp"
#include "simulation/TopologicalQueue.hpp"

class Simulator {
public:
    // Work done by run() since the last clear(), for performance reports
    struct Stats {
        uint64_t combinationalOps = 0;   // operations drained from the combinational queue
        uint64_t topologicalCommits = 0; // nodes committed from the topological queue
        uint64_t sequentialOps = 0;      // operations drained from the sequential queue
    };
    static Stats stats;

    //the simulator instance we use can be a global object so both queues and nodes can be global...
    static OperationQueue combinationalQueue;
    static OperationQueue sequentialQueue;
    static TopologicalQueue topologicalQueue;

    static std::vector<std::unique_ptr<Node>> nodes;

    static std::unordered_map<std::string, std::vector<Node*>> primaryInputs;
    static std::unordered_map<std::string, std::vector<Node*>> primaryOutputs;

    // parsers and synthesizer entry points
    static int parse(const std::string &path);

    static std::string synthesize(const std::string &path);

    static void initialize();

    // Two-phase simulation driver
    static void run();

    // Runs 2^n input combinations and writes output to a file
    static bool generateTruthTable(const std::string &path);
    static bool generateTruthTable(std::ostream &out);

    // Reset simulation state: empties all queues and drops every node
    static void clear();
};

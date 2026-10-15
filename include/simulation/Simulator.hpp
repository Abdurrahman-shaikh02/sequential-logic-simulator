#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "core/Node.hpp"
#include "simulation/OperationQueue.hpp"

class Simulator {
public:
    //the simulator instance we use can be a global object so both queues and nodes can be global...
    static OperationQueue combinationalQueue;
    static OperationQueue sequentialQueue;

    static std::vector<std::unique_ptr<Node>> nodes;

    static std::unordered_map<std::string, std::vector<Node*>> primaryInputs;
    static std::unordered_map<std::string, std::vector<Node*>> primaryOutputs;

    // parsers and synthesizer entry points
    static int parse(const std::string &path);

    static std::string synthesize(const std::string &path);

    // Two-phase simulation driver
    static void run();

    // runs all 2^n input combinations, prints the table, writes input/output files
    static bool generateTruthTable(const std::string &inputPath,
                                   const std::string &outputPath);

    // Reset simulation state
    //static void clear();
};

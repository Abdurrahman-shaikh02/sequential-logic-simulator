#pragma once

#include <string>
#include <vector>
#include <memory>

#include "core/Node.hpp"
#include "simulation/OperationQueue.hpp"

class Simulator {
    //the simulator instance we use can be a global object so both queues and nodes can be global...
    static OperationQueue combinationalQueue;
    static OperationQueue sequentialQueue;

    static std::vector<std::unique_ptr<Node>> nodes;

    static int primaryInputCount;
    static int primaryOutputCount;

    // parsers and synthesizer entry points
    static int parse(const std::string &path);

    static std::string synthesize(const std::string &path);

    // Two-phase simulation driver
    static void run();

    // Reset simulation state
    //static void clear();
};

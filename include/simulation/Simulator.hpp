
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "core/Node.hpp"
#include "simulation/OperationQueue.hpp"

class Simulator {
public:
    static OperationQueue combinationalQueue;
    static OperationQueue sequentialQueue;

    static std::vector<std::unique_ptr<Node>> nodes;

    static std::unordered_map<std::string, std::vector<Node*>> primaryInputs;
    static std::unordered_map<std::string, std::vector<Node*>> primaryOutputs;

    static int parse(const std::string &path);
    static std::string synthesize(const std::string &path);

    static void run();

    //static void clear();
};

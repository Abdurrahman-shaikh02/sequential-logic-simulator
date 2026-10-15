#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include "core/Node.hpp"

class Parser {
public:
    static int parse(const std::string &path, std::vector<std::unique_ptr<Node>> &nodes, std::unordered_map<std::string, std::vector<Node*>> &primaryInputs, std::unordered_map<std::string, std::vector<Node*>> &primaryOutputs);
};
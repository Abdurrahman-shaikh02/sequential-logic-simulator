#pragma once

#include <string>
#include <vector>
#include <memory>
#include "core/Node.hpp"

class Parser {
public:
    static int parse(const std::string &path, std::vector<std::unique_ptr<Node>> &nodes, int &primaryInputCount, int &primaryOutputCount);
};
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <functional>
#include <vector>
#include <algorithm>
#include <iostream>

#include "parser/Parser.hpp"
#include "core/Node.hpp"
#include "core/Port.hpp"
#include "combinational/AND.hpp"
#include "combinational/OR.hpp"
#include "combinational/NAND.hpp"
#include "combinational/NOR.hpp"
#include "combinational/NOT.hpp"
#include "combinational/XOR.hpp"
#include "combinational/MUX.hpp"
#include "combinational/ANDNOT.hpp"
#include "combinational/ORNOT.hpp"
#include "sequential/DFF_P.hpp"
#include "sequential/DFF_N.hpp"
#include "sequential/TFF_P.hpp"
#include "sequential/TFF_N.hpp"
#include "sequential/JKFF_P.hpp"
#include "sequential/JKFF_N.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"

// To be modified slightly based on actual netlist syntax
// Defines structures for mapping netlist descriptors to code
// Each gate and wire should have a unique name -- circuit rejected otherwise

namespace {
    // Maps netlist ports to the Port class
    const std::unordered_map<std::string, Port> portNameTable = {
        {"A", Port::A}, {"B", Port::B}, {"S", Port::S}, {"J", Port::J}, {"K", Port::K}, {"D", Port::D}, {"T", Port::T}, {"CLK", Port::CLK}, {"Y", Port::O}, {"Q", Port::O},
    };

    struct compTypeInfo {
        std::vector<std::string> ports;
        std::function<std::unique_ptr<Node>()> create;
    };

    // Check exact names of FFs in the netlist
    const std::unordered_map<std::string, compTypeInfo> compTypeTable = {
        {"$and", {{"A","B","Y"}, [] { return std::make_unique<AND>(); }}},
        {"$or", {{"A","B","Y"}, [] { return std::make_unique<OR>(); }}},
        {"$nand", {{"A","B","Y"}, [] { return std::make_unique<NAND>(); }}},
        {"$nor", {{"A","B","Y"}, [] { return std::make_unique<NOR>(); }}},
        {"$xor", {{"A","B","Y"}, [] { return std::make_unique<XOR>(); }}},
        {"$not", {{"A","Y"}, [] { return std::make_unique<NOT>(); }}},
        {"$andnot", {{"A","B","Y"}, [] { return std::make_unique<ANDNOT>(); }}},
        {"$ornot", {{"A","B","Y"}, [] { return std::make_unique<ORNOT>(); }}},
        {"$mux", {{"A","B","S","Y"}, [] { return std::make_unique<MUX>(); }}},
        {"$dff_p", {{"D","CLK","Q"}, [] { return std::make_unique<DFF_P>(); }}},
        // {"$dff_n", {{"D","CLK","Q"}, [] { return std::make_unique<DFF_N>(); }}},
        // {"$tff_p", {{"T","CLK","Q"}, [] { return std::make_unique<TFF_P>(); }}},
        // {"$tff_n", {{"T","CLK","Q"},  [] { return std::make_unique<TFF_N>(); }}},
        // {"$jkff_p", {{"J","K","CLK","Q"}, [] { return std::make_unique<JKFF_P>(); }}},
        // {"$jkff_n", {{"J","K","CLK","Q"}, [] { return std::make_unique<JKFF_N>(); }}},
    };

    // Structure created for validation of components
    struct Component {
        Node* node;
        std::string type;
        std::vector<std::string> seenPorts;
    };

    // Wire should have a single driver and one or more consumers
    struct Wire {
        Node* driver = nullptr;
        std::vector<std::pair<Node*, Port>> consumers;
    };

    // Remove comment (if any) from and tokenize netlist line
    std::vector<std::string> tokenize(const std::string &line) {
        auto pos = line.find('#');
        std::istringstream iss(pos == std::string::npos ? line : line.substr(0, pos));

        std::vector<std::string> tokens;
        std::string tok;
        while (iss >> tok) 
            tokens.push_back(tok);
        return tokens;
    }
}

// Reads netlist from file specified in path, creates components and validates them
// On error, writes to std error and returns -1 to caller
// Creates graph connections between nodes, and performs cycle detection
// Assigns priority to each node based on topological order

int Parser::parse(const std::string &path, std::vector<std::unique_ptr<Node>> &nodes, int &primaryInputCount, int &primaryOutputCount) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "cannot open file " << path << '\n';
        return -1;
    }

    std::unordered_map<std::string, Component> components;
    std::unordered_map<std::string, Wire> wires;
    std::string moduleName;
    std::string line;

    // Read each line of the netlist and create components
    while (std::getline(file, line)) {
        auto tok = tokenize(line);
        if (tok.empty()) 
            continue;
        if (tok.size() != 6) {
            std::cerr << "expected 6 columns: " << line << '\n';
            return -1;
        }

        const std::string &mod = tok[0], &compName = tok[1], &compType = tok[2], &portName = tok[3], &dir = tok[4], &wireName = tok[5];

        if (moduleName.empty())
            moduleName = mod;
        else if (moduleName != mod) {
            std::cerr << "multiple top-level modules: " << moduleName << " & " << mod << '\n';
            return -1;
        }

        // Create a primary node if component is '-'
        if (compType == "-") {
            if (components.count(compName)) {
                std::cerr << "duplicate primary component: " << compName << '\n';
                return -1;
            }

            std::unique_ptr<Node> nodeObj;
            if (dir == "pi") {
                nodeObj = std::make_unique<InputNode>();
                primaryInputCount++;
            } else if (dir == "po") {
                nodeObj = std::make_unique<OutputNode>();
                primaryOutputCount++;
            } else {
                std::cerr << "unknown primary port direction: " << dir << '\n';
                return -1;
            }

            // Move component into Simulator::nodes
            Node* nodePtr = nodeObj.get();
            nodes.push_back(std::move(nodeObj));
            components[compName] = {nodePtr, dir, {}};

            Wire &w = wires[wireName];
            if (dir == "pi") {
                if (w.driver) {
                    std::cerr << "multiple drivers for wire " << wireName << '\n';
                    return -1;
                }
                w.driver = nodePtr;
            } else {
                w.consumers.push_back({nodePtr, Port::O});
            }
            continue;
        }

        // For all other types: validate gate type and port
        auto currType = compTypeTable.find(compType);
        if (currType == compTypeTable.end()) {
            std::cerr << "unknown component type: " << compType << '\n';
            return -1;
        }
        const compTypeInfo &info = currType->second;

        auto currPort = portNameTable.find(portName);
        if (currPort == portNameTable.end()) {
            std::cerr << "unknown port name: " << portName << '\n';
            return -1;
        }
        Port port = currPort->second;

        if (std::find(info.ports.begin(), info.ports.end(), portName) == info.ports.end()) {
            std::cerr << "port " << portName << " does not exist on " << compType << '\n';
            return -1;
        }

        // Create or fetch component
        auto currName = components.find(compName);
        if (currName == components.end()) {
            std::unique_ptr<Node> nodeObj = info.create();
            Node* nodePtr = nodeObj.get();
            nodes.push_back(std::move(nodeObj));
            currName = components.emplace(compName, Component{nodePtr, compType, {}}).first;
        } else if (currName->second.type != compType) {
            std::cerr << "component " << compName << " redefined with different type\n";
            return -1;
        }
        Component &comp = currName->second;

        if (std::find(comp.seenPorts.begin(), comp.seenPorts.end(), portName) != comp.seenPorts.end()) {
            std::cerr << "port " << portName << " redefined on component " << compName << '\n';
            return -1;
        }
        comp.seenPorts.push_back(portName);

        Wire &w = wires[wireName];
        if (dir == "out") {
            if (w.driver) {
                std::cerr << "multiple drivers for wire " << wireName << '\n';
                return -1;
            }
            w.driver = comp.node;
        } else if (dir == "in") {
            w.consumers.push_back({comp.node, port});
        } else {
            std::cerr << "unknown direction: " << dir << '\n';
            return -1;
        }
    }

    // Validate every component: all ports seen
    for (auto &[name, comp] : components) {
        auto currType = compTypeTable.find(comp.type);
        if (currType == compTypeTable.end()) 
            continue;
        for (auto &p : currType->second.ports) {
            if (std::find(comp.seenPorts.begin(), comp.seenPorts.end(), p) == comp.seenPorts.end()) {
                std::cerr << "port " << p << " missing on instance " << name << '\n';
                return -1;
            }
        }
    }

    // Validate every wire: at least one driver and consumer
    for (auto &[name, w] : wires) {
        if (!w.driver) {
            std::cerr << "wire " << name << " has no driver\n";
            return -1;
        }
        if (w.consumers.empty()) {
            std::cerr << "wire " << name << " has no consumers\n";
            return -1;
        }
    }

    // Add other checks on components and wires if required

    // Create graph by adding wire output nodes to input node's list
    for (auto &[name, w] : wires) {
        for (auto &[consumerNode, consumerPort] : w.consumers) {
            w.driver->addConnection(*consumerNode, consumerPort);
        }
    }

    // Component graph gets created (nodes owned by Simulator)
    // To be added: code for cycle detection and topo sorting
    // Uses Wires class (before/after graph creation?)

    return 0;
}
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <functional>
#include <vector>
#include <algorithm>
#include <iostream>
#include <regex>
#include <queue>

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

// Defines structures for mapping netlist descriptors to code
// Each gate and wire should have a unique name -- circuit rejected otherwise

namespace {
    const std::unordered_map<std::string, Port> portNameTable = {
        {"A", Port::A}, {"B", Port::B}, {"S", Port::S}, {"J", Port::J}, {"K", Port::K}, {"D", Port::D}, {"T", Port::T}, {"C", Port::CLK}, {"Y", Port::O}, {"Q", Port::O},
    };

    struct compTypeInfo {
        std::vector<std::string> ports;
        std::function<std::unique_ptr<Node>()> create;
    };

    // Map each netlist gate name to its lambda for component creation
    const std::unordered_map<std::string, compTypeInfo> compTypeTable = {
        {"$_AND_", {{"A","B","Y"}, [] { return std::make_unique<AND>(); }}},
        {"$_OR_", {{"A","B","Y"}, [] { return std::make_unique<OR>(); }}},
        {"$_NAND_", {{"A","B","Y"}, [] { return std::make_unique<NAND>(); }}},
        {"$_NOR_", {{"A","B","Y"}, [] { return std::make_unique<NOR>(); }}},
        {"$_XOR_", {{"A","B","Y"}, [] { return std::make_unique<XOR>(); }}},
        {"$_NOT_", {{"A","Y"}, [] { return std::make_unique<NOT>(); }}},
        {"$_ANDNOT_", {{"A","B","Y"}, [] { return std::make_unique<ANDNOT>(); }}},
        {"$_ORNOT_", {{"A","B","Y"}, [] { return std::make_unique<ORNOT>(); }}},
        {"$_MUX_", {{"A","B","S","Y"}, [] { return std::make_unique<MUX>(); }}},
        {"$_DFF_P_", {{"D","C","Q"}, [] { return std::make_unique<DFF_P>(); }}},
        // {"$_DFF_N_", {{"D","C","Q"}, [] { return std::make_unique<DFF_N>(); }}},
        // {"$_TFF_P_", {{"T","C","Q"}, [] { return std::make_unique<TFF_P>(); }}},
        // {"$_TFF_N_", {{"T","C","Q"},  [] { return std::make_unique<TFF_N>(); }}},
        // {"$_JKFF_P_", {{"J","K","C","Q"}, [] { return std::make_unique<JKFF_P>(); }}},
        // {"$_JKFF_N_", {{"J","K","C","Q"}, [] { return std::make_unique<JKFF_N>(); }}},
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

    // Find wires that are part of a multibit wire
    // Used if size has not been specified in the name initially
    // Check if wire name is of the type 'name [digit]'  
    bool parseIndexed(const std::string &name, std::string &base, int &index) {
        static const std::regex re(R"(^(.*) \[(\d+)\]$)");
        std::smatch m;

        if (!std::regex_match(name, m, re)) 
            return false;
        base = m[1];
        index = std::stoi(m[2]);
        return true;
    }

    // Wire may be defined with a range as 'name [high:low]'
    // Returns how many separate wires need to be created
    bool parseRange(const std::string &name, std::string &base, int &high, int &low) {
        static const std::regex re(R"(^(.*) \[(\d+):(\d+)\]$)");
        std::smatch m;

        if (!std::regex_match(name, m, re)) 
            return false;
        base = m[1];
        high = std::stoi(m[2]);
        low = std::stoi(m[3]);
        return true;
    }

    struct primaryNode { 
        std::string name, dir, baseWire;
    };

    // Check for constant input type in the netlist
    // The wire can be connected to a single bit
    bool isConstantLiteral(const std::string &name) {
        static const std::regex re(R"(^1'[01x]$)");
        return std::regex_match(name, re);
    }

    // Assign Signal value based on numeric netlist value
    Signal constantValue(const std::string &name) {
        return name.back() == '1' ? Signal::HIGH : Signal::LOW;
    }

    // Remove comment (if any) from and tokenize netlist line
    std::vector<std::string> tokenize(const std::string &line) {
        auto pos = line.find('#');
        std::istringstream iss(pos == std::string::npos ? line : line.substr(0, pos));

        std::vector<std::string> tokens;
        std::string tok;
        while (iss >> tok) 
            tokens.push_back(tok);

        // Allow wire name to contain spaces
        if (tokens.size() > 6) {
            std::string wireName = tokens[5];
            for (size_t i = 6; i < tokens.size(); i++)
                wireName += " " + tokens[i];
            tokens.resize(5);
            tokens.push_back(wireName);
        }
        return tokens;
    }

    // Identify sequential gate types by checking if they have a C port
    bool isSequential(const std::string &type) {
        auto it = compTypeTable.find(type);
        if (it == compTypeTable.end()) return false;
        auto &ports = it->second.ports;
        return std::find(ports.begin(), ports.end(), "C") != ports.end();
    }
}

// Reads netlist from file specified in path, creates components and validates them
// Primary inputs/ouptuts are returned as <name, vector of Nodes> (can be multibit)
// On error, writes to std error and returns -1 to caller
// Creates graph connections between nodes, and performs cycle detection
// Assigns priority to each node based on topological order

int Parser::parse(const std::string &path, std::vector<std::unique_ptr<Node>> &nodes, std::unordered_map<std::string, std::vector<Node*>> &primaryInputs, std::unordered_map<std::string, std::vector<Node*>> &primaryOutputs) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "cannot open file " << path << '\n';
        return -1;
    }

    std::unordered_map<std::string, Component> components;
    std::unordered_map<std::string, Wire> wires;
    std::vector<primaryNode> primaryNodes;
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

        // Add a primary node to creation list if component is '-'
        if (compType == "-") {
            if (components.count(compName)) {
                std::cerr << "duplicate primary component name: " << compName << '\n';
                return -1;
            }
            components[compName] = {nullptr, dir, {}};
            primaryNodes.push_back({compName, dir, wireName});
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
            // Check if input is a constant value
            // Call evalute on its connected port with the correct Signal
            if (isConstantLiteral(wireName) && !w.driver) {
                auto nodeObj = std::make_unique<InputNode>();
                Node* raw = nodeObj.get();
                nodes.push_back(std::move(nodeObj));
                raw->evaluate(Port::O, constantValue(wireName));
                w.driver = raw;
            }
            w.consumers.push_back({comp.node, port});
        } else {
            std::cerr << "unknown direction: " << dir << '\n';
            return -1;
        }
    }

    // Creation of primary node objects from temporary list
    for (auto &pnode : primaryNodes) {
        std::string base; 
        int high, low;
        std::vector<std::pair<int, std::string>> indexed;

        // If range was defined all wires need to be present in netlist
        if (parseRange(pnode.baseWire, base, high, low)) {
            for (int i = high; i >= low; i--) {
                std::string wname = base + " [" + std::to_string(i) + "]";
                if (!wires.count(wname)) {
                    std::cerr << "expected wire " << wname << " for ranged port " << pnode.name << '\n';
                    return -1;
                }
                indexed.push_back({i, wname});
            }
        } 

        // Range not specified in wire
        else {
            for (auto &[wname, w] : wires) {
                std::string b; int idx;
                if (parseIndexed(wname, b, idx) && b == pnode.baseWire)
                    indexed.push_back({idx, wname});
            }
        }

        auto &target = (pnode.dir == "pi") ? primaryInputs[pnode.name] : primaryOutputs[pnode.name];

        // Create an input or output node if the wire is not multibit
        if (indexed.empty()) {
            std::unique_ptr<Node> nodeObj = (pnode.dir == "pi") ? std::unique_ptr<Node>(std::make_unique<InputNode>()) : std::unique_ptr<Node>(std::make_unique<OutputNode>());
            Node* raw = nodeObj.get();
            nodes.push_back(std::move(nodeObj));
            target.push_back(raw);

            Wire &w = wires[pnode.baseWire];
            if (pnode.dir == "pi") {
                if (w.driver) { 
                    std::cerr << "multiple drivers for wire " << pnode.baseWire << '\n'; 
                    return -1; 
                }
                w.driver = raw;
            } else {
                w.consumers.push_back({raw, Port::O});
            }
        } 
        
        // Create a node for each bit of the indexed wire
        else {
            std::sort(indexed.begin(), indexed.end());

            // Insert each bit of wire in sorted order
            for (auto &[index, wname] : indexed) {
                std::unique_ptr<Node> nodeObj = (pnode.dir == "pi") ? std::unique_ptr<Node>(std::make_unique<InputNode>()) : std::unique_ptr<Node>(std::make_unique<OutputNode>());
                Node* raw = nodeObj.get();
                nodes.push_back(std::move(nodeObj));
                target.push_back(raw);

                Wire &w = wires[wname];
                if (pnode.dir == "pi") {
                    if (w.driver) { 
                        std::cerr << "multiple drivers for wire " << wname << '\n'; 
                        return -1; 
                    }
                    w.driver = raw;
                } else {
                    w.consumers.push_back({raw, Port::O});
                }
            }
        }
    }

    // Validate components: all ports seen
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

    // Validate wires: at least one driver for each
    // Warn if output is not being consumed
    for (auto &[name, w] : wires) {
        if (!w.driver) {
            std::cerr << "wire " << name << " has no driver\n";
            return -1;
        }
        if (w.consumers.empty())
            std::cerr << "warning: wire " << name << " has no consumers\n";
    }

    // Add other checks on components and wires if required

    // Create graph by adding wire output nodes to input node's list
    for (auto &[name, w] : wires) {
        for (auto &[consumerNode, consumerPort] : w.consumers) {
            w.driver->addConnection(*consumerNode, consumerPort);
        }
    }

    // Map each gate node to its netlist type
    std::unordered_map<Node*, std::string> nodeType;
    for (auto &[name, comp] : components)
        if (compTypeTable.count(comp.type))
            nodeType[comp.node] = comp.type;

    // Build in-degree for Kahn's algorithm excluding D/J/K/T ports
    std::unordered_map<Node*, int> indeg;
    std::unordered_map<Node*, Wire*> driverWire;
    for (auto &n : nodes)
        indeg[n.get()] = 0;

    for (auto &[name, w] : wires) {
        for (auto &[consumerNode, port] : w.consumers) {
            auto it = nodeType.find(consumerNode);
            bool seq = it != nodeType.end() && isSequential(it->second);
            if (seq && port != Port::CLK) 
                continue;
            indeg[consumerNode]++;
        }
        if (w.driver)
            driverWire[w.driver] = &w;
    }

    // Process zero-in-degree nodes then decrement downstream in-degree
    std::queue<Node*> ready;
    for (auto &n : nodes)
        if (indeg[n.get()] == 0)
            ready.push(n.get());

    std::vector<Node*> order;
    order.reserve(nodes.size());
    while (!ready.empty()) {
        Node *cur = ready.front();
        ready.pop();
        order.push_back(cur);

        auto currWire = driverWire.find(cur);
        if (currWire == driverWire.end()) 
            continue;

        for (auto &[consumerNode, port] : currWire->second->consumers) {
            auto currType = nodeType.find(consumerNode);
            bool seq = currType != nodeType.end() && isSequential(currType->second);
            if (seq && port != Port::CLK) continue;
            if (--indeg[consumerNode] == 0)
                ready.push(consumerNode);
        }
    }

    // Any leftover node is a part of a cycle
    if (order.size() != nodes.size()) {
        std::cerr << "cycle detected in circuit\n";
        return -1;
    }

    // Reorder nodes into topological order and assign each its priority
    std::unordered_map<Node*, std::unique_ptr<Node>> byPtr;
    for (auto &n : nodes)
        byPtr[n.get()] = std::move(n);

    nodes.clear();
    for (int i = 0; i < (int)order.size(); i++) {
        order[i]->priority = i;
        nodes.push_back(std::move(byPtr[order[i]]));
    }

    std::unordered_map<Node*, std::string> nameOf;
    for (auto &[name, c] : components)
        nameOf[c.node] = name;
    for (auto &[name, bits] : primaryInputs)
        for (size_t i = 0; i < bits.size(); i++)
            nameOf[bits[i]] = name + (bits.size() > 1 ? "[" + std::to_string(i) + "]" : "");
    for (auto &[name, bits] : primaryOutputs)
        for (size_t i = 0; i < bits.size(); i++)
            nameOf[bits[i]] = name + (bits.size() > 1 ? "[" + std::to_string(i) + "]" : "");

    std::cerr << "topo order:\n";
    for (auto &n : nodes) {
        auto it = nameOf.find(n.get());
        std::cerr << "  priority " << n->priority << ": "
                   << (it != nameOf.end() ? it->second : "(constant/unnamed)") << '\n';
    }

    return 0;
}

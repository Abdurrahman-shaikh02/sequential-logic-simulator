#include "synthesizer/Synthesizer.hpp"
#include "parser/Parser.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

int main() {
    const std::string input = "docs/fsm.net";

    std::cout << "[1] Synthesizing " << input << "...\n";
    const std::string netlist = Synthesizer::synthesize(input);

    if (netlist.empty()) {
        std::cerr << "FAIL: Synthesis failed.\n";
        return 1;
    }

    std::cout << "PASS: Generated netlist: " << netlist << "\n";

    std::vector<std::unique_ptr<Node>> nodes;
    std::unordered_map<std::string, std::vector<Node*>> primaryInputs;
    std::unordered_map<std::string, std::vector<Node*>> primaryOutputs;

    std::cout << "[2] Parsing generated netlist...\n";
    const int result = Parser::parse(
        netlist, nodes, primaryInputs, primaryOutputs
    );

    if (result != 0) {
        std::cerr << "FAIL: Parser returned " << result << ".\n";
        return 2;
    }

    std::cout << "PASS: Parser accepted the generated netlist.\n";
    std::cout << "Nodes: " << nodes.size() << "\n";
    std::cout << "Primary input names: " << primaryInputs.size() << "\n";
    std::cout << "Primary output names: " << primaryOutputs.size() << "\n";
    std::cout << "INTEGRATION TEST PASSED\n";

    return 0;
}

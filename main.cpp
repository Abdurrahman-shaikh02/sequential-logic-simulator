#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include "simulation/Simulator.hpp"
#include "synthesizer/Synthesizer.hpp"
#include "synthesizer/DeadStates.hpp"
#include "synthesizer/UnreachableStates.hpp"
#include "testbench/Testbench.hpp"
#include "testbench/TestSuite.hpp"

namespace fs = std::filesystem;

void printBanner() {
    std::cout << "\n======================================================================\n";
    std::cout << "             SEQUENTIAL LOGIC MULTI-WORKLET SIMULATOR (SLMS)          \n";
    std::cout << "   Worklet 1: Netlist & Combinational | Worklet 2: Sequential & FSM   \n";
    std::cout << "          Worklet 3: Waveforms, Generic Testbench & Assertions        \n";
    std::cout << "======================================================================\n\n";
}

void printHelp() {
    std::cout << "Usage:\n";
    std::cout << "  ./simulator                              Launch interactive menu\n";
    std::cout << "  ./simulator --testbench <file.tb>        Run simulation from testbench file\n";
    std::cout << "  ./simulator --interactive                Interactively enter circuit inputs\n";
    std::cout << "  ./simulator --fsm <file.net>             Synthesize FSM and analyze states\n";
    std::cout << "  ./simulator --truthtable <netlist> <out> Generate combinational truth table\n";
    std::cout << "  ./simulator --test [category]            Run software tests (all, unit, etc.)\n";
    std::cout << "  ./simulator --gui [port]                 Launch web GUI server\n";
    std::cout << "  ./simulator --help                       Display this help menu\n\n";
}


// Prompt for a netlist and let the user supply input vectors directly in the
// terminal. This uses the same Testbench event engine as file-based testing.
bool runInteractiveSimulation() {
    printBanner();
    std::cout << "INTERACTIVE CIRCUIT SIMULATION\n"
              << "Enter a synthesized netlist table, or an FSM .net file.\n"
              << "For each step, enter 0 or 1 for every primary input.\n"
              << "Enter q at the step prompt to finish and run the simulation.\n\n"
              << "Circuit path: ";
    std::string circuitPath;
    std::getline(std::cin >> std::ws, circuitPath);
    if (circuitPath.empty()) {
        std::cerr << "[-] No circuit path supplied.\n";
        return false;
    }

    slms::Testbench tb;
    if (!tb.loadCircuit(circuitPath)) {
        std::cerr << "[-] Could not load circuit. Check the path and file format.\n";
        return false;
    }

    std::vector<std::pair<std::string, int>> inputs;
    for (const auto &[name, nodes] : Simulator::primaryInputs) {
        if (nodes.size() == 1) inputs.emplace_back(name, -1);
        else for (size_t i = 0; i < nodes.size(); ++i)
            inputs.emplace_back(name, static_cast<int>(i));
    }
    std::sort(inputs.begin(), inputs.end());
    if (inputs.empty()) {
        std::cerr << "[-] No primary inputs were found in this circuit.\n";
        Simulator::clear();
        return false;
    }

    std::cout << "\nLoaded circuit successfully. Primary inputs:\n";
    for (const auto &input : inputs)
        std::cout << "  - " << input.first << (input.second >= 0 ? "[" + std::to_string(input.second) + "]" : "") << "\n";
    std::cout << "\nSimulation steps are spaced 10 time units apart.\n";

    int64_t step = 0;
    while (true) {
        std::cout << "\nStep " << (step / 10 + 1) << " (time " << step << ") — press Enter to provide inputs, or type q to finish: ";
        std::string command;
        std::getline(std::cin, command);
        if (!command.empty() && (command == "q" || command == "Q" || command == "quit")) break;
        bool valid = true;
        std::vector<int> values;
        for (const auto &input : inputs) {
            std::string label = input.first + (input.second >= 0 ? "[" + std::to_string(input.second) + "]" : "");
            while (true) {
                std::cout << "  " << label << " = ";
                std::string value;
                std::getline(std::cin, value);
                if (value == "0" || value == "1") {
                    values.push_back(value == "1" ? 1 : 0);
                    break;
                }
                if (value == "q" || value == "Q" || value == "quit") {
                    valid = false;
                    break;
                }
                std::cout << "    Please enter 0 or 1 (or q to finish).\n";
            }
            if (!valid) break;
        }
        if (!valid) break;
        for (size_t i = 0; i < inputs.size(); ++i)
            tb.addStimulus(step, inputs[i].first, values[i], inputs[i].second);
        step += 10;
    }

    if (step == 0) {
        std::cout << "[!] No input vectors entered; simulation cancelled.\n";
        Simulator::clear();
        return false;
    }

    tb.setMaxTime(step + 5);
    std::cout << "\n[+] Running simulation for " << (step / 10) << " input vector(s)...\n";
    bool ok = tb.run();
    tb.printReport();
    fs::create_directories("output");
    tb.exportVCD("output/interactive_simulation.vcd");
    tb.exportWaveform("output/interactive_waveform.txt");
    tb.exportJSON("output/interactive_waveform.json");
    std::cout << "\nWaveform files created:\n"
              << "  output/interactive_waveform.txt\n"
              << "  output/interactive_waveform.json\n"
              << "  output/interactive_simulation.vcd\n";
    std::ifstream wave("output/interactive_waveform.txt");
    if (wave) {
        std::cout << "\n--- Waveform preview ---\n";
        std::string line;
        int count = 0;
        while (std::getline(wave, line) && count++ < 30) std::cout << line << "\n";
    }
    Simulator::clear();
    return ok;
}

bool runTestbenchDemo(const std::string &tbPath) {
    std::cout << "[+] Loading testbench: " << tbPath << " ...\n";
    slms::Testbench tb;
    if (!tb.loadTestbenchFile(tbPath)) {
        std::cerr << "[-] Failed to load testbench file.\n";
        return false;
    }

    std::cout << "[+] Running event-driven simulation ...\n";
    bool ok = tb.run();

    tb.printReport();

    // Export waveforms
    fs::create_directories("output");
    std::string vcdPath = "output/simulation.vcd";
    std::string txtPath = "output/waveform.txt";
    std::string jsonPath = "output/waveform.json";

    tb.exportVCD(vcdPath);
    tb.exportWaveform(txtPath);
    tb.exportJSON(jsonPath);

    std::cout << "[+] Exported IEEE 1364 VCD  : " << vcdPath << "\n";
    std::cout << "[+] Exported ASCII Waveform : " << txtPath << "\n";
    std::cout << "[+] Exported GUI JSON       : " << jsonPath << "\n";
    std::cout << "[+] Final Testbench Result  : " << (ok ? "PASSED" : "FAILED") << "\n\n";
    return ok;
}

void runFsmAnalysis(const std::string &fsmPath) {
    std::cout << "[+] Analyzing FSM: " << fsmPath << " ...\n";
    std::vector<std::string> dead, unreachable;
    std::cout << "\n--- Dead States --- \n";
    DeadStates::find(fsmPath, dead, true);

    std::cout << "\n--- Unreachable States ---\n";
    UnreachableStates::find(fsmPath, unreachable, true);

    std::cout << "\n[+] Synthesizing FSM into flip-flops and gates ...\n";
    std::string table = Simulator::synthesize(fsmPath);
    if (!table.empty()) {
        std::cout << "[+] Synthesized Netlist Table: " << table << "\n\n";
    } else {
        std::cerr << "[-] FSM Synthesis failed.\n\n";
    }
}

void runTruthTable(const std::string &netlistPath, const std::string &outputPath) {
    std::cout << "[+] Parsing netlist: " << netlistPath << " ...\n";
    Simulator::clear();
    int rc = Simulator::parse(netlistPath);
    if (rc != 0) {
        std::cerr << "[-] Failed to parse netlist.\n";
        return;
    }

    std::cout << "[+] Generating truth table ...\n";
    if (Simulator::generateTruthTable(outputPath)) {
        std::cout << "[+] Truth table written to: " << outputPath << "\n";
        std::ifstream ifs(outputPath);
        std::string line;
        int count = 0;
        std::cout << "--- Preview ---\n";
        while (std::getline(ifs, line) && count++ < 10) {
            std::cout << line << "\n";
        }
        if (count >= 10) std::cout << "... (truncated)\n";
    } else {
        std::cerr << "[-] Truth table generation failed.\n";
    }
    Simulator::clear();
}

bool runTestSuite(const std::string &category) {
    slms::TestSuite suite;
    if (category == "unit") {
        suite.runUnitTests();
    } else if (category == "integration") {
        suite.runIntegrationTests();
    } else if (category == "validation") {
        suite.runValidationTests();
    } else if (category == "blackbox") {
        suite.runBlackBoxTests();
    } else if (category == "whitebox") {
        suite.runWhiteBoxTests();
    } else if (category == "regression") {
        suite.runRegressionTests();
    } else if (category == "system") {
        suite.runSystemTests();
    } else {
        suite.runAll();
    }
    suite.printReport();
    return suite.allPassed();
}

void launchGui(int port) {
    std::cout << "[+] Launching SLMS Web GUI Server on port " << port << " ...\n";
    std::cout << "[+] Opening URL: http://localhost:" << port << "\n";
    std::string cmd = "python3 gui/server.py " + std::to_string(port);
    std::system(cmd.c_str());
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        } else if (arg == "--testbench" && argc >= 3) {
            return runTestbenchDemo(argv[2]) ? 0 : 1;
        } else if (arg == "--interactive") {
            return runInteractiveSimulation() ? 0 : 1;
        } else if (arg == "--fsm" && argc >= 3) {
            runFsmAnalysis(argv[2]);
            return 0;
        } else if (arg == "--truthtable" && argc >= 4) {
            runTruthTable(argv[2], argv[3]);
            return 0;
        } else if (arg == "--test") {
            std::string cat = (argc >= 3) ? argv[2] : "all";
            return runTestSuite(cat) ? 0 : 1;
        } else if (arg == "--gui") {
            int port = (argc >= 3) ? std::stoi(argv[2]) : 8080;
            launchGui(port);
            return 0;
        }
    }

    // Interactive CLI mode
    printBanner();
    while (true) {
        std::cout << "Select an Option:\n";
        std::cout << "  1. Run Traffic Light FSM Testbench (Full Demonstration)\n";
        std::cout << "  2. Run Custom Testbench File (.tb)\n";
        std::cout << "  3. Interactive Terminal Circuit Simulation (enter inputs manually)\n";
        std::cout << "  4. Synthesize FSM and Analyze Dead/Unreachable States\n";
        std::cout << "  5. Generate Combinational Truth Table\n";
        std::cout << "  6. Run Complete Software Testing Suite (All 7 Test Types)\n";
        std::cout << "  7. Launch Graphical User Interface (Web GUI)\n";
        std::cout << "  8. Exit\n";
        std::cout << "Enter choice (1-8): ";

        int choice = 0;
        if (!(std::cin >> choice)) {
            break;
        }

        if (choice == 1) {
            runTestbenchDemo("examples/traffic_light/traffic_light.tb");
        } else if (choice == 2) {
            std::cout << "Enter path to testbench file: ";
            std::string p;
            std::cin >> p;
            runTestbenchDemo(p);
        } else if (choice == 3) {
            runInteractiveSimulation();
        } else if (choice == 4) {
            std::cout << "Enter path to FSM file (e.g., docs/fsm_example/dead_unreachable_fsm.net): ";
            std::string p;
            std::cin >> p;
            runFsmAnalysis(p);
        } else if (choice == 5) {
            std::cout << "Enter path to combinational netlist table: ";
            std::string net;
            std::cin >> net;
            std::cout << "Enter output table file path: ";
            std::string out;
            std::cin >> out;
            runTruthTable(net, out);
        } else if (choice == 6) {
            runTestSuite("all");
        } else if (choice == 7) {
            launchGui(8080);
        } else if (choice == 8) {
            std::cout << "Exiting SLMS. Goodbye!\n";
            break;
        } else {
            std::cout << "[!] Invalid choice, please try again.\n";
        }
    }

    return 0;
}

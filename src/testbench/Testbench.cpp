#include "testbench/Testbench.hpp"
#include "simulation/Simulator.hpp"
#include "parser/Parser.hpp"
#include "synthesizer/Synthesizer.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace slms {

namespace {
    std::string trim(const std::string &s) {
        auto b = s.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) return "";
        auto e = s.find_last_not_of(" \t\r\n");
        return s.substr(b, e - b + 1);
    }

    // Small, generic Boolean expression evaluator for testbench INVARIANTs.
    // Supported grammar: !, &&, ||, parentheses, signal == 0/1, signal != 0/1,
    // and bare signal names. Unknown signal names cause the invariant to fail.
    class BooleanExpression {
        std::string text;
        size_t pos = 0;
        const std::unordered_map<std::string, Signal>& signals;
        void ws() { while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos; }
        bool consume(const std::string& token) { ws(); if (text.compare(pos, token.size(), token) == 0) { pos += token.size(); return true; } return false; }
        std::string identifier() {
            ws(); size_t start = pos;
            while (pos < text.size() && (std::isalnum(static_cast<unsigned char>(text[pos])) || text[pos]=='_' || text[pos]=='[' || text[pos]==']')) ++pos;
            if (start == pos) throw std::runtime_error("expected signal name in invariant");
            return text.substr(start, pos-start);
        }
        bool atom() {
            ws();
            if (consume("(")) { bool v = disjunction(); if (!consume(")")) throw std::runtime_error("missing ')' in invariant"); return v; }
            std::string name = identifier();
            auto it = signals.find(name);
            if (it == signals.end()) throw std::runtime_error("unknown signal in invariant: " + name);
            bool value = it->second == Signal::HIGH;
            if (consume("==")) { ws(); if (pos >= text.size() || (text[pos] != '0' && text[pos] != '1')) throw std::runtime_error("expected 0 or 1 in invariant"); bool rhs=text[pos++]=='1'; return value==rhs; }
            if (consume("!=")) { ws(); if (pos >= text.size() || (text[pos] != '0' && text[pos] != '1')) throw std::runtime_error("expected 0 or 1 in invariant"); bool rhs=text[pos++]=='1'; return value!=rhs; }
            return value;
        }
        bool unary() { if (consume("!")) return !unary(); return atom(); }
        bool conjunction() { bool v=unary(); while (consume("&&")) { bool rhs=unary(); v = v && rhs; } return v; }
        bool disjunction() { bool v=conjunction(); while (consume("||")) { bool rhs=conjunction(); v = v || rhs; } return v; }
    public:
        BooleanExpression(const std::string& e, const std::unordered_map<std::string, Signal>& s): text(e), signals(s) {}
        bool evaluate() { bool v=disjunction(); ws(); if (pos != text.size()) throw std::runtime_error("unexpected token in invariant near: " + text.substr(pos)); return v; }
    };
}

Testbench::Testbench()
    : isSynthesizedFSM(false), maxTime(100), isInitialized(false), verbose(true) {}

Testbench::~Testbench() {
    reset();
}

void Testbench::reset() {
    Simulator::clear();
    waveform.clear();
    assertionEngine.clear();
    stimuli.clear();
    assertions.clear();
    clockDef.enabled = false;
    isInitialized = false;
}

bool Testbench::loadCircuit(const std::string &filePath) {
    circuitPath = filePath;
    Simulator::clear();

    // Check if it's an FSM file (.net)
    if (filePath.size() >= 4 && filePath.substr(filePath.size() - 4) == ".net") {
        isSynthesizedFSM = true;
        tablePath = Simulator::synthesize(filePath);
        if (tablePath.empty()) {
            std::cerr << "[Testbench] Error: FSM synthesis failed for " << filePath << "\n";
            return false;
        }
    } else {
        isSynthesizedFSM = false;
        tablePath = filePath;
    }

    int rc = Simulator::parse(tablePath);
    if (rc != 0) {
        std::cerr << "[Testbench] Error: Parser failed for " << tablePath << "\n";
        return false;
    }

    // Register all primary signals in WaveformGenerator
    std::vector<std::string> sigs;
    for (const auto &[name, nodes] : Simulator::primaryInputs) {
        if (nodes.size() == 1) {
            sigs.push_back(name);
        } else {
            for (size_t i = 0; i < nodes.size(); ++i) {
                sigs.push_back(name + "[" + std::to_string(i) + "]");
            }
        }
    }
    for (const auto &[name, nodes] : Simulator::primaryOutputs) {
        if (nodes.size() == 1) {
            sigs.push_back(name);
        } else {
            for (size_t i = 0; i < nodes.size(); ++i) {
                sigs.push_back(name + "[" + std::to_string(i) + "]");
            }
        }
    }
    waveform.registerSignals(sigs);

    return true;
}

void Testbench::setClock(const std::string &name, int64_t period, int64_t highDuration) {
    clockDef.signalName = name;
    clockDef.period = period > 0 ? period : 10;
    clockDef.dutyHigh = (highDuration > 0 && highDuration < clockDef.period) ? highDuration : std::max<int64_t>(1, clockDef.period / 2);
    clockDef.enabled = true;
    waveform.registerSignal(name);
}

void Testbench::addStimulus(int64_t time, const std::string &signalName, Signal value, int bitIndex) {
    stimuli.push_back({time, signalName, bitIndex, value});
    if (time > maxTime) maxTime = time + 10;
}

void Testbench::addStimulus(int64_t time, const std::string &signalName, int value, int bitIndex) {
    addStimulus(time, signalName, value != 0 ? Signal::HIGH : Signal::LOW, bitIndex);
}

void Testbench::addAssertion(int64_t time, const std::string &signalName, Signal expected, const std::string &desc, int bitIndex) {
    assertions.push_back({time, signalName, bitIndex, expected, desc});
    if (time > maxTime) maxTime = time + 10;
}

void Testbench::addAssertion(int64_t time, const std::string &signalName, int expected, const std::string &desc, int bitIndex) {
    addAssertion(time, signalName, expected != 0 ? Signal::HIGH : Signal::LOW, desc, bitIndex);
}

void Testbench::addInvariant(const std::string &name,
                             std::function<bool(const std::unordered_map<std::string, Signal>&, int64_t)> checkFn,
                             const std::string &desc) {
    assertionEngine.addInvariant(name, checkFn, desc);
}

void Testbench::applySignalValue(const std::string &name, int bitIndex, Signal val) {
    auto it = Simulator::primaryInputs.find(name);
    if (it == Simulator::primaryInputs.end()) {
        throw std::invalid_argument("Unknown primary input: " + name);
    }
    if (bitIndex < 0) {
        // Scalar or all bits
        for (auto *n : it->second) {
            n->evaluate(Port::O, val);
        }
    } else {
        if (static_cast<size_t>(bitIndex) >= it->second.size()) {
            throw std::out_of_range("Invalid input bit index for " + name + ": " + std::to_string(bitIndex));
        }
        it->second[bitIndex]->evaluate(Port::O, val);
    }
}

Signal Testbench::readSignalValue(const std::string &name, int bitIndex) const {
    auto outIt = Simulator::primaryOutputs.find(name);
    if (outIt != Simulator::primaryOutputs.end()) {
        if (bitIndex < 0 && !outIt->second.empty()) {
            return outIt->second[0]->getResult();
        } else if (static_cast<size_t>(bitIndex) < outIt->second.size()) {
            return outIt->second[bitIndex]->getResult();
        }
    }

    auto inIt = Simulator::primaryInputs.find(name);
    if (inIt != Simulator::primaryInputs.end()) {
        if (bitIndex < 0 && !inIt->second.empty()) {
            return inIt->second[0]->getResult();
        } else if (static_cast<size_t>(bitIndex) < inIt->second.size()) {
            return inIt->second[bitIndex]->getResult();
        }
    }

    throw std::out_of_range("Unknown signal or invalid bit index: " + name + "[" + std::to_string(bitIndex) + "]");
}

bool Testbench::loadTestbenchFile(const std::string &tbPath) {
    std::ifstream ifs(tbPath);
    if (!ifs.is_open()) {
        std::cerr << "[Testbench] Cannot open testbench file: " << tbPath << "\n";
        return false;
    }

    std::string line;
    int lineNo = 0;
    while (std::getline(ifs, line)) {
        lineNo++;
        line = trim(line.substr(0, line.find('#')));
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "CIRCUIT" || token == "NETLIST" || token == "FSM") {
            std::string cPath;
            iss >> cPath;
            if (!loadCircuit(cPath)) {
                return false;
            }
        } else if (token == "CLOCK") {
            std::string clkName;
            int64_t period = 10, duty = -1;
            iss >> clkName >> period;
            if (iss >> duty) {}
            setClock(clkName, period, duty);
        } else if (token == "MAX_TIME") {
            int64_t t;
            iss >> t;
            maxTime = t;
        } else if (token[0] == '@') {
            // @<time> sig=val sig=val ...
            int64_t t = std::stoll(token.substr(1));
            std::string assign;
            while (iss >> assign) {
                auto eq = assign.find('=');
                if (eq != std::string::npos) {
                    std::string sName = assign.substr(0, eq);
                    int val = std::stoi(assign.substr(eq + 1));
                    addStimulus(t, sName, val);
                }
            }
        } else if (token == "ASSERT") {
            // ASSERT @<time> sig=val [sig2=val2] ["message"]
            std::string atTime;
            iss >> atTime;
            int64_t t = 0;
            if (atTime[0] == '@') {
                t = std::stoll(atTime.substr(1));
            } else {
                t = std::stoll(atTime);
            }

            std::string rest;
            std::getline(iss, rest);
            rest = trim(rest);

            // Extract message if in quotes
            std::string msg = "";
            auto quote1 = rest.find('"');
            if (quote1 != std::string::npos) {
                auto quote2 = rest.find('"', quote1 + 1);
                if (quote2 != std::string::npos) {
                    msg = rest.substr(quote1 + 1, quote2 - quote1 - 1);
                    rest = trim(rest.substr(0, quote1));
                }
            }

            std::istringstream assStream(rest);
            std::string assign;
            while (assStream >> assign) {
                auto eq = assign.find('=');
                if (eq != std::string::npos) {
                    std::string sName = assign.substr(0, eq);
                    int val = std::stoi(assign.substr(eq + 1));
                    addAssertion(t, sName, val, msg);
                }
            }
        } else if (token == "INVARIANT") {
            // Simple invariant definition: INVARIANT <name> !(sig1 == 1 && sig2 == 1) "desc"
            std::string invName;
            iss >> invName;
            std::string invBody;
            std::getline(iss, invBody);
            invBody = trim(invBody);

            std::string msg = invName;
            auto quote1 = invBody.find('"');
            if (quote1 != std::string::npos) {
                auto quote2 = invBody.find('"', quote1 + 1);
                if (quote2 != std::string::npos) {
                    msg = invBody.substr(quote1 + 1, quote2 - quote1 - 1);
                }
            }

            // Generic Boolean invariant: remove the optional quoted description,
            // then evaluate the expression against each timestamp's signal snapshot.
            std::string expression = invBody;
            if (quote1 != std::string::npos) expression = trim(invBody.substr(0, quote1));
            if (!expression.empty()) {
                addInvariant(invName, [expression](const std::unordered_map<std::string, Signal> &st, int64_t) {
                    try { return BooleanExpression(expression, st).evaluate(); }
                    catch (const std::exception &e) { std::cerr << "[Testbench] Invalid invariant expression: " << e.what() << "\n"; return false; }
                }, msg);
            } else {
                std::cerr << "[Testbench] Warning: empty invariant expression for " << invName << " at line " << lineNo << "\n";
            }
        }
    }

    return true;
}

bool Testbench::run(int64_t duration) {
    if (tablePath.empty()) {
        std::cerr << "[Testbench] Error: No circuit loaded to simulate\n";
        return false;
    }

    if (duration > 0) {
        maxTime = duration;
    }

    // 1. Gather all discrete event timestamps
    std::set<int64_t> eventTimes;
    eventTimes.insert(0);

    for (const auto &st : stimuli) {
        if (st.timestamp <= maxTime) eventTimes.insert(st.timestamp);
    }
    for (const auto &as : assertions) {
        if (as.timestamp <= maxTime) eventTimes.insert(as.timestamp);
    }

    // Add clock toggle timestamps
    if (clockDef.enabled && clockDef.period > 0) {
        // At t=k*period the clock falls LOW; at t=k*period+dutyHigh it rises HIGH.
        // Schedule BOTH edges; don't step by the high duration alone.
        for (int64_t t = 0; t <= maxTime; ) {
            eventTimes.insert(t);
            if (clockDef.dutyHigh <= maxTime - t) eventTimes.insert(t + clockDef.dutyHigh);
            if (clockDef.period > maxTime - t) break; // also avoids overflow
            t += clockDef.period;
        }
        eventTimes.insert(maxTime);
    }

    // 2. Initialize circuit (turn inputs to LOW, topological sweep)
    Simulator::initialize();
    isInitialized = true;

    // 3. Event-driven simulation over time; invalid signals must FAIL, not pass as zero.
    try {
    for (int64_t t : eventTimes) {
        // A) Apply clock edge if enabled
        if (clockDef.enabled) {
            int64_t phase = t % clockDef.period;
            Signal clkVal = (phase >= clockDef.dutyHigh) ? Signal::HIGH : Signal::LOW;
            applySignalValue(clockDef.signalName, -1, clkVal);
        }

        // B) Apply scheduled stimuli at this timestamp
        for (const auto &st : stimuli) {
            if (st.timestamp == t) {
                applySignalValue(st.signalName, st.bitIndex, st.value);
            }
        }

        // C) Drain event queues (Combinational + Topological + Sequential)
        Simulator::run();

        // D) Snapshot circuit signals
        std::unordered_map<std::string, Signal> currentSnapshot;

        // Sample Primary Inputs
        for (const auto &[name, nodes] : Simulator::primaryInputs) {
            if (nodes.size() == 1) {
                currentSnapshot[name] = nodes[0]->getResult();
            } else {
                for (size_t i = 0; i < nodes.size(); ++i) {
                    currentSnapshot[name + "[" + std::to_string(i) + "]"] = nodes[i]->getResult();
                }
            }
        }

        // Sample Primary Outputs
        for (const auto &[name, nodes] : Simulator::primaryOutputs) {
            if (nodes.size() == 1) {
                currentSnapshot[name] = nodes[0]->getResult();
            } else {
                for (size_t i = 0; i < nodes.size(); ++i) {
                    currentSnapshot[name + "[" + std::to_string(i) + "]"] = nodes[i]->getResult();
                }
            }
        }

        // Record snapshot in waveform
        waveform.recordSnapshot(t, currentSnapshot);

        // E) Check assertions scheduled at time t
        for (const auto &as : assertions) {
            if (as.timestamp == t) {
                Signal actual = readSignalValue(as.signalName, as.bitIndex);
                assertionEngine.assertSignal(t, as.signalName, actual, as.expected, as.description);
            }
        }

        // F) Evaluate safety invariants
        assertionEngine.evaluateInvariants(t, currentSnapshot);
    }

    } catch (const std::exception &e) {
        std::cerr << "[Testbench] Simulation failed: " << e.what() << "\n";
        return false;
    }
    return assertionEngine.allPassed();
}

bool Testbench::exportVCD(const std::string &vcdPath) const {
    return waveform.writeVCD(vcdPath);
}

bool Testbench::exportWaveform(const std::string &txtPath, int stepWidth) const {
    return waveform.writeTextDiagram(txtPath, stepWidth);
}

bool Testbench::exportTable(const std::string &tablePath) const {
    return waveform.writeTable(tablePath);
}

bool Testbench::exportJSON(const std::string &jsonPath) const {
    return waveform.writeJSON(jsonPath);
}

void Testbench::printReport(std::ostream &os) const {
    os << "\n";
    waveform.printTextDiagram(os);
    assertionEngine.printReport(os);
}

} // namespace slms

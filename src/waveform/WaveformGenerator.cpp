#include "waveform/WaveformGenerator.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <ctime>

namespace slms {

std::string WaveformGenerator::getVcdId(size_t index) {
    std::string id;
    do {
        id += static_cast<char>('!' + (index % 94));
        index /= 94;
    } while (index > 0);
    return id;
}

WaveformGenerator::WaveformGenerator(const std::string &module)
    : moduleName(module) {}

void WaveformGenerator::registerSignal(const std::string &name) {
    if (std::find(signalNames.begin(), signalNames.end(), name) == signalNames.end()) {
        signalNames.push_back(name);
    }
}

void WaveformGenerator::registerSignals(const std::vector<std::string> &names) {
    for (const auto &n : names) {
        registerSignal(n);
    }
}

void WaveformGenerator::recordSignal(int64_t timestamp, const std::string &name, Signal val) {
    registerSignal(name);
    timeline[timestamp][name] = val;
}

void WaveformGenerator::recordSnapshot(int64_t timestamp, const std::unordered_map<std::string, Signal> &snapshot) {
    for (const auto &[name, val] : snapshot) {
        registerSignal(name);
        timeline[timestamp][name] = val;
    }
}

void WaveformGenerator::clear() {
    signalNames.clear();
    timeline.clear();
}

// -------------------------------------------------------------
// IEEE 1364 Value Change Dump (VCD) Generation
// -------------------------------------------------------------
void WaveformGenerator::printVCD(std::ostream &os) const {
    // Current timestamp for header
    std::time_t now = std::time(nullptr);
    char dateBuf[100];
    std::strftime(dateBuf, sizeof(dateBuf), "%c", std::localtime(&now));

    os << "$date\n   " << dateBuf << "\n$end\n";
    os << "$version\n   Sequential Logic Simulator (SLMS) Waveform Generator\n$end\n";
    os << "$timescale 1ns $end\n";
    os << "$scope module " << moduleName << " $end\n";

    std::unordered_map<std::string, std::string> signalToId;
    for (size_t i = 0; i < signalNames.size(); ++i) {
        std::string id = getVcdId(i);
        signalToId[signalNames[i]] = id;
        os << "$var wire 1 " << id << " " << signalNames[i] << " $end\n";
    }

    os << "$upscope $end\n";
    os << "$enddefinitions $end\n";

    if (timeline.empty()) return;

    // Initial values at time 0 or first timestamp
    auto it = timeline.begin();
    int64_t firstTime = it->first;

    os << "#" << firstTime << "\n";
    os << "$dumpvars\n";

    // Track previous values for change dumping
    std::unordered_map<std::string, Signal> prevValues;
    for (const auto &sig : signalNames) {
        Signal val = Signal::LOW;
        if (it->second.count(sig)) {
            val = it->second.at(sig);
        }
        prevValues[sig] = val;
        os << (val == Signal::HIGH ? '1' : '0') << signalToId.at(sig) << "\n";
    }
    os << "$end\n";

    // Advance to subsequent timestamps
    ++it;
    for (; it != timeline.end(); ++it) {
        int64_t t = it->first;
        bool timePrinted = false;

        for (const auto &sig : signalNames) {
            if (it->second.count(sig)) {
                Signal cur = it->second.at(sig);
                if (cur != prevValues[sig]) {
                    if (!timePrinted) {
                        os << "#" << t << "\n";
                        timePrinted = true;
                    }
                    os << (cur == Signal::HIGH ? '1' : '0') << signalToId.at(sig) << "\n";
                    prevValues[sig] = cur;
                }
            }
        }
    }
}

bool WaveformGenerator::writeVCD(const std::string &filePath) const {
    std::ofstream ofs(filePath);
    if (!ofs.is_open()) return false;
    printVCD(ofs);
    return true;
}

// -------------------------------------------------------------
// Graphical ASCII Waveform Diagram
// -------------------------------------------------------------
void WaveformGenerator::printTextDiagram(std::ostream &os, int stepWidth) const {
    if (timeline.empty() || signalNames.empty()) {
        os << "(No waveform data to display)\n";
        return;
    }

    // Collect all timestamps
    std::vector<int64_t> times;
    for (const auto &[t, _] : timeline) {
        times.push_back(t);
    }

    // Determine max signal name length for alignment
    size_t maxLabelLen = 10;
    for (const auto &sig : signalNames) {
        maxLabelLen = std::max(maxLabelLen, sig.length());
    }

    os << "\n" << std::string(maxLabelLen + 10 + times.size() * stepWidth, '=') << "\n";
    os << "                  SLMS DIGITAL TIMING WAVEFORM\n";
    os << std::string(maxLabelLen + 10 + times.size() * stepWidth, '=') << "\n\n";

    // 1. Time axis
    os << std::setw(maxLabelLen + 2) << std::left << "Time(ns)" << " : ";
    for (size_t i = 0; i < times.size(); ++i) {
        std::string tStr = std::to_string(times[i]);
        if (tStr.length() < static_cast<size_t>(stepWidth)) {
            tStr.append(stepWidth - tStr.length(), ' ');
        }
        os << tStr;
    }
    os << "\n";

    // Separator line
    os << std::setw(maxLabelLen + 2) << std::left << "" << " : ";
    for (size_t i = 0; i < times.size(); ++i) {
        os << "+" << std::string(stepWidth - 1, '-');
    }
    os << "+\n";

    // 2. Waveform for each signal
    // High level: "---" with '/' on rise
    // Low level:  "___" with '\' on fall
    for (const auto &sig : signalNames) {
        os << std::setw(maxLabelLen + 2) << std::left << sig << " : ";
        Signal prevSig = Signal::LOW;
        bool isFirst = true;

        for (int64_t t : times) {
            Signal curSig = Signal::LOW;
            const auto &state = timeline.at(t);
            if (state.count(sig)) {
                curSig = state.at(sig);
            }

            if (isFirst) {
                if (curSig == Signal::HIGH) {
                    os << "^" << std::string(stepWidth - 1, '-');
                } else {
                    os << "_" << std::string(stepWidth - 1, '_');
                }
                isFirst = false;
            } else {
                if (prevSig == Signal::LOW && curSig == Signal::HIGH) {
                    // Rising edge
                    os << "/" << std::string(stepWidth - 1, '-');
                } else if (prevSig == Signal::HIGH && curSig == Signal::LOW) {
                    // Falling edge
                    os << "\\" << std::string(stepWidth - 1, '_');
                } else if (curSig == Signal::HIGH) {
                    // Constant High
                    os << std::string(stepWidth, '-');
                } else {
                    // Constant Low
                    os << std::string(stepWidth, '_');
                }
            }
            prevSig = curSig;
        }
        os << "\n";
    }
    os << "\nLegend: '-' = HIGH (1), '_' = LOW (0), '/' = Rising Edge, '\\' = Falling Edge\n\n";
}

bool WaveformGenerator::writeTextDiagram(const std::string &filePath, int stepWidth) const {
    std::ofstream ofs(filePath);
    if (!ofs.is_open()) return false;
    printTextDiagram(ofs, stepWidth);
    return true;
}

// -------------------------------------------------------------
// Tabular Output (docs/out_format.txt)
// -------------------------------------------------------------
void WaveformGenerator::printTable(std::ostream &os) const {
    if (timeline.empty() || signalNames.empty()) return;

    os << std::setw(8) << std::left << "Time";
    for (const auto &sig : signalNames) {
        os << "\t" << sig;
    }
    os << "\n";

    for (const auto &[t, sigMap] : timeline) {
        os << std::setw(8) << std::left << t;
        for (const auto &sig : signalNames) {
            int val = 0;
            if (sigMap.count(sig)) {
                val = (sigMap.at(sig) == Signal::HIGH) ? 1 : 0;
            }
            os << "\t" << val;
        }
        os << "\n";
    }
}

bool WaveformGenerator::writeTable(const std::string &filePath) const {
    std::ofstream ofs(filePath);
    if (!ofs.is_open()) return false;
    printTable(ofs);
    return true;
}

// -------------------------------------------------------------
// JSON Output for Web GUI
// -------------------------------------------------------------
std::string WaveformGenerator::toJSON() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"module\": \"" << moduleName << "\",\n";
    
    // Signals
    ss << "  \"signals\": [";
    for (size_t i = 0; i < signalNames.size(); ++i) {
        ss << "\"" << signalNames[i] << "\"" << (i + 1 < signalNames.size() ? ", " : "");
    }
    ss << "],\n";

    // Timestamps
    std::vector<int64_t> times;
    for (const auto &[t, _] : timeline) {
        times.push_back(t);
    }
    ss << "  \"times\": [";
    for (size_t i = 0; i < times.size(); ++i) {
        ss << times[i] << (i + 1 < times.size() ? ", " : "");
    }
    ss << "],\n";

    // Data per signal: "sig": [0, 1, 1, 0...]
    ss << "  \"data\": {\n";
    for (size_t i = 0; i < signalNames.size(); ++i) {
        const auto &sig = signalNames[i];
        ss << "    \"" << sig << "\": [";
        for (size_t j = 0; j < times.size(); ++j) {
            int64_t t = times[j];
            int val = 0;
            if (timeline.at(t).count(sig)) {
                val = (timeline.at(t).at(sig) == Signal::HIGH) ? 1 : 0;
            }
            ss << val << (j + 1 < times.size() ? ", " : "");
        }
        ss << "]" << (i + 1 < signalNames.size() ? ",\n" : "\n");
    }
    ss << "  }\n";
    ss << "}\n";
    return ss.str();
}

bool WaveformGenerator::writeJSON(const std::string &filePath) const {
    std::ofstream ofs(filePath);
    if (!ofs.is_open()) return false;
    ofs << toJSON();
    return true;
}

} // namespace slms

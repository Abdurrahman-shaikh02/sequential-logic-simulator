#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <iostream>
#include <cstdint>
#include "core/Signal.hpp"

namespace slms {

// Records value changes of signals over discrete simulation time
class WaveformGenerator {
private:
    std::string moduleName;
    std::vector<std::string> signalNames;
    // Map from time -> (signalName -> Signal)
    std::map<int64_t, std::unordered_map<std::string, Signal>> timeline;

    // Helper to generate unique VCD identifiers (!, ", #, $, %, etc.)
    static std::string getVcdId(size_t index);

public:
    explicit WaveformGenerator(const std::string &module = "top");

    // Signal registration
    void registerSignal(const std::string &name);
    void registerSignals(const std::vector<std::string> &names);

    // Record an individual signal event or a full snapshot at timestamp
    void recordSignal(int64_t timestamp, const std::string &name, Signal val);
    void recordSnapshot(int64_t timestamp, const std::unordered_map<std::string, Signal> &snapshot);

    // IEEE 1364 Value Change Dump (VCD) export
    bool writeVCD(const std::string &filePath) const;
    void printVCD(std::ostream &os) const;

    // ASCII Graphical Digital Waveform diagram
    // Example:
    // clk   : ___/‾‾‾\___/‾‾‾\___
    // en    : _________/‾‾‾‾‾‾‾‾‾
    bool writeTextDiagram(const std::string &filePath, int stepWidth = 4) const;
    void printTextDiagram(std::ostream &os, int stepWidth = 4) const;

    // Tabular output (Time  Signal1  Signal2  ...) as in docs/out_format.txt
    bool writeTable(const std::string &filePath) const;
    void printTable(std::ostream &os) const;

    // JSON export for Web GUI
    std::string toJSON() const;
    bool writeJSON(const std::string &filePath) const;

    // Accessors
    const std::vector<std::string>& getSignals() const { return signalNames; }
    const std::map<int64_t, std::unordered_map<std::string, Signal>>& getTimeline() const { return timeline; }
    void clear();
};

} // namespace slms

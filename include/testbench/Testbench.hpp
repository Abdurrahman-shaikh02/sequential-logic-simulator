#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <unordered_map>
#include "core/Signal.hpp"
#include "waveform/WaveformGenerator.hpp"
#include "assertion/AssertionEngine.hpp"

namespace slms {

// Individual stimulus event
struct StimulusEvent {
    int64_t timestamp;
    std::string signalName;
    int bitIndex; // -1 for scalar
    Signal value;
};

// Clock configuration
struct ClockDef {
    std::string signalName;
    int64_t period;
    int64_t dutyHigh; // e.g. period / 2
    bool enabled;
};

// Scheduled assertion check
struct AssertionCheck {
    int64_t timestamp;
    std::string signalName;
    int bitIndex; // -1 for scalar
    Signal expected;
    std::string description;
};

class Testbench {
private:
    std::string circuitPath;
    bool isSynthesizedFSM;
    std::string tablePath;

    ClockDef clockDef{"clk", 10, 5, false};
    std::vector<StimulusEvent> stimuli;
    std::vector<AssertionCheck> assertions;

    WaveformGenerator waveform;
    AssertionEngine assertionEngine;

    int64_t maxTime;
    bool isInitialized;
    bool verbose;

    void applySignalValue(const std::string &name, int bitIndex, Signal val);
    Signal readSignalValue(const std::string &name, int bitIndex) const;

public:
    Testbench();
    ~Testbench();

    // Circuit loading
    bool loadCircuit(const std::string &filePath);

    // Clock setup
    void setClock(const std::string &name, int64_t period, int64_t highDuration = -1);

    // Stimulus scheduling
    void addStimulus(int64_t time, const std::string &signalName, Signal value, int bitIndex = -1);
    void addStimulus(int64_t time, const std::string &signalName, int value, int bitIndex = -1);

    // Assertion scheduling
    void addAssertion(int64_t time, const std::string &signalName, Signal expected, const std::string &desc = "", int bitIndex = -1);
    void addAssertion(int64_t time, const std::string &signalName, int expected, const std::string &desc = "", int bitIndex = -1);

    // Invariant registration
    void addInvariant(const std::string &name,
                      std::function<bool(const std::unordered_map<std::string, Signal>&, int64_t)> checkFn,
                      const std::string &desc);

    // Parse generic testbench file (.tb)
    bool loadTestbenchFile(const std::string &tbPath);

    // Run simulation
    bool run(int64_t duration = -1);

    // File exports
    bool exportVCD(const std::string &vcdPath) const;
    bool exportWaveform(const std::string &txtPath, int stepWidth = 4) const;
    bool exportTable(const std::string &tablePath) const;
    bool exportJSON(const std::string &jsonPath) const;

    // Reports
    void printReport(std::ostream &os = std::cout) const;

    // Accessors
    WaveformGenerator& getWaveform() { return waveform; }
    const WaveformGenerator& getWaveform() const { return waveform; }
    AssertionEngine& getAssertionEngine() { return assertionEngine; }
    const AssertionEngine& getAssertionEngine() const { return assertionEngine; }
    void setMaxTime(int64_t t) { maxTime = t; }
    int64_t getMaxTime() const { return maxTime; }
    void setVerbose(bool v) { verbose = v; }

    void reset();
};

} // namespace slms

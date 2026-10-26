#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <iostream>
#include <cstdint>
#include "core/Signal.hpp"

namespace slms {

enum class AssertionType {
    VALUE_EQUAL,       // Signal == expected at time T
    INVARIANT,         // Safety condition that must hold for all timestamps
    CUSTOM             // Custom condition / temporal logic
};

struct AssertionRecord {
    int id;
    int64_t timestamp;
    AssertionType type;
    std::string name;
    std::string signalName;
    Signal expectedSignal;
    Signal actualSignal;
    bool passed;
    std::string message;
    std::string failureReason;
};

class AssertionEngine {
private:
    std::vector<AssertionRecord> records;
    int nextId = 1;
    int passCount = 0;
    int failCount = 0;

    struct InvariantDef {
        std::string name;
        std::function<bool(const std::unordered_map<std::string, Signal>&, int64_t)> checkFn;
        std::string description;
    };
    std::vector<InvariantDef> invariants;

public:
    AssertionEngine();

    // Direct signal equality assertion at timestamp
    bool assertSignal(int64_t time, const std::string &signalName, Signal actual, Signal expected, const std::string &desc = "");

    // Register a safety invariant that is evaluated on every time step
    void addInvariant(const std::string &name,
                      std::function<bool(const std::unordered_map<std::string, Signal>&, int64_t)> checkFn,
                      const std::string &desc);

    // Evaluate all registered invariants against current circuit state
    void evaluateInvariants(int64_t time, const std::unordered_map<std::string, Signal>& state);

    // Custom boolean condition assertion
    bool assertCustom(int64_t time, bool condition, const std::string &name, const std::string &desc, const std::string &failReason = "");

    // Status queries
    bool allPassed() const { return failCount == 0; }
    int getPassCount() const { return passCount; }
    int getFailCount() const { return failCount; }
    int getTotalCount() const { return passCount + failCount; }
    const std::vector<AssertionRecord>& getRecords() const { return records; }

    // Reports
    void printReport(std::ostream &os = std::cout) const;
    std::string toJSON() const;

    void clear();
};

} // namespace slms

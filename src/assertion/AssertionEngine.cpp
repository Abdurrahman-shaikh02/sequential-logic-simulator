#include "assertion/AssertionEngine.hpp"
#include <iomanip>
#include <sstream>

namespace slms {

AssertionEngine::AssertionEngine() : passCount(0), failCount(0) {}

bool AssertionEngine::assertSignal(int64_t time, const std::string &signalName, Signal actual, Signal expected, const std::string &desc) {
    bool ok = (actual == expected);
    AssertionRecord rec;
    rec.id = nextId++;
    rec.timestamp = time;
    rec.type = AssertionType::VALUE_EQUAL;
    rec.name = "SignalValue:" + signalName;
    rec.signalName = signalName;
    rec.expectedSignal = expected;
    rec.actualSignal = actual;
    rec.passed = ok;
    rec.message = desc;

    if (!ok) {
        std::ostringstream ss;
        ss << "Expected " << (expected == Signal::HIGH ? "HIGH(1)" : "LOW(0)")
           << " but got " << (actual == Signal::HIGH ? "HIGH(1)" : "LOW(0)");
        rec.failureReason = ss.str();
        failCount++;
    } else {
        passCount++;
    }

    records.push_back(rec);
    return ok;
}

void AssertionEngine::addInvariant(const std::string &name,
                                  std::function<bool(const std::unordered_map<std::string, Signal>&, int64_t)> checkFn,
                                  const std::string &desc) {
    invariants.push_back({name, checkFn, desc});
}

void AssertionEngine::evaluateInvariants(int64_t time, const std::unordered_map<std::string, Signal>& state) {
    for (const auto &inv : invariants) {
        bool ok = inv.checkFn(state, time);
        AssertionRecord rec;
        rec.id = nextId++;
        rec.timestamp = time;
        rec.type = AssertionType::INVARIANT;
        rec.name = "Invariant:" + inv.name;
        rec.signalName = "";
        rec.expectedSignal = Signal::HIGH;
        rec.actualSignal = ok ? Signal::HIGH : Signal::LOW;
        rec.passed = ok;
        rec.message = inv.description;

        if (!ok) {
            rec.failureReason = "Safety invariant violated: " + inv.description;
            failCount++;
        } else {
            passCount++;
        }
        records.push_back(rec);
    }
}

bool AssertionEngine::assertCustom(int64_t time, bool condition, const std::string &name, const std::string &desc, const std::string &failReason) {
    AssertionRecord rec;
    rec.id = nextId++;
    rec.timestamp = time;
    rec.type = AssertionType::CUSTOM;
    rec.name = name;
    rec.signalName = "";
    rec.expectedSignal = Signal::HIGH;
    rec.actualSignal = condition ? Signal::HIGH : Signal::LOW;
    rec.passed = condition;
    rec.message = desc;

    if (!condition) {
        rec.failureReason = failReason.empty() ? "Condition failed" : failReason;
        failCount++;
    } else {
        passCount++;
    }

    records.push_back(rec);
    return condition;
}

void AssertionEngine::printReport(std::ostream &os) const {
    os << "\n======================================================================\n";
    os << "                      SLMS ASSERTION ENGINE REPORT                    \n";
    os << "======================================================================\n";
    os << " Total Evaluated : " << (passCount + failCount) << "\n";
    os << " Passed          : " << passCount << " (" 
       << (passCount + failCount > 0 ? (passCount * 100 / (passCount + failCount)) : 100) << "%)\n";
    os << " Failed          : " << failCount << "\n";
    os << " Overall Status  : " << (failCount == 0 ? "[ PASSED ]" : "[ FAILED ]") << "\n";
    os << "----------------------------------------------------------------------\n";

    if (failCount > 0) {
        os << "VIOLATIONS DETECTED:\n";
        for (const auto &r : records) {
            if (!r.passed) {
                os << " [!] Time " << std::setw(5) << r.timestamp << " ns | " 
                   << std::setw(20) << std::left << r.name << " : "
                   << r.failureReason;
                if (!r.message.empty()) {
                    os << " (" << r.message << ")";
                }
                os << "\n";
            }
        }
    } else {
        os << " All assertions and invariants held successfully across simulation!\n";
    }
    os << "======================================================================\n\n";
}

std::string AssertionEngine::toJSON() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"total\": " << (passCount + failCount) << ",\n";
    ss << "  \"passed\": " << passCount << ",\n";
    ss << "  \"failed\": " << failCount << ",\n";
    ss << "  \"status\": \"" << (failCount == 0 ? "PASSED" : "FAILED") << "\",\n";
    ss << "  \"records\": [\n";
    for (size_t i = 0; i < records.size(); ++i) {
        const auto &r = records[i];
        ss << "    {\n";
        ss << "      \"id\": " << r.id << ",\n";
        ss << "      \"time\": " << r.timestamp << ",\n";
        ss << "      \"name\": \"" << r.name << "\",\n";
        ss << "      \"signal\": \"" << r.signalName << "\",\n";
        ss << "      \"passed\": " << (r.passed ? "true" : "false") << ",\n";
        ss << "      \"reason\": \"" << r.failureReason << "\",\n";
        ss << "      \"message\": \"" << r.message << "\"\n";
        ss << "    }" << (i + 1 < records.size() ? ",\n" : "\n");
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

void AssertionEngine::clear() {
    records.clear();
    invariants.clear();
    nextId = 1;
    passCount = 0;
    failCount = 0;
}

} // namespace slms

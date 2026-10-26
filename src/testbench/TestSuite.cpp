#include "testbench/TestSuite.hpp"
#include "testbench/Testbench.hpp"
#include "waveform/WaveformGenerator.hpp"
#include "assertion/AssertionEngine.hpp"
#include "simulation/Simulator.hpp"
#include "parser/Parser.hpp"
#include "synthesizer/Synthesizer.hpp"
#include "synthesizer/DeadStates.hpp"
#include "synthesizer/UnreachableStates.hpp"
#include "combinational/AND.hpp"
#include "combinational/OR.hpp"
#include "combinational/NAND.hpp"
#include "combinational/NOR.hpp"
#include "combinational/NOT.hpp"
#include "combinational/XOR.hpp"
#include "combinational/MUX.hpp"
#include "sequential/DFF_P.hpp"
#include "sequential/TFF_P.hpp"
#include "sequential/JKFF_P.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "operations/SetPortOperation.hpp"
#include "operations/ChangeStateOperation.hpp"

#include <iomanip>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace slms {

TestSuite::TestSuite() : passCount(0), failCount(0) {}

void TestSuite::record(const std::string &cat, const std::string &name, bool ok, const std::string &details) {
    results.push_back({cat, name, ok, details});
    if (ok) {
        passCount++;
    } else {
        failCount++;
    }
}

// ----------------------------------------------------------------------------
// 1. UNIT TESTING
// ----------------------------------------------------------------------------

bool TestSuite::runAll() {
    results.clear();
    passCount = 0;
    failCount = 0;

    runUnitTests();
    runIntegrationTests();
    runValidationTests();
    runBlackBoxTests();
    runWhiteBoxTests();
    runRegressionTests();
    runSystemTests();

    return allPassed();
}

void TestSuite::printReport(std::ostream &os) const {
    os << "\n========================================================================================\n";
    os << "                 SEQUENTIAL LOGIC SIMULATOR (SLMS) - TEST SUITE REPORT                  \n";
    os << "========================================================================================\n";
    os << std::left << std::setw(15) << "CATEGORY"
       << std::setw(45) << "TEST NAME"
       << std::setw(10) << "STATUS"
       << "DETAILS\n";
    os << "----------------------------------------------------------------------------------------\n";

    for (const auto &r : results) {
        os << std::left << std::setw(15) << ("[" + r.category + "]")
           << std::setw(45) << r.testName
           << std::setw(10) << (r.passed ? "[ PASS ]" : "[ FAIL ]")
           << r.details << "\n";
    }

    os << "----------------------------------------------------------------------------------------\n";
    os << " TOTAL TESTS : " << (passCount + failCount) << " | "
       << "PASSED : " << passCount << " | "
       << "FAILED : " << failCount << " | "
       << "SUCCESS RATE : " << (passCount + failCount > 0 ? (passCount * 100 / (passCount + failCount)) : 0) << "%\n";
    os << " OVERALL SUITE STATUS : " << (failCount == 0 ? ">>> ALL TESTS PASSED <<<" : ">>> SOME TESTS FAILED <<<") << "\n";
    os << "========================================================================================\n\n";
}

} // namespace slms

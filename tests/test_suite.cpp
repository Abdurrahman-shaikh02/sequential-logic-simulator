#include "testbench/TestSuite.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    std::string category = (argc >= 2) ? argv[1] : "all";
    slms::TestSuite suite;

    std::cout << "[+] Running SLMS Software Testing Suite (Category: " << category << ") ...\n";
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
    return suite.allPassed() ? 0 : 1;
}

#pragma once

#include <string>
#include <vector>
#include <iostream>

namespace slms {

struct TestResult {
    std::string category;
    std::string testName;
    bool passed;
    std::string details;
};

class TestSuite {
private:
    std::vector<TestResult> results;
    int passCount = 0;
    int failCount = 0;

    void record(const std::string &cat, const std::string &name, bool ok, const std::string &details = "");

public:
    TestSuite();

    // 1. Unit Testing
    void runUnitTests();

    // 2. Integration Testing
    void runIntegrationTests();

    // 3. Validation Testing
    void runValidationTests();

    // 4. Black-Box Testing
    void runBlackBoxTests();

    // 5. White-Box Testing
    void runWhiteBoxTests();

    // 6. Regression Testing
    void runRegressionTests();

    // 7. System (End-to-End) Testing
    void runSystemTests();

    // Run everything
    bool runAll();

    // Reports
    void printReport(std::ostream &os = std::cout) const;
    bool allPassed() const { return failCount == 0; }
    int getPassCount() const { return passCount; }
    int getFailCount() const { return failCount; }
    const std::vector<TestResult>& getResults() const { return results; }
};

} // namespace slms

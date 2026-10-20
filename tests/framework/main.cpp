// Test runner for hdltest (see hdltest.hpp).
//
// Each test runs in a forked child process:
//   - the simulator's static state (queues, node list) starts fresh every time
//   - a crash (segfault, abort, failed assert) fails that test, not the run
//   - a hang is killed after --timeout seconds
//   - everything the test prints is captured and only shown when it fails
//     (or always with -v)

#include <fnmatch.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "framework/hdltest.hpp"

namespace {

enum class Outcome { PASS, FAIL, XFAIL, XPASS, CRASH, TIMEOUT };

struct Result {
    const hdltest::TestCase *test;
    Outcome outcome;
    std::string details;   // failure messages / crash reason
    std::string output;    // captured stdout + stderr of the test
    double seconds;
};

struct Options {
    std::vector<std::string> include, exclude;
    bool list = false, fork = true, verbose = false, failFast = false, color = false;
    int timeout = 30;
    std::string junit;
};

const char *RED = "", *GREEN = "", *YELLOW = "", *BOLD = "", *DIM = "", *RESET = "";

void useColor(bool on) {
    if (!on) return;
    RED = "\033[31m"; GREEN = "\033[32m"; YELLOW = "\033[33m";
    BOLD = "\033[1m"; DIM = "\033[2m"; RESET = "\033[0m";
}

void usage(const char *argv0) {
    std::cout <<
        "usage: " << argv0 << " [options] [pattern ...]\n"
        "\n"
        "Patterns are shell globs matched against 'suite.name'; a pattern\n"
        "without '*', '?' or '[' matches any test whose name contains it.\n"
        "Prefix a pattern with '-' to exclude it.\n"
        "Example: run_tests parser 'gates.*' -parser.err_\n"
        "\n"
        "  -l, --list         list matching tests and exit\n"
        "  -v, --verbose      show every test and its captured output\n"
        "  -x, --fail-fast    stop after the first failing test\n"
        "      --no-fork      run tests in this process (for gdb; a crash ends the run)\n"
        "      --timeout=S    per-test timeout in seconds (default 30, 0 = none)\n"
        "      --junit=FILE   also write a JUnit XML report (for CI)\n"
        "      --color, --no-color\n"
        "  -h, --help\n";
}

bool matches(const std::string &pattern, const std::string &name) {
    if (pattern.find_first_of("*?[") == std::string::npos)
        return name.find(pattern) != std::string::npos;
    return fnmatch(pattern.c_str(), name.c_str(), 0) == 0;
}

bool selected(const Options &o, const hdltest::TestCase &t) {
    std::string n = t.fullName();
    auto hit = [&](const std::string &p) { return matches(p, n); };
    if (!o.include.empty() && std::none_of(o.include.begin(), o.include.end(), hit))
        return false;
    return std::none_of(o.exclude.begin(), o.exclude.end(), hit);
}

// Runs the test body in the current process. Returns failure text ("" = pass).
std::string runBody(const hdltest::TestCase &t) {
    auto &st = hdltest::state();
    st.failures.clear();
    st.info.clear();
    try {
        t.fn();
    } catch (const hdltest::AbortTest &) {
        // a REQUIRE/FAIL fired; the failure is already recorded
    } catch (const std::exception &e) {
        hdltest::reportFailure(t.file, t.line, std::string("uncaught exception: ") + e.what());
    } catch (...) {
        hdltest::reportFailure(t.file, t.line, "uncaught non-std exception");
    }

    std::string out;
    for (auto &f : st.failures)
        out += "    " + f.file + ":" + std::to_string(f.line) + ": " + f.message + "\n";
    return out;
}

std::string readAll(FILE *f) {
    std::string s;
    rewind(f);
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        s.append(buf, n);
    return s;
}

Outcome finalOutcome(const hdltest::TestCase &t, bool passed) {
    if (t.xfail.empty())
        return passed ? Outcome::PASS : Outcome::FAIL;
    return passed ? Outcome::XPASS : Outcome::XFAIL;
}

Result runForked(const hdltest::TestCase &t, const Options &o) {
    Result r{&t, Outcome::FAIL, "", "", 0};
    FILE *outFile = tmpfile(), *resFile = tmpfile();
    if (!outFile || !resFile) {
        r.details = "    runner: cannot create temp files\n";
        return r;
    }

    std::cout.flush();
    std::cerr.flush();
    fflush(nullptr);

    auto start = std::chrono::steady_clock::now();
    pid_t pid = fork();
    if (pid < 0) {
        r.details = std::string("    runner: fork failed: ") + strerror(errno) + "\n";
        return r;
    }

    if (pid == 0) {
        // child: capture all output, run, write failures, exit.
        // No core dumps: writing one (or piping it to apport/systemd-coredump)
        // makes every crashing test take a second or more.
        struct rlimit noCore = {0, 0};
        setrlimit(RLIMIT_CORE, &noCore);
        prctl(PR_SET_DUMPABLE, 0);
        dup2(fileno(outFile), STDOUT_FILENO);
        dup2(fileno(outFile), STDERR_FILENO);
        if (o.timeout > 0) alarm(o.timeout);
        std::string failures = runBody(t);
        std::cout.flush();
        std::cerr.flush();
        fflush(nullptr);
        fwrite(failures.data(), 1, failures.size(), resFile);
        fflush(resFile);
        _exit(failures.empty() ? 0 : 1);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    r.output = readAll(outFile);
    r.details = readAll(resFile);
    fclose(outFile);
    fclose(resFile);

    // exit 1 with no recorded failure means something else (a sanitizer) ended the test
    bool reported = WIFEXITED(status) && (WEXITSTATUS(status) == 0 || (WEXITSTATUS(status) == 1 && !r.details.empty()));
    if (reported) {
        r.outcome = finalOutcome(t, WEXITSTATUS(status) == 0);
    } else if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM) {
        r.outcome = t.xfail.empty() ? Outcome::TIMEOUT : Outcome::XFAIL;
        r.details += "    timed out after " + std::to_string(o.timeout) + "s (infinite loop in the simulator?)\n";
    } else {
        r.outcome = t.xfail.empty() ? Outcome::CRASH : Outcome::XFAIL;
        if (WIFSIGNALED(status))
            r.details += std::string("    crashed: ") + strsignal(WTERMSIG(status)) +
                         " (signal " + std::to_string(WTERMSIG(status)) + ")\n";
        else
            r.details += "    exited with unexpected status " + std::to_string(WEXITSTATUS(status)) +
                         " (sanitizer error? see captured output)\n";
    }
    return r;
}

Result runInProcess(const hdltest::TestCase &t) {
    auto start = std::chrono::steady_clock::now();
    std::string failures = runBody(t);
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return {&t, finalOutcome(t, failures.empty()), failures, "", secs};
}

const char *label(Outcome o) {
    switch (o) {
        case Outcome::PASS: return "PASS";
        case Outcome::FAIL: return "FAIL";
        case Outcome::XFAIL: return "XFAIL";
        case Outcome::XPASS: return "XPASS";
        case Outcome::CRASH: return "CRASH";
        case Outcome::TIMEOUT: return "TIMEOUT";
    }
    return "?";
}

const char *colorOf(Outcome o) {
    switch (o) {
        case Outcome::PASS: return GREEN;
        case Outcome::XFAIL: return YELLOW;
        default: return RED;
    }
}

bool isBad(Outcome o) {
    return o == Outcome::FAIL || o == Outcome::XPASS || o == Outcome::CRASH || o == Outcome::TIMEOUT;
}

std::string xmlEscape(const std::string &s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '&': r += "&amp;"; break;
            case '<': r += "&lt;"; break;
            case '>': r += "&gt;"; break;
            case '"': r += "&quot;"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20 && c != '\n' && c != '\t') r += '?';
                else r += c;
        }
    }
    return r;
}

void writeJunit(const std::string &path, const std::vector<Result> &results) {
    std::ofstream f(path);
    int failures = 0, skipped = 0;
    double total = 0;
    for (auto &r : results) {
        failures += isBad(r.outcome);
        skipped += r.outcome == Outcome::XFAIL;
        total += r.seconds;
    }
    f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      << "<testsuite name=\"hdl_sim\" tests=\"" << results.size() << "\" failures=\"" << failures
      << "\" skipped=\"" << skipped << "\" time=\"" << total << "\">\n";
    for (auto &r : results) {
        f << "  <testcase classname=\"" << xmlEscape(r.test->suite) << "\" name=\""
          << xmlEscape(r.test->name) << "\" time=\"" << r.seconds << "\">\n";
        if (isBad(r.outcome))
            f << "    <failure message=\"" << label(r.outcome) << "\">" << xmlEscape(r.details) << "</failure>\n";
        else if (r.outcome == Outcome::XFAIL)
            f << "    <skipped message=\"expected failure: " << xmlEscape(r.test->xfail) << "\"/>\n";
        if (!r.output.empty())
            f << "    <system-out>" << xmlEscape(r.output) << "</system-out>\n";
        f << "  </testcase>\n";
    }
    f << "</testsuite>\n";
}

void printResult(const Result &r, const Options &o) {
    const auto *t = r.test;
    bool bad = isBad(r.outcome);
    std::cout << colorOf(r.outcome) << BOLD << "[" << label(r.outcome) << "] " << RESET
              << t->fullName() << DIM << "  (" << t->file << ":" << t->line << ", "
              << static_cast<int>(r.seconds * 1000) << " ms)" << RESET << "\n";
    if (r.outcome == Outcome::XFAIL)
        std::cout << "    expected failure: " << t->xfail << "\n";
    if (r.outcome == Outcome::XPASS)
        std::cout << "    marked TEST_XFAIL (\"" << t->xfail
                  << "\") but it passed: the issue looks fixed, change it to TEST\n";
    if (bad || (o.verbose && r.outcome == Outcome::XFAIL))
        std::cout << r.details;
    if (!r.output.empty() && (bad || o.verbose)) {
        std::string out = r.output;
        if (out.size() > 4000)
            out = "[... " + std::to_string(out.size() - 4000) + " bytes omitted ...]\n" +
                  out.substr(out.size() - 4000);
        std::cout << DIM << "    --- captured output ---\n";
        std::istringstream lines(out);
        for (std::string l; std::getline(lines, l);)
            std::cout << "    | " << l << "\n";
        std::cout << "    -----------------------" << RESET << "\n";
    }
}

}  // namespace

int main(int argc, char **argv) {
    Options o;
    o.color = isatty(STDOUT_FILENO);

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { usage(argv[0]); return 0; }
        else if (a == "-l" || a == "--list") o.list = true;
        else if (a == "-v" || a == "--verbose") o.verbose = true;
        else if (a == "-x" || a == "--fail-fast") o.failFast = true;
        else if (a == "--no-fork") o.fork = false;
        else if (a == "--color") o.color = true;
        else if (a == "--no-color") o.color = false;
        else if (a.rfind("--timeout=", 0) == 0) o.timeout = std::atoi(a.c_str() + 10);
        else if (a.rfind("--junit=", 0) == 0) o.junit = a.substr(8);
        else if (a.rfind("--", 0) == 0) { std::cerr << "unknown option " << a << "\n"; usage(argv[0]); return 2; }
        else if (a.size() > 1 && a[0] == '-') o.exclude.push_back(a.substr(1));
        else o.include.push_back(a);
    }
    useColor(o.color);

    std::vector<const hdltest::TestCase *> tests;
    for (auto &t : hdltest::registry())
        if (selected(o, t)) tests.push_back(&t);
    std::stable_sort(tests.begin(), tests.end(), [](auto *a, auto *b) { return a->suite < b->suite; });

    if (o.list) {
        for (auto *t : tests)
            std::cout << t->fullName() << (t->xfail.empty() ? "" : "  [xfail]") << "\n";
        return 0;
    }
    if (tests.empty()) {
        std::cerr << "no tests match the given patterns\n";
        return 2;
    }

    std::vector<Result> results;
    auto start = std::chrono::steady_clock::now();
    std::string progressSuite;   // suite whose dots are on the current line

    for (auto *t : tests) {
        Result r = o.fork ? runForked(*t, o) : runInProcess(*t);
        results.push_back(r);
        bool bad = isBad(r.outcome);

        if (o.verbose || bad || r.outcome == Outcome::XFAIL) {
            if (!progressSuite.empty()) std::cout << "\n";
            progressSuite.clear();
            printResult(r, o);
        } else {
            // quiet: a line of dots per suite keeps progress visible without noise
            if (t->suite != progressSuite) {
                if (!progressSuite.empty()) std::cout << "\n";
                std::cout << DIM << t->suite << " " << RESET;
                progressSuite = t->suite;
            }
            std::cout << GREEN << "." << RESET << std::flush;
        }
        if (bad && o.failFast) break;
    }
    if (!progressSuite.empty()) std::cout << "\n";

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    int counts[6] = {};
    for (auto &r : results) counts[static_cast<int>(r.outcome)]++;
    int bad = 0;
    for (auto &r : results) bad += isBad(r.outcome);

    char secs[32];
    snprintf(secs, sizeof secs, "%.2fs", elapsed);
    std::cout << "\n" << BOLD << "==== " << results.size() << " tests in " << secs << ": " << RESET
              << GREEN << counts[(int)Outcome::PASS] << " passed" << RESET;
    if (counts[(int)Outcome::XFAIL])
        std::cout << ", " << YELLOW << counts[(int)Outcome::XFAIL] << " expected failures (known issues)" << RESET;
    for (Outcome x : {Outcome::FAIL, Outcome::XPASS, Outcome::CRASH, Outcome::TIMEOUT})
        if (counts[(int)x]) std::cout << ", " << RED << counts[(int)x] << " " << label(x) << RESET;
    std::cout << "\n";

    if (bad) {
        std::cout << RED << "\nFailing tests:\n" << RESET;
        for (auto &r : results)
            if (isBad(r.outcome))
                std::cout << "  " << label(r.outcome) << "  " << r.test->fullName() << "\n";
        std::cout << "\nRe-run one with: make test TESTS=<name> ARGS=-v\n";
    }

    if (!o.junit.empty()) writeJunit(o.junit, results);
    return bad ? 1 : 0;
}

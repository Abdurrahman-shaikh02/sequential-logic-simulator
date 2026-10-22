// Command line front end for the simulator.
//
//   simulator sim   <netlist> -s <stimulus> [-o <trace>] [--no-dump] [--steps N]
//   simulator synth <fsm>     [-o <netlist>]
//   simulator truth <netlist> [-o <table>]
//   simulator ports <netlist>
//
// Common: -p/--perf, --repeat N, --warmup N, --stats text|json|csv, -q, -v, -h
//
// Stimulus file: a "// inputs: a b[3:0] ..." header naming every primary input
// (any order, buses MSB first), then one line per time step with one binary
// field per header entry. Each step drives all inputs, lets the circuit settle
// and samples the outputs. The trace has the same shape with "// outputs:".
//
// Exit codes: 0 ok, 1 usage error, 2 parse/synthesis error (design or
// stimulus), 3 runtime error (simulation or writing output).

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cxxabi.h>
#include <fstream>
#include <functional>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <typeinfo>
#include <unordered_set>
#include <vector>

#include "simulation/Simulator.hpp"
#include "synthesizer/Synthesizer.hpp"
#include "sequential/SequentialNode.hpp"

namespace {

enum ExitCode { OK = 0, USAGE = 1, PARSE_ERROR = 2, RUNTIME_ERROR = 3 };

enum class StatsFormat { TEXT, JSON, CSV };

struct Options {
    std::string command;
    std::string design;
    std::string stimulus;
    std::string out;            // "" = stdout
    bool noDump = false;
    std::optional<uint64_t> steps;
    bool perf = false;
    int repeat = 1;
    int warmup = 0;
    StatsFormat statsFormat = StatsFormat::TEXT;
    bool quiet = false;
    bool verbose = false;
};

// ---------------------------------------------------------------------------
// Usage
// ---------------------------------------------------------------------------

const char *MAIN_USAGE =
    "usage: simulator <command> <file> [options]\n"
    "\n"
    "commands:\n"
    "  sim   <netlist>   simulate a netlist with a stimulus file\n"
    "  synth <fsm>       synthesize an FSM description into a netlist\n"
    "  truth <netlist>   write the truth table of a combinational netlist\n"
    "  ports <netlist>   list primary inputs/outputs as a stimulus header\n"
    "\n"
    "common options:\n"
    "  -p, --perf              collect and print statistics (to stderr)\n"
    "      --repeat N          run N times, report the median (needs -p)\n"
    "      --warmup N          do N untimed runs first (needs -p)\n"
    "      --stats FORMAT      text (default), json or csv\n"
    "  -q, --quiet             hide warnings from the parser/synthesizer\n"
    "  -v, --verbose           print progress to stderr\n"
    "  -h, --help              show help (simulator <command> -h for details)\n";

std::string commandUsage(const std::string &cmd) {
    if (cmd == "sim")
        return "usage: simulator sim <netlist> -s <stimulus> [options]\n"
               "\n"
               "  -s, --stimulus FILE     stimulus file (required)\n"
               "  -o, --out FILE          output trace (default stdout)\n"
               "      --no-dump           do not write the trace\n"
               "      --steps N           stop after N stimulus lines\n"
               "\n"
               "stimulus format:\n"
               "  // inputs: clk rst d[3:0]\n"
               "  0 1 0000\n"
               "  1 1 0000\n"
               "Every primary input appears once in the header; buses are written\n"
               "MSB first. Each line drives all inputs, settles, then samples the\n"
               "outputs. Never change data inputs and a clock edge on the same line.\n";
    if (cmd == "synth")
        return "usage: simulator synth <fsm> [options]\n"
               "\n"
               "  -o, --out FILE          netlist to write (default stdout)\n";
    if (cmd == "truth")
        return "usage: simulator truth <netlist> [options]\n"
               "\n"
               "  -o, --out FILE          truth table to write (default stdout)\n";
    if (cmd == "ports")
        return "usage: simulator ports <netlist> [options]\n"
               "\n"
               "Prints the stimulus/trace headers for the netlist.\n";
    return MAIN_USAGE;
}

int usageError(const std::string &msg) {
    std::cerr << "simulator: " << msg << "\n(see simulator -h)\n";
    return USAGE;
}

bool parseCount(const char *arg, uint64_t &value) {
    if (!arg || !*arg || *arg == '-')
        return false;
    char *end = nullptr;
    errno = 0;
    unsigned long long v = std::strtoull(arg, &end, 10);
    if (errno || *end)
        return false;
    value = v;
    return true;
}

// Returns OK, or an exit code to stop with (USAGE, or -1 for "help printed")
int parseArgs(int argc, char **argv, Options &opt) {
    if (argc < 2) {
        std::cerr << MAIN_USAGE;
        return USAGE;
    }
    std::string first = argv[1];
    if (first == "-h" || first == "--help") {
        std::cout << MAIN_USAGE;
        return -1;
    }
    if (first != "sim" && first != "synth" && first != "truth" && first != "ports")
        return usageError("unknown command '" + first + "'");
    opt.command = first;

    enum { OPT_NO_DUMP = 256, OPT_STEPS, OPT_REPEAT, OPT_WARMUP, OPT_STATS };
    static const option longOpts[] = {
        {"stimulus", required_argument, nullptr, 's'},
        {"out", required_argument, nullptr, 'o'},
        {"no-dump", no_argument, nullptr, OPT_NO_DUMP},
        {"steps", required_argument, nullptr, OPT_STEPS},
        {"perf", no_argument, nullptr, 'p'},
        {"repeat", required_argument, nullptr, OPT_REPEAT},
        {"warmup", required_argument, nullptr, OPT_WARMUP},
        {"stats", required_argument, nullptr, OPT_STATS},
        {"quiet", no_argument, nullptr, 'q'},
        {"verbose", no_argument, nullptr, 'v'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0},
    };

    // Parse everything after the command; argv[1] plays the program name
    int subArgc = argc - 1;
    char **subArgv = argv + 1;
    bool repeatGiven = false, warmupGiven = false;
    std::vector<std::string> onlyFor;   // flags that need a specific command

    opterr = 0;
    optind = 1;
    int c;
    while ((c = getopt_long(subArgc, subArgv, ":s:o:pqvh", longOpts, nullptr)) != -1) {
        uint64_t n;
        switch (c) {
            case 's': opt.stimulus = optarg; onlyFor.push_back("-s"); break;
            case 'o': opt.out = optarg; onlyFor.push_back("-o"); break;
            case OPT_NO_DUMP: opt.noDump = true; onlyFor.push_back("--no-dump"); break;
            case OPT_STEPS:
                if (!parseCount(optarg, n))
                    return usageError(std::string("--steps expects a number, got '") + optarg + "'");
                opt.steps = n;
                onlyFor.push_back("--steps");
                break;
            case 'p': opt.perf = true; break;
            case OPT_REPEAT:
                if (!parseCount(optarg, n) || n == 0 || n > 1000000)
                    return usageError(std::string("--repeat expects a number >= 1, got '") + optarg + "'");
                opt.repeat = static_cast<int>(n);
                repeatGiven = true;
                break;
            case OPT_WARMUP:
                if (!parseCount(optarg, n) || n > 1000000)
                    return usageError(std::string("--warmup expects a number, got '") + optarg + "'");
                opt.warmup = static_cast<int>(n);
                warmupGiven = true;
                break;
            case OPT_STATS: {
                std::string f = optarg;
                if (f == "text") opt.statsFormat = StatsFormat::TEXT;
                else if (f == "json") opt.statsFormat = StatsFormat::JSON;
                else if (f == "csv") opt.statsFormat = StatsFormat::CSV;
                else return usageError("--stats expects text, json or csv, got '" + f + "'");
                break;
            }
            case 'q': opt.quiet = true; break;
            case 'v': opt.verbose = true; break;
            case 'h':
                std::cout << commandUsage(opt.command);
                return -1;
            case ':':
                return usageError(std::string("option ") + subArgv[optind - 1] + " needs a value");
            default:
                return usageError(std::string("unknown option ") + subArgv[optind - 1]);
        }
    }

    std::vector<std::string> positional(subArgv + optind, subArgv + subArgc);
    if (positional.empty())
        return usageError(opt.command + " needs a " + (opt.command == "synth" ? "FSM" : "netlist") + " file");
    if (positional.size() > 1)
        return usageError("unexpected argument '" + positional[1] + "'");
    opt.design = positional[0];

    // Reject flags that mean nothing for this command
    for (auto &flag : onlyFor) {
        bool ok = (flag == "-o") ? opt.command != "ports" : opt.command == "sim";
        if (!ok)
            return usageError(flag + " is not an option of '" + opt.command + "'");
    }
    if (opt.command == "sim" && opt.stimulus.empty())
        return usageError("sim needs a stimulus file (-s FILE)");
    if (opt.noDump && !opt.out.empty())
        return usageError("--no-dump and -o cannot be used together");
    if ((repeatGiven || warmupGiven) && !opt.perf)
        return usageError("--repeat and --warmup need -p");
    if (opt.quiet && opt.verbose)
        return usageError("-q and -v cannot be used together");
    return OK;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

using Clock = std::chrono::steady_clock;

double msSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// Runs `fn` with std::cerr captured; replays it unless quiet (always on failure)
bool withLibraryLog(bool quiet, const std::function<bool()> &fn) {
    std::ostringstream buf;
    std::streambuf *old = std::cerr.rdbuf(buf.rdbuf());
    bool ok = fn();
    std::cerr.rdbuf(old);
    if (!ok || !quiet)
        std::cerr << buf.str();
    return ok;
}

std::string demangle(const char *name) {
    int status = 0;
    std::unique_ptr<char, void (*)(void *)> res(abi::__cxa_demangle(name, nullptr, nullptr, &status), std::free);
    return status == 0 ? res.get() : name;
}

long peakRssKb() {
    rusage ru{};
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_maxrss;   // kilobytes on Linux
}

// One named port as it appears in a header: "d[3:0]" or "clk"
struct PortRef {
    std::string name;
    std::vector<Node *> bits;   // bits[k] = bit k (LSB first, as the parser stores them)
};

std::string portLabel(const PortRef &p) {
    if (p.bits.size() == 1)
        return p.name;
    return p.name + "[" + std::to_string(p.bits.size() - 1) + ":0]";
}

// Ports sorted by name, so headers are stable from run to run
std::vector<PortRef> sortedPorts(const std::unordered_map<std::string, std::vector<Node *>> &m) {
    std::vector<PortRef> ports;
    for (auto &[name, bits] : m)
        ports.push_back({name, bits});
    std::sort(ports.begin(), ports.end(), [](auto &a, auto &b) { return a.name < b.name; });
    return ports;
}

std::string header(const std::string &kind, const std::vector<PortRef> &ports) {
    std::string h = "// " + kind + ":";
    for (auto &p : ports)
        h += " " + portLabel(p);
    return h;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

// Timings of one run, in milliseconds; a phase a command does not have stays unset
struct Timings {
    std::optional<double> parse, synth, init, simulate, ioRead, ioWrite;
};

struct DesignStats {
    bool known = false;
    size_t nodes = 0;
    size_t inputBits = 0;
    size_t outputBits = 0;
    size_t flipFlops = 0;
    std::map<std::string, size_t> byType;
};

struct Report {
    std::string command, design;
    int runs = 0;
    std::vector<Timings> timings;
    DesignStats designStats;
    std::optional<uint64_t> steps;
    Simulator::Stats work;
    bool simulated = false;
};

DesignStats collectDesignStats() {
    DesignStats d;
    d.known = true;
    d.nodes = Simulator::nodes.size();
    for (auto &n : Simulator::nodes) {
        Node &node = *n;
        d.byType[demangle(typeid(node).name())]++;
        if (dynamic_cast<SequentialNode *>(&node))
            d.flipFlops++;
    }
    for (auto &[name, bits] : Simulator::primaryInputs) d.inputBits += bits.size();
    for (auto &[name, bits] : Simulator::primaryOutputs) d.outputBits += bits.size();
    return d;
}

std::optional<double> median(const std::vector<Timings> &runs, std::optional<double> Timings::*field) {
    std::vector<double> v;
    for (auto &t : runs)
        if (t.*field) v.push_back(*(t.*field));
    if (v.empty()) return std::nullopt;
    std::sort(v.begin(), v.end());
    size_t m = v.size() / 2;
    return v.size() % 2 ? v[m] : (v[m - 1] + v[m]) / 2;
}

std::string jsonString(const std::string &s) {
    std::string r = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') r += '\\';
        if (static_cast<unsigned char>(c) < 0x20) { r += ' '; continue; }
        r += c;
    }
    return r + "\"";
}

std::string csvField(const std::string &s) {
    if (s.find_first_of(",\"\n") == std::string::npos) return s;
    std::string r = "\"";
    for (char c : s) { if (c == '"') r += '"'; r += c; }
    return r + "\"";
}

#ifdef __OPTIMIZE__
constexpr bool OPTIMIZED_BUILD = true;
#else
constexpr bool OPTIMIZED_BUILD = false;
#endif

void printReport(const Report &r, StatsFormat format, std::ostream &os) {
    struct Phase { const char *name; std::optional<double> Timings::*field; };
    const Phase phases[] = {
        {"parse", &Timings::parse}, {"synth", &Timings::synth}, {"init", &Timings::init},
        {"simulate", &Timings::simulate}, {"io_read", &Timings::ioRead}, {"io_write", &Timings::ioWrite},
    };

    uint64_t ops = r.work.combinationalOps + r.work.topologicalCommits + r.work.sequentialOps;
    std::optional<double> simMs = median(r.timings, &Timings::simulate);
    std::optional<double> opsPerSec;
    if (r.simulated && simMs && *simMs > 0)
        opsPerSec = ops / (*simMs / 1000.0);
    long rss = peakRssKb();

    std::ostringstream num;
    auto fmt = [&](double v) { num.str(""); num << std::fixed << std::setprecision(3) << v; return num.str(); };

    if (format == StatsFormat::TEXT) {
        os << "== " << r.command << " " << r.design << "  (" << r.runs << (r.runs == 1 ? " run" : " runs, median")
           << (OPTIMIZED_BUILD ? "" : ", UNOPTIMIZED BUILD: use make release") << ")\n";
        for (auto &ph : phases)
            if (auto v = median(r.timings, ph.field))
                os << "  " << std::left << std::setw(14) << (std::string(ph.name) + " ms") << fmt(*v) << '\n';
        if (r.designStats.known) {
            const auto &d = r.designStats;
            os << "  nodes         " << d.nodes << "  (";
            bool firstType = true;
            for (auto &[t, n] : d.byType) {
                os << (firstType ? "" : ", ") << t << " " << n;
                firstType = false;
            }
            os << ")\n";
            os << "  input bits    " << d.inputBits << '\n';
            os << "  output bits   " << d.outputBits << '\n';
            os << "  flip-flops    " << d.flipFlops << '\n';
        }
        if (r.steps) os << "  steps         " << *r.steps << '\n';
        if (r.simulated) {
            os << "  comb ops      " << r.work.combinationalOps << '\n';
            os << "  commits       " << r.work.topologicalCommits << '\n';
            os << "  seq ops       " << r.work.sequentialOps << '\n';
            if (r.steps && *r.steps) os << "  ops/step      " << fmt(double(ops) / *r.steps) << '\n';
            if (opsPerSec) os << "  ops/sec       " << fmt(*opsPerSec) << '\n';
        }
        os << "  peak rss KB   " << rss << '\n';
        return;
    }

    // json and csv share one flat list of fields
    std::vector<std::pair<std::string, std::string>> fields;   // value already formatted
    auto str = [](const std::string &s) { return s; };
    fields.push_back({"command", str(r.command)});
    fields.push_back({"design", str(r.design)});
    fields.push_back({"runs", std::to_string(r.runs)});
    fields.push_back({"optimized", OPTIMIZED_BUILD ? "true" : "false"});
    for (auto &ph : phases) {
        auto v = median(r.timings, ph.field);
        fields.push_back({std::string(ph.name) + "_ms", v ? fmt(*v) : ""});
    }
    auto count = [&](bool have, uint64_t v) { return have ? std::to_string(v) : std::string(); };
    const auto &d = r.designStats;
    fields.push_back({"nodes", count(d.known, d.nodes)});
    fields.push_back({"input_bits", count(d.known, d.inputBits)});
    fields.push_back({"output_bits", count(d.known, d.outputBits)});
    fields.push_back({"flip_flops", count(d.known, d.flipFlops)});
    fields.push_back({"steps", count(r.steps.has_value(), r.steps.value_or(0))});
    fields.push_back({"comb_ops", count(r.simulated, r.work.combinationalOps)});
    fields.push_back({"commits", count(r.simulated, r.work.topologicalCommits)});
    fields.push_back({"seq_ops", count(r.simulated, r.work.sequentialOps)});
    fields.push_back({"ops_per_sec", opsPerSec ? fmt(*opsPerSec) : ""});
    fields.push_back({"peak_rss_kb", std::to_string(rss)});

    const std::vector<std::string> stringFields = {"command", "design"};
    auto isString = [&](const std::string &k) {
        return std::find(stringFields.begin(), stringFields.end(), k) != stringFields.end();
    };

    if (format == StatsFormat::CSV) {
        for (size_t i = 0; i < fields.size(); i++) os << (i ? "," : "") << fields[i].first;
        os << '\n';
        for (size_t i = 0; i < fields.size(); i++) os << (i ? "," : "") << csvField(fields[i].second);
        os << '\n';
        return;
    }

    os << "{";
    for (size_t i = 0; i < fields.size(); i++) {
        auto &[k, v] = fields[i];
        os << (i ? ", " : "") << jsonString(k) << ": ";
        if (isString(k)) os << jsonString(v);
        else os << (v.empty() ? "null" : v);
    }
    if (d.known) {
        os << ", \"nodes_by_type\": {";
        bool firstType = true;
        for (auto &[t, n] : d.byType) {
            os << (firstType ? "" : ", ") << jsonString(t) << ": " << n;
            firstType = false;
        }
        os << "}";
    }
    os << "}\n";
}

// ---------------------------------------------------------------------------
// Stimulus
// ---------------------------------------------------------------------------

struct Stimulus {
    std::vector<Node *> bits;   // driven nodes, in file order (MSB first per port)
    std::vector<char> values;   // steps * bits.size() values, row major
    uint64_t steps = 0;
};

bool stimulusError(const std::string &path, int line, const std::string &msg) {
    std::cerr << path << ":" << line << ": " << msg << '\n';
    return false;
}

// Parses "name" or "name[H:0]"; width is 0 when no range was given
bool parseHeaderEntry(const std::string &tok, std::string &name, size_t &width) {
    auto open = tok.find('[');
    if (open == std::string::npos) {
        name = tok;
        width = 0;
        return !name.empty();
    }
    if (tok.back() != ']') return false;
    name = tok.substr(0, open);
    std::string range = tok.substr(open + 1, tok.size() - open - 2);
    auto colon = range.find(':');
    if (name.empty() || colon == std::string::npos || range.substr(colon + 1) != "0") return false;
    uint64_t high;
    if (!parseCount(range.substr(0, colon).c_str(), high)) return false;
    width = high + 1;
    return true;
}

bool readStimulus(const std::string &path, std::optional<uint64_t> maxSteps, Stimulus &stim) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "cannot open stimulus file " << path << '\n';
        return false;
    }

    std::vector<size_t> widths;   // per header entry
    bool haveHeader = false;
    std::string line;
    int lineNo = 0;

    while (std::getline(file, line)) {
        lineNo++;
        if (maxSteps && stim.steps >= *maxSteps) break;

        std::istringstream iss(line);
        std::string tok;
        if (!(iss >> tok)) continue;   // blank line

        if (tok.rfind("//", 0) == 0) {
            // A comment; the first one with "inputs:" is the header
            std::string rest = line.substr(line.find("//") + 2);
            std::istringstream h(rest);
            std::string key;
            if (!(h >> key) || key != "inputs:") continue;
            if (haveHeader) return stimulusError(path, lineNo, "second '// inputs:' header");

            std::unordered_set<std::string> seen;
            std::string entry;
            while (h >> entry) {
                std::string name;
                size_t width;
                if (!parseHeaderEntry(entry, name, width))
                    return stimulusError(path, lineNo, "bad header entry '" + entry + "' (expected name or name[N:0])");
                auto it = Simulator::primaryInputs.find(name);
                if (it == Simulator::primaryInputs.end())
                    return stimulusError(path, lineNo, "netlist has no primary input '" + name + "'");
                if (!seen.insert(name).second)
                    return stimulusError(path, lineNo, "input '" + name + "' listed twice");

                const auto &bits = it->second;
                size_t actual = bits.size();
                if (width == 0 && actual != 1)
                    return stimulusError(path, lineNo, "input '" + name + "' is " + std::to_string(actual) +
                                         " bits wide; write it as " + name + "[" + std::to_string(actual - 1) + ":0]");
                if (width != 0 && width != actual)
                    return stimulusError(path, lineNo, "input '" + name + "' is " + std::to_string(actual) +
                                         " bits wide, header says " + std::to_string(width));
                for (size_t k = actual; k-- > 0;)
                    stim.bits.push_back(bits[k]);
                widths.push_back(actual);
            }
            for (auto &[name, bits] : Simulator::primaryInputs)
                if (!seen.count(name))
                    return stimulusError(path, lineNo, "header is missing primary input '" + name + "'");
            haveHeader = true;
            continue;
        }

        if (!haveHeader)
            return stimulusError(path, lineNo, "expected a '// inputs: ...' header before the first step");

        // A data line: one binary field per header entry
        std::vector<std::string> fields = {tok};
        while (iss >> tok) {
            if (tok.rfind("//", 0) == 0) break;   // trailing comment
            fields.push_back(tok);
        }
        if (fields.size() != widths.size())
            return stimulusError(path, lineNo, "expected " + std::to_string(widths.size()) + " fields, got " +
                                 std::to_string(fields.size()));
        for (size_t i = 0; i < fields.size(); i++) {
            if (fields[i].size() != widths[i])
                return stimulusError(path, lineNo, "field " + std::to_string(i + 1) + " should be " +
                                     std::to_string(widths[i]) + " bits, got '" + fields[i] + "'");
            for (char ch : fields[i]) {
                if (ch != '0' && ch != '1')
                    return stimulusError(path, lineNo, "field " + std::to_string(i + 1) + " is not binary: '" +
                                         fields[i] + "'");
                stim.values.push_back(ch == '1');
            }
        }
        stim.steps++;
    }

    if (!haveHeader)
        return stimulusError(path, lineNo, "no '// inputs: ...' header found");
    return true;
}

// ---------------------------------------------------------------------------
// Commands. Each run does the whole job from scratch; output goes to `out`.
// ---------------------------------------------------------------------------

int parseDesign(const Options &opt, Timings &t) {
    Simulator::clear();
    auto start = Clock::now();
    bool ok = withLibraryLog(opt.quiet, [&] { return Simulator::parse(opt.design) == 0; });
    t.parse = msSince(start);
    if (!ok) return PARSE_ERROR;
    if (opt.verbose)
        std::cerr << "parsed " << opt.design << ": " << Simulator::nodes.size() << " nodes\n";
    return OK;
}

int runSim(const Options &opt, Timings &t, Report &rep, std::ostream &out) {
    if (int rc = parseDesign(opt, t)) return rc;

    Stimulus stim;
    auto start = Clock::now();
    if (!readStimulus(opt.stimulus, opt.steps, stim)) return PARSE_ERROR;
    t.ioRead = msSince(start);
    if (opt.verbose)
        std::cerr << "read " << stim.steps << " steps from " << opt.stimulus << '\n';

    std::vector<PortRef> outPorts = sortedPorts(Simulator::primaryOutputs);
    std::vector<Node *> outBits;   // MSB first per port
    for (auto &p : outPorts)
        for (size_t k = p.bits.size(); k-- > 0;)
            outBits.push_back(p.bits[k]);

    start = Clock::now();
    Simulator::initialize();
    t.init = msSince(start);

    // Sampled outputs are kept in memory so formatting is not timed as simulation
    const size_t nIn = stim.bits.size(), nOut = outBits.size();
    std::vector<char> samples;
    if (!opt.noDump) samples.resize(stim.steps * nOut);

    start = Clock::now();
    for (uint64_t s = 0; s < stim.steps; s++) {
        const char *row = stim.values.data() + s * nIn;
        for (size_t i = 0; i < nIn; i++)
            stim.bits[i]->evaluate(Port::O, row[i] ? Signal::HIGH : Signal::LOW);
        Simulator::run();
        if (!opt.noDump) {
            char *dst = samples.data() + s * nOut;
            for (size_t i = 0; i < nOut; i++)
                dst[i] = outBits[i]->getResult() == Signal::HIGH;
        }
    }
    t.simulate = msSince(start);

    rep.steps = stim.steps;
    rep.simulated = true;
    rep.work = Simulator::stats;
    rep.designStats = collectDesignStats();

    if (opt.noDump) return OK;

    start = Clock::now();
    out << header("outputs", outPorts) << '\n';
    std::string line;
    for (uint64_t s = 0; s < stim.steps; s++) {
        line.clear();
        const char *row = samples.data() + s * nOut;
        size_t i = 0;
        for (size_t p = 0; p < outPorts.size(); p++) {
            if (p) line += ' ';
            for (size_t k = 0; k < outPorts[p].bits.size(); k++)
                line += row[i++] ? '1' : '0';
        }
        line += '\n';
        out << line;
    }
    t.ioWrite = msSince(start);
    return OK;
}

int runSynth(const Options &opt, Timings &t, Report &, std::ostream &out) {
    auto start = Clock::now();
    bool ok = withLibraryLog(opt.quiet, [&] { return Synthesizer::synthesize(opt.design, out); });
    t.synth = msSince(start);
    return ok ? OK : PARSE_ERROR;
}

int runTruth(const Options &opt, Timings &t, Report &rep, std::ostream &out) {
    if (int rc = parseDesign(opt, t)) return rc;
    rep.designStats = collectDesignStats();

    // Covers initialization, all 2^n evaluations and formatting the rows
    auto start = Clock::now();
    bool ok = Simulator::generateTruthTable(out);
    t.simulate = msSince(start);
    rep.work = Simulator::stats;
    rep.simulated = true;
    return ok ? OK : RUNTIME_ERROR;
}

int runPorts(const Options &opt, Timings &t, Report &rep, std::ostream &out) {
    if (int rc = parseDesign(opt, t)) return rc;
    rep.designStats = collectDesignStats();
    out << header("inputs", sortedPorts(Simulator::primaryInputs)) << '\n';
    out << header("outputs", sortedPorts(Simulator::primaryOutputs)) << '\n';
    return OK;
}

}  // namespace

int main(int argc, char **argv) {
    std::ios::sync_with_stdio(false);

    Options opt;
    int rc = parseArgs(argc, argv, opt);
    if (rc == -1) return OK;
    if (rc != OK) return rc;

    using Command = int (*)(const Options &, Timings &, Report &, std::ostream &);
    Command command = opt.command == "sim"   ? runSim
                    : opt.command == "synth" ? runSynth
                    : opt.command == "truth" ? runTruth
                                             : runPorts;

    // Open the output first so a bad path fails before any work is done
    std::ofstream outFile;
    if (!opt.out.empty()) {
        outFile.open(opt.out);
        if (!outFile.is_open()) {
            std::cerr << "cannot open " << opt.out << " for writing\n";
            return RUNTIME_ERROR;
        }
    }
    std::ostream &dest = opt.out.empty() ? std::cout : outFile;

    Report rep;
    rep.command = opt.command;
    rep.design = opt.design;

    // Without -p there is one run straight to the destination. With -p every
    // run (warmups included) writes to memory and only the last one is kept,
    // so repeats do not duplicate the output.
    int totalRuns = opt.perf ? opt.warmup + opt.repeat : 1;
    std::string kept;
    for (int i = 0; i < totalRuns; i++) {
        Timings t;
        if (opt.perf) {
            std::ostringstream buf;
            rc = command(opt, t, rep, buf);
            if (i == totalRuns - 1) kept = buf.str();
        } else {
            rc = command(opt, t, rep, dest);
        }
        if (rc != OK) return rc;

        // Counters and design size are the same every run; only timings vary
        bool warmup = i < opt.warmup;
        if (!warmup) {
            rep.timings.push_back(t);
            rep.runs++;
        }
        if (opt.verbose && opt.perf)
            std::cerr << (warmup ? "warmup " : "run ") << (warmup ? i + 1 : i - opt.warmup + 1) << " done\n";
    }
    if (opt.perf) dest << kept;

    dest.flush();
    if (!dest) {
        std::cerr << "error writing " << (opt.out.empty() ? "standard output" : opt.out) << '\n';
        return RUNTIME_ERROR;
    }

    if (opt.perf) printReport(rep, opt.statsFormat, std::cerr);
    Simulator::clear();
    return OK;
}

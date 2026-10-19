#pragma once

// Shared helpers for hdl_sim tests: printing, probes, a manual circuit builder,
// a netlist harness, temp directories and stream capture.

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "framework/hdltest.hpp"

#include "core/Node.hpp"
#include "core/Port.hpp"
#include "core/Signal.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "sequential/SequentialNode.hpp"
#include "simulation/Simulator.hpp"

// ---------------------------------------------------------------------------
// Printing (so CHECK_EQ failures show "HIGH" / "Port::CLK" instead of numbers)
// ---------------------------------------------------------------------------

inline std::ostream &operator<<(std::ostream &os, Signal s) {
    return os << (s == Signal::HIGH ? "HIGH" : "LOW");
}

inline std::ostream &operator<<(std::ostream &os, Port p) {
    static const char *names[] = {"O", "A", "B", "S", "J", "K", "D", "T", "CLK"};
    auto i = static_cast<unsigned>(p);
    return os << "Port::" << (i < sizeof names / sizeof *names ? names[i] : "?");
}

namespace hdltest {

inline Signal sig(int b) { return b ? Signal::HIGH : Signal::LOW; }
inline int bit(Signal s) { return s == Signal::HIGH ? 1 : 0; }

// Fixed seed so random tests are reproducible run to run
inline std::mt19937 &rng() {
    static std::mt19937 g(0x5EED);
    return g;
}
inline int randomBit() { return std::uniform_int_distribution<int>(0, 1)(rng()); }
inline uint64_t randomBits(int n) {
    return std::uniform_int_distribution<uint64_t>(0, n >= 64 ? ~0ULL : (1ULL << n) - 1)(rng());
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

// Absolute path of a file under tests/fixtures (works from any cwd)
inline std::string fixture(const std::string &rel) {
    return std::string(HDLTEST_FIXTURE_DIR) + "/" + rel;
}

// A fresh directory that is removed (with its contents) when the object dies
class TempDir {
    std::filesystem::path dir;
public:
    TempDir() {
        std::string tmpl = (std::filesystem::temp_directory_path() / "hdltest-XXXXXX").string();
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');
        if (!mkdtemp(buf.data()))
            FAIL("cannot create temp dir from " << tmpl);
        dir = buf.data();
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
    TempDir(const TempDir &) = delete;
    TempDir &operator=(const TempDir &) = delete;

    std::string path(const std::string &name) const { return (dir / name).string(); }

    std::string write(const std::string &name, const std::string &content) const {
        std::string p = path(name);
        std::ofstream(p) << content;
        return p;
    }
};

inline std::string readFile(const std::string &path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline bool fileExists(const std::string &path) { return std::filesystem::exists(path); }

// Captures everything written to a std stream while alive
class Capture {
    std::ostream &stream;
    std::ostringstream buf;
    std::streambuf *old;
public:
    explicit Capture(std::ostream &s) : stream(s), old(s.rdbuf(buf.rdbuf())) {}
    ~Capture() { stream.rdbuf(old); }
    Capture(const Capture &) = delete;
    Capture &operator=(const Capture &) = delete;
    std::string str() const { return buf.str(); }
};

// ---------------------------------------------------------------------------
// Probes: nodes that record what the engine does to them
// ---------------------------------------------------------------------------

struct Call {
    Port port;
    Signal value;
    bool operator==(const Call &o) const { return port == o.port && value == o.value; }
};
inline std::ostream &operator<<(std::ostream &os, const Call &c) {
    return os << "(" << c.port << ", " << c.value << ")";
}

// Combinational-side probe: records evaluate() calls
struct ProbeNode : Node {
    std::vector<Call> calls;
    void evaluate(Port p, Signal v) override { calls.push_back({p, v}); }
    void setResult(Signal v) { result = v; }
};

// Sequential-side probe: records every hook the engine can call
struct ProbeSeqNode : SequentialNode {
    std::vector<std::string> log;
    void evaluate(Port p, Signal v) override { log.push_back("evaluate " + toString(p) + " " + toString(v)); }
    void setPort(Port p, Signal v) override { log.push_back("setPort " + toString(p) + " " + toString(v)); }
    void changeState() override { log.push_back("changeState"); }
};

// Runs every operation in a queue (including ones enqueued while draining)
inline int drain(OperationQueue &q) {
    int n = 0;
    while (auto op = q.dequeue()) {
        op->run();
        n++;
    }
    return n;
}

// ---------------------------------------------------------------------------
// Manual circuits
// ---------------------------------------------------------------------------

// Builds a circuit straight into Simulator::nodes. Add nodes in topological
// order (drivers before combinational consumers): each node's priority is its
// insertion index, which is what the Parser would assign.
class Circuit {
    int nextPriority = 0;

    template <class T>
    T &own(std::unique_ptr<T> p) {
        T &ref = *p;
        ref.priority = nextPriority++;
        Simulator::nodes.push_back(std::move(p));
        return ref;
    }

public:
    Circuit() { Simulator::clear(); }
    ~Circuit() { Simulator::clear(); }
    Circuit(const Circuit &) = delete;
    Circuit &operator=(const Circuit &) = delete;

    template <class T>
    T &add() { return own(std::make_unique<T>()); }

    InputNode &input(const std::string &name) {
        auto &n = add<InputNode>();
        Simulator::primaryInputs[name].push_back(&n);
        return n;
    }

    OutputNode &output(const std::string &name) {
        auto &n = add<OutputNode>();
        Simulator::primaryOutputs[name].push_back(&n);
        return n;
    }

    static void connect(Node &from, Node &to, Port p) { from.addConnection(to, p); }

    // Output node fed by `from`
    OutputNode &probe(Node &from, const std::string &name) {
        auto &o = output(name);
        connect(from, o, Port::O);
        return o;
    }

    static void init() { Simulator::initialize(); }

    // Drive an input and let the circuit settle
    static void set(InputNode &in, int v) {
        in.evaluate(Port::O, sig(v));
        Simulator::run();
    }

    // Drive several inputs "at the same time", then settle once
    static void setAll(const std::vector<std::pair<InputNode *, int>> &values) {
        for (auto &[in, v] : values)
            in->evaluate(Port::O, sig(v));
        Simulator::run();
    }

    // One full clock cycle: rising edge, then falling edge
    static void pulse(InputNode &clk) {
        set(clk, 1);
        set(clk, 0);
    }

    static int read(Node &n) { return bit(n.getResult()); }
};

// ---------------------------------------------------------------------------
// Netlists
// ---------------------------------------------------------------------------

// Parses a netlist into the Simulator and drives it by port name.
// Bus bit i is element i of the parser's vector (bit 0 = LSB).
class NetlistSim {
public:
    int rc = -1;
    std::string parseLog;   // whatever the parser wrote to std::cerr

    explicit NetlistSim(const std::string &path, bool initialize = true) {
        Simulator::clear();
        {
            Capture err(std::cerr);
            rc = Simulator::parse(path);
            parseLog = err.str();
        }
        if (rc != 0)
            FAIL("parsing " << path << " failed (rc " << rc << "):\n" << parseLog);
        if (initialize)
            Simulator::initialize();
    }
    ~NetlistSim() { Simulator::clear(); }
    NetlistSim(const NetlistSim &) = delete;
    NetlistSim &operator=(const NetlistSim &) = delete;

    static std::vector<Node *> &inputs(const std::string &name) {
        auto it = Simulator::primaryInputs.find(name);
        if (it == Simulator::primaryInputs.end())
            FAIL("netlist has no primary input '" << name << "'");
        return it->second;
    }

    static std::vector<Node *> &outputs(const std::string &name) {
        auto it = Simulator::primaryOutputs.find(name);
        if (it == Simulator::primaryOutputs.end())
            FAIL("netlist has no primary output '" << name << "'");
        return it->second;
    }

    static int width(const std::string &name) {
        auto in = Simulator::primaryInputs.find(name);
        if (in != Simulator::primaryInputs.end()) return static_cast<int>(in->second.size());
        return static_cast<int>(outputs(name).size());
    }

    // Drive input `name` without settling
    static void drive(const std::string &name, uint64_t value) {
        auto &bits = inputs(name);
        for (size_t i = 0; i < bits.size(); i++)
            bits[i]->evaluate(Port::O, sig((value >> i) & 1));
    }

    static void settle() { Simulator::run(); }

    // Drive and settle
    static void set(const std::string &name, uint64_t value) {
        drive(name, value);
        settle();
    }

    static uint64_t get(const std::string &name) {
        auto &bits = outputs(name);
        uint64_t v = 0;
        for (size_t i = 0; i < bits.size(); i++)
            v |= static_cast<uint64_t>(bit(bits[i]->getResult())) << i;
        return v;
    }

    static void clock(const std::string &clk = "clk") {
        set(clk, 1);
        set(clk, 0);
    }
};

}  // namespace hdltest

// Friend of Node and List (see core/Node.hpp, core/List.hpp): read-only access
// to a node's outgoing connections, in list order.
struct ParserTestAccess {
    static std::vector<std::pair<const Node *, Port>> connections(const Node &n) {
        std::vector<std::pair<const Node *, Port>> out;
        for (auto *c = n.connections.head; c; c = c->next)
            out.push_back({&c->target, c->p});
        return out;
    }
};

namespace hdltest {

inline std::vector<std::pair<const Node *, Port>> connectionsOf(const Node &n) {
    return ParserTestAccess::connections(n);
}

inline bool drives(const Node &from, const Node *to, Port p) {
    for (auto &[n, port] : connectionsOf(from))
        if (n == to && port == p) return true;
    return false;
}

}  // namespace hdltest

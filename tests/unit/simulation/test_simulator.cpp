// Simulator: clear/parse/initialize/run and the truth-table generator.

#include <map>

#include "support/helpers.hpp"

#include "combinational/AND.hpp"
#include "combinational/NOT.hpp"
#include "sequential/DFF_P.hpp"

using namespace hdltest;

namespace {

// The file written by generateTruthTable:
//   a b | y
//   0 0 | 0
//   ...
struct TruthTable {
    std::vector<std::string> inLabels, outLabels;
    std::vector<std::vector<int>> ins, outs;   // one entry per row
};

// Splits "x y | z" into its two sides
template <class T>
std::pair<std::vector<T>, std::vector<T>> splitAtBar(const std::string &line) {
    std::istringstream in(line);
    std::pair<std::vector<T>, std::vector<T>> sides;
    bool right = false;
    for (std::string tok; in >> tok;) {
        if (tok == "|") { right = true; continue; }
        T v;
        std::istringstream(tok) >> v;
        (right ? sides.second : sides.first).push_back(v);
    }
    return sides;
}

TruthTable readTable(const std::string &path) {
    TruthTable t;
    std::istringstream in(readFile(path));
    std::string line;
    if (!std::getline(in, line)) return t;
    std::tie(t.inLabels, t.outLabels) = splitAtBar<std::string>(line);
    while (std::getline(in, line)) {
        auto [i, o] = splitAtBar<int>(line);
        t.ins.push_back(i);
        t.outs.push_back(o);
    }
    return t;
}

// Generates the table of the current circuit into a temp file and reads it back
TruthTable generate() {
    TempDir dir;
    std::string path = dir.path("table.txt");
    Capture err(std::cerr);
    bool ok = Simulator::generateTruthTable(path);
    INFO("stderr: " << err.str());
    REQUIRE(ok);
    CHECK_EQ(err.str(), "");
    return readTable(path);
}

// Integer value of bus `name` in one row, read through the column labels
unsigned busValue(const std::vector<std::string> &labels, const std::vector<int> &row, const std::string &name) {
    unsigned v = 0;
    for (size_t i = 0; i < labels.size(); i++) {
        if (labels[i].rfind(name + "[", 0) != 0) continue;
        int idx = std::stoi(labels[i].substr(name.size() + 1));
        v |= static_cast<unsigned>(row[i]) << idx;
    }
    return v;
}

}  // namespace

// ---------------------------------------------------------------------------
// clear / parse / initialize / run
// ---------------------------------------------------------------------------

TEST(simulator, clear_resets_all_state) {
    Simulator::clear();
    {
        NetlistSim sim(fixture("netlists/and/netlist.table"), false);
        REQUIRE(!Simulator::nodes.empty());
        Simulator::primaryInputs["a"][0]->evaluate(Port::O, Signal::HIGH);   // queue some work
        REQUIRE(!Simulator::combinationalQueue.isEmpty());
        Simulator::clear();
        CHECK(Simulator::nodes.empty());
        CHECK(Simulator::primaryInputs.empty());
        CHECK(Simulator::primaryOutputs.empty());
        CHECK(Simulator::combinationalQueue.isEmpty());
        CHECK(Simulator::sequentialQueue.isEmpty());
        CHECK(Simulator::topologicalQueue.isEmpty());
    }
}

TEST(simulator, parse_fills_the_global_tables) {
    Simulator::clear();
    Capture err(std::cerr);
    REQUIRE_EQ(Simulator::parse(fixture("netlists/and/netlist.table")), 0);
    CHECK_EQ(Simulator::nodes.size(), 4u);
    CHECK_EQ(Simulator::primaryInputs.size(), 2u);
    CHECK_EQ(Simulator::primaryOutputs.size(), 1u);
    CHECK_EQ(Simulator::primaryInputs.count("a"), 1u);
    CHECK_EQ(Simulator::primaryOutputs.count("y"), 1u);
}

TEST(simulator, parse_reports_failure) {
    Simulator::clear();
    Capture err(std::cerr);
    CHECK_EQ(Simulator::parse("/nonexistent/netlist.table"), -1);
    CHECK_CONTAINS(err.str(), "cannot open file");
}

TEST(simulator, run_with_nothing_queued_returns) {
    Simulator::clear();
    Simulator::run();
    CHECK(Simulator::combinationalQueue.isEmpty());
}

TEST(simulator, initialize_settles_the_circuit_and_empties_queues) {
    // a -> NOT -> y must read 1 right after initialize()
    Circuit c;
    auto &a = c.input("a");
    auto &n = c.add<NOT>();
    auto &y = c.probe(n, "y");
    Circuit::connect(a, n, Port::A);
    c.init();
    CHECK_EQ(Circuit::read(y), 1);
    CHECK(Simulator::combinationalQueue.isEmpty());
    CHECK(Simulator::sequentialQueue.isEmpty());
    CHECK(Simulator::topologicalQueue.isEmpty());
}

TEST(simulator, initialize_drives_registered_inputs_low) {
    Circuit c;
    auto &a = c.input("a");
    auto &y = c.probe(a, "y");
    a.evaluate(Port::O, Signal::HIGH);   // before initialize
    c.init();
    CHECK_EQ(Circuit::read(a), 0);
    CHECK_EQ(Circuit::read(y), 0);
}

TEST(simulator, initialize_settles_flip_flop_low) {
    Circuit c;
    auto &d = c.input("d");
    auto &clk = c.input("clk");
    auto &ff = c.add<DFF_P>();
    auto &q = c.probe(ff, "q");
    Circuit::connect(d, ff, Port::D);
    Circuit::connect(clk, ff, Port::CLK);
    c.init();
    CHECK_EQ(Circuit::read(q), 0);
}

TEST(simulator, run_without_input_change_keeps_outputs) {
    NetlistSim sim(fixture("netlists/and/netlist.table"));
    NetlistSim::set("a", 1);
    NetlistSim::set("b", 1);
    REQUIRE_EQ(NetlistSim::get("y"), 1u);
    for (int i = 0; i < 3; i++) {
        Simulator::run();
        CHECK_EQ(NetlistSim::get("y"), 1u);
    }
}

TEST(simulator, reconvergent_fanout_does_not_glitch) {
    // x = a AND NOT(NOT(NOT(a))) is 0 for any a. The slow path is three levels
    // deep, so an event-order simulator would briefly see x = 1 after a rises.
    // Topological commit order must never forward that glitch.
    Circuit c;
    auto &a = c.input("a");
    auto &n1 = c.add<NOT>();
    auto &n2 = c.add<NOT>();
    auto &n3 = c.add<NOT>();
    auto &x = c.add<AND>();
    auto &probe = c.add<ProbeNode>();
    Circuit::connect(a, n1, Port::A);
    Circuit::connect(n1, n2, Port::A);
    Circuit::connect(n2, n3, Port::A);
    Circuit::connect(a, x, Port::A);
    Circuit::connect(n3, x, Port::B);
    Circuit::connect(x, probe, Port::A);
    c.init();
    probe.calls.clear();

    for (int v : {1, 0, 1, 0}) {
        Circuit::set(a, v);
        INFO("a=" << v << " probe saw " << toString(probe.calls));
        for (auto &call : probe.calls) CHECK_EQ(call.value, Signal::LOW);
    }
}

// ---------------------------------------------------------------------------
// generateTruthTable
// ---------------------------------------------------------------------------

TEST(truth_table, and_gate_rows) {
    NetlistSim sim(fixture("netlists/and/netlist.table"));
    auto t = generate();
    CHECK_EQ(t.inLabels, (std::vector<std::string>{"a", "b"}));
    CHECK_EQ(t.outLabels, (std::vector<std::string>{"y"}));
    CHECK_EQ(t.ins, (std::vector<std::vector<int>>{{0, 0}, {0, 1}, {1, 0}, {1, 1}}));
    CHECK_EQ(t.outs, (std::vector<std::vector<int>>{{0}, {0}, {0}, {1}}));
}

TEST(truth_table, file_format_is_exact) {
    NetlistSim sim(fixture("netlists/and/netlist.table"));
    TempDir dir;
    REQUIRE(Simulator::generateTruthTable(dir.path("table.txt")));
    CHECK_EQ(readFile(dir.path("table.txt")),
             "a b | y \n"
             "0 0 | 0 \n"
             "0 1 | 0 \n"
             "1 0 | 0 \n"
             "1 1 | 1 \n");
}

TEST(truth_table, nothing_is_printed_to_stdout) {
    NetlistSim sim(fixture("netlists/and/netlist.table"));
    TempDir dir;
    Capture out(std::cout);
    REQUIRE(Simulator::generateTruthTable(dir.path("table.txt")));
    CHECK_EQ(out.str(), "");
}

TEST(truth_table, all_zero_first_row_is_correct_for_inverting_logic) {
    // decoder y0 = ~a & ~b is 1 on the all-zero row, so that row must propagate
    NetlistSim sim(fixture("netlists/decode2-4/netlist.table"));
    auto t = generate();
    REQUIRE_EQ(t.outLabels, (std::vector<std::string>{"y0", "y1", "y2", "y3"}));
    CHECK_EQ(t.outs, (std::vector<std::vector<int>>{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}));
}

TEST(truth_table, rows_count_up_in_binary_with_first_column_as_msb) {
    NetlistSim sim(fixture("netlists/comparator/netlist.table"));
    auto t = generate();
    REQUIRE_EQ(t.ins.size(), 256u);
    for (size_t r = 0; r < t.ins.size(); r++) {
        unsigned v = 0;
        for (int b : t.ins[r]) v = (v << 1) | static_cast<unsigned>(b);
        INFO("row " << r);
        REQUIRE_EQ(v, r);
    }
}

TEST(truth_table, matches_direct_simulation_for_every_row) {
    NetlistSim sim(fixture("netlists/comparator/netlist.table"));
    auto t = generate();
    REQUIRE_EQ(t.ins.size(), 256u);
    REQUIRE_EQ(t.outs.size(), 256u);

    // Columns are: names sorted, then each bus from its msb (last element) down.
    // Re-drive those exact nodes and compare.
    std::vector<Node *> inNodes, outNodes;
    for (auto *m : {&Simulator::primaryInputs, &Simulator::primaryOutputs}) {
        std::map<std::string, std::vector<Node *>> sorted(m->begin(), m->end());
        for (auto &[name, bits] : sorted)
            for (auto it = bits.rbegin(); it != bits.rend(); ++it)
                (m == &Simulator::primaryInputs ? inNodes : outNodes).push_back(*it);
    }
    for (size_t r = 0; r < t.ins.size(); r++) {
        for (size_t i = 0; i < inNodes.size(); i++) inNodes[i]->evaluate(Port::O, sig(t.ins[r][i]));
        Simulator::run();
        std::vector<int> got;
        for (auto *n : outNodes) got.push_back(bit(n->getResult()));
        INFO("row " << r);
        CHECK_EQ(got, t.outs[r]);
    }
}

TEST(truth_table, bus_labels_run_from_msb_to_lsb) {
    NetlistSim sim(fixture("netlists/adder/netlist.table"));
    auto t = generate();
    CHECK_EQ(t.inLabels, (std::vector<std::string>{"a[3]", "a[2]", "a[1]", "a[0]", "b[3]", "b[2]", "b[1]", "b[0]"}));
    CHECK_EQ(t.outLabels, (std::vector<std::string>{"y[3]", "y[2]", "y[1]", "y[0]"}));
}

TEST(truth_table, bus_bit_labels_match_bit_positions) {
    // adder: y = a + b. Read the table by its labels and check the arithmetic.
    NetlistSim sim(fixture("netlists/adder/netlist.table"));
    auto t = generate();
    REQUIRE_EQ(t.ins.size(), 256u);
    for (size_t r = 0; r < t.ins.size(); r++) {
        unsigned a = busValue(t.inLabels, t.ins[r], "a"), b = busValue(t.inLabels, t.ins[r], "b");
        unsigned y = busValue(t.outLabels, t.outs[r], "y");
        INFO("row " << r << ": a=" << a << " b=" << b);
        REQUIRE_EQ(y, (a + b) & 0xF);
    }
}

TEST(truth_table, result_does_not_depend_on_prior_input_state) {
    // the generator re-initializes, so leftover input values must not leak in
    NetlistSim sim(fixture("netlists/decode2-4/netlist.table"));
    sim.set("a", 1);
    sim.set("b", 1);
    auto t = generate();
    REQUIRE_EQ(t.outs.size(), 4u);
    CHECK_EQ(t.outs[0], (std::vector<int>{1, 0, 0, 0}));
}

TEST(truth_table, can_be_generated_twice) {
    NetlistSim sim(fixture("netlists/and/netlist.table"));
    auto first = generate();
    auto second = generate();
    CHECK_EQ(first.ins, second.ins);
    CHECK_EQ(first.outs, second.outs);
}

TEST(truth_table, rejects_netlist_without_inputs) {
    Simulator::clear();
    TempDir dir;
    Capture err(std::cerr);
    CHECK(!Simulator::generateTruthTable(dir.path("table.txt")));
    CHECK_CONTAINS(err.str(), "no primary inputs");
    CHECK(!fileExists(dir.path("table.txt")));
}

TEST(truth_table, rejects_more_than_20_inputs) {
    Circuit c;
    for (int i = 0; i < 21; i++) c.probe(c.input("i" + std::to_string(i)), "o" + std::to_string(i));
    c.init();
    TempDir dir;
    Capture err(std::cerr);
    CHECK(!Simulator::generateTruthTable(dir.path("table.txt")));
    CHECK_CONTAINS(err.str(), "21 primary inputs");
    CHECK(!fileExists(dir.path("table.txt")));
}

TEST(truth_table, rejects_sequential_circuits) {
    NetlistSim sim(fixture("netlists/ringcounter/netlist.table"));
    TempDir dir;
    Capture err(std::cerr);
    CHECK(!Simulator::generateTruthTable(dir.path("table.txt")));
    CHECK_CONTAINS(err.str(), "sequential");
    CHECK(!fileExists(dir.path("table.txt")));
}

TEST(truth_table, rejects_hand_built_circuit_with_a_flip_flop) {
    Circuit c;
    auto &d = c.input("d");
    auto &ff = c.add<DFF_P>();
    c.probe(ff, "q");
    Circuit::connect(d, ff, Port::D);
    c.init();
    TempDir dir;
    Capture err(std::cerr);
    CHECK(!Simulator::generateTruthTable(dir.path("table.txt")));
    CHECK_CONTAINS(err.str(), "sequential");
}

TEST(truth_table, rejects_unwritable_output_path) {
    NetlistSim sim(fixture("netlists/and/netlist.table"));
    Capture err(std::cerr);
    CHECK(!Simulator::generateTruthTable("/nonexistent/dir/table.txt"));
    CHECK_CONTAINS(err.str(), "cannot open");
}

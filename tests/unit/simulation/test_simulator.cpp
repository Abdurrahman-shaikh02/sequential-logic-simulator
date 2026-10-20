// Simulator: clear/parse/initialize/run and the truth-table generator.

#include <map>

#include "support/helpers.hpp"

#include "combinational/AND.hpp"
#include "combinational/NOT.hpp"
#include "sequential/DFF_P.hpp"

using namespace hdltest;

namespace {

std::vector<std::vector<int>> readRows(const std::string &path) {
    std::vector<std::vector<int>> rows;
    std::istringstream in(readFile(path));
    for (std::string line; std::getline(in, line);) {
        std::istringstream ls(line);
        std::vector<int> r;
        for (int v; ls >> v;) r.push_back(v);
        rows.push_back(r);
    }
    return rows;
}

// Header printed by generateTruthTable: "a b | y" -> ({a, b}, {y})
std::pair<std::vector<std::string>, std::vector<std::string>> readHeader(const std::string &out) {
    std::istringstream in(out.substr(0, out.find('\n')));
    std::pair<std::vector<std::string>, std::vector<std::string>> h;
    bool outputs = false;
    for (std::string tok; in >> tok;) {
        if (tok == "|") outputs = true;
        else (outputs ? h.second : h.first).push_back(tok);
    }
    return h;
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
    TempDir dir;
    Capture out(std::cout);
    REQUIRE(Simulator::generateTruthTable(dir.path("in.txt"), dir.path("out.txt")));

    CHECK_EQ(readRows(dir.path("in.txt")), (std::vector<std::vector<int>>{{0, 0}, {0, 1}, {1, 0}, {1, 1}}));
    CHECK_EQ(readRows(dir.path("out.txt")), (std::vector<std::vector<int>>{{0}, {0}, {0}, {1}}));
    auto [ins, outs] = readHeader(out.str());
    CHECK_EQ(ins, (std::vector<std::string>{"a", "b"}));
    CHECK_EQ(outs, (std::vector<std::string>{"y"}));
}

TEST(truth_table, all_zero_first_row_is_correct_for_inverting_logic) {
    // decoder y0 = ~a & ~b is 1 on the all-zero row: the generator's priming
    // step must make that row propagate
    NetlistSim sim(fixture("netlists/decode2-4/netlist.table"));
    TempDir dir;
    Capture out(std::cout);
    REQUIRE(Simulator::generateTruthTable(dir.path("in.txt"), dir.path("out.txt")));
    auto rows = readRows(dir.path("out.txt"));
    REQUIRE_EQ(rows.size(), 4u);
    CHECK_EQ(rows[0], (std::vector<int>{1, 0, 0, 0}));   // y0 y1 y2 y3
    CHECK_EQ(rows[3], (std::vector<int>{0, 0, 0, 1}));
}

TEST(truth_table, matches_direct_simulation_for_every_row) {
    NetlistSim sim(fixture("netlists/comparator/netlist.table"));
    TempDir dir;
    {
        Capture out(std::cout);
        REQUIRE(Simulator::generateTruthTable(dir.path("in.txt"), dir.path("out.txt")));
    }
    auto ins = readRows(dir.path("in.txt")), outs = readRows(dir.path("out.txt"));
    REQUIRE_EQ(ins.size(), 256u);
    REQUIRE_EQ(outs.size(), 256u);

    // Columns follow the vector order the simulator uses: names sorted, then
    // each bus element in parser order. Re-drive those exact nodes and compare.
    std::vector<Node *> inNodes, outNodes;
    for (auto *m : {&Simulator::primaryInputs, &Simulator::primaryOutputs}) {
        std::map<std::string, std::vector<Node *>> sorted(m->begin(), m->end());
        for (auto &[name, bits] : sorted)
            for (auto *n : bits) (m == &Simulator::primaryInputs ? inNodes : outNodes).push_back(n);
    }
    for (size_t r = 0; r < ins.size(); r++) {
        for (size_t i = 0; i < inNodes.size(); i++) inNodes[i]->evaluate(Port::O, sig(ins[r][i]));
        Simulator::run();
        std::vector<int> got;
        for (auto *n : outNodes) got.push_back(bit(n->getResult()));
        INFO("row " << r);
        CHECK_EQ(got, outs[r]);
    }
}

TEST_XFAIL(truth_table, bus_bit_labels_match_bit_positions,
           "generateTruthTable's flatten() labels vector element 0 as name[N-1], but the "
           "parser stores bit 0 at element 0, so every bus column is labelled with the "
           "mirrored bit index (a[3] is really a[0])") {
    // adder: y = a + b. Read the table by its labels and check the arithmetic.
    NetlistSim sim(fixture("netlists/adder/netlist.table"));
    TempDir dir;
    Capture out(std::cout);
    REQUIRE(Simulator::generateTruthTable(dir.path("in.txt"), dir.path("out.txt")));
    auto [inLabels, outLabels] = readHeader(out.str());
    auto ins = readRows(dir.path("in.txt")), outs = readRows(dir.path("out.txt"));

    auto value = [](const std::vector<std::string> &labels, const std::vector<int> &row, const std::string &bus) {
        unsigned v = 0;
        for (size_t i = 0; i < labels.size(); i++) {
            if (labels[i].rfind(bus + "[", 0) != 0) continue;
            int idx = std::stoi(labels[i].substr(bus.size() + 1));
            v |= static_cast<unsigned>(row[i]) << idx;
        }
        return v;
    };
    for (size_t r = 0; r < ins.size(); r++) {
        unsigned a = value(inLabels, ins[r], "a"), b = value(inLabels, ins[r], "b");
        unsigned y = value(outLabels, outs[r], "y");
        INFO("row " << r << ": a=" << a << " b=" << b);
        REQUIRE_EQ(y, (a + b) & 0xF);
    }
}

TEST(truth_table, rejects_netlist_without_inputs) {
    Simulator::clear();
    TempDir dir;
    CHECK(!Simulator::generateTruthTable(dir.path("in.txt"), dir.path("out.txt")));
}

TEST(truth_table, rejects_more_than_20_inputs) {
    Circuit c;
    for (int i = 0; i < 21; i++) c.probe(c.input("i" + std::to_string(i)), "o" + std::to_string(i));
    c.init();
    TempDir dir;
    CHECK(!Simulator::generateTruthTable(dir.path("in.txt"), dir.path("out.txt")));
}

TEST(truth_table, rejects_unwritable_output_path) {
    NetlistSim sim(fixture("netlists/and/netlist.table"));
    Capture out(std::cout);
    CHECK(!Simulator::generateTruthTable("/nonexistent/dir/in.txt", "/nonexistent/dir/out.txt"));
}

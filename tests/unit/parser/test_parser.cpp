// tests/unit/parser/test_parser.cpp
//
// Tests for Parser::parse(): netlist validation + graph creation.
// Self-contained (no gtest). Each test writes a netlist to a temp file.
//
// !! ADAPT THIS: the only Node API assumption is in connectionsOf() below.
// !! Change it to however your Node exposes its outgoing connections.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#include <utility>

#include "parser/Parser.hpp"
#include "core/Node.hpp"
#include "core/Port.hpp"
#include "combinational/AND.hpp"
#include "combinational/NOT.hpp"
#include "combinational/MUX.hpp"
#include "combinational/OR.hpp"
#include "sequential/DFF_P.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"

// ASSUMPTION: adapt to your Node interface.
// Must return the list of (consumer node, consumer port) that `n` drives,
// i.e. what addConnection(consumer, port) stored.
static std::vector<std::pair<const Node*, Port>> connectionsOf(const Node& n) {
    std::vector<std::pair<const Node*, Port>> out;
    for (const auto& c : n.getConnections())          // <-- adapt
        out.push_back({&c.node, c.port});             // <-- adapt
    return out;
}

// Mini harness
static int g_failed = 0, g_checks = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failed; \
    std::cerr << "    CHECK FAILED " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } } while (0)

struct TestCase { std::string name; std::function<void()> fn; };
static std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }
struct Reg { Reg(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };
#define TEST(name) static void name(); static Reg reg_##name(#name, name); static void name()

// Helpers
namespace fs = std::filesystem;

// Joins fields with tabs -> one netlist row (module comp type port dir wire)
static std::string row(const std::string& mod, const std::string& comp, const std::string& type,
                       const std::string& port, const std::string& dir, const std::string& wire) {
    return mod + "\t" + comp + "\t" + type + "\t" + port + "\t" + dir + "\t" + wire + "\n";
}

static std::string writeNetlist(const std::string& testName, const std::string& content) {
    fs::path p = fs::temp_directory_path() / ("parser_test_" + testName + ".net");
    std::ofstream(p) << content;
    return p.string();
}

struct Result {
    int rc = 0;
    std::vector<std::unique_ptr<Node>> nodes;
    int pi = 0, po = 0;
    std::string err;
};

// Runs the parser and captures std::cerr so error messages can be asserted.
static Result run(const std::string& testName, const std::string& content) {
    Result r;
    std::string path = writeNetlist(testName, content);
    std::ostringstream cap;
    auto* old = std::cerr.rdbuf(cap.rdbuf());
    r.rc = Parser::parse(path, r.nodes, r.pi, r.po);
    std::cerr.rdbuf(old);
    r.err = cap.str();
    fs::remove(path);
    return r;
}

static bool errHas(const Result& r, const std::string& s) { return r.err.find(s) != std::string::npos; }

template <class T>
static std::vector<T*> nodesOfType(const Result& r) {
    std::vector<T*> v;
    for (auto& n : r.nodes) if (auto* p = dynamic_cast<T*>(n.get())) v.push_back(p);
    return v;
}

static bool drives(const Node& from, const Node* to, Port p) {
    for (auto& [n, port] : connectionsOf(from)) if (n == to && port == p) return true;
    return false;
}

// Reusable netlist fragments -------------------------------------------------
static const std::string AND_NETLIST =
    row("test", "a", "-", "-", "pi", "\\a") +
    row("test", "b", "-", "-", "pi", "\\b") +
    row("test", "y", "-", "-", "po", "$and$test.v:2$1_Y") +
    row("test", "$and$test.v:2$1", "$and", "A", "in",  "\\a") +
    row("test", "$and$test.v:2$1", "$and", "B", "in",  "\\b") +
    row("test", "$and$test.v:2$1", "$and", "Y", "out", "$and$test.v:2$1_Y");

// POSITIVE: parsing + graph creation

TEST(and_gate_counts_and_node_types) {
    auto r = run("and_ok", AND_NETLIST);
    CHECK(r.rc == 0);
    CHECK(r.pi == 2);
    CHECK(r.po == 1);
    CHECK(r.nodes.size() == 4);
    CHECK(nodesOfType<InputNode>(r).size() == 2);
    CHECK(nodesOfType<OutputNode>(r).size() == 1);
    CHECK(nodesOfType<AND>(r).size() == 1);
    CHECK(r.err.empty());
}

TEST(and_gate_graph_connections) {
    auto r = run("and_graph", AND_NETLIST);
    CHECK(r.rc == 0);
    auto ins = nodesOfType<InputNode>(r);
    auto outs = nodesOfType<OutputNode>(r);
    auto ands = nodesOfType<AND>(r);
    if (ins.size() != 2 || outs.size() != 1 || ands.size() != 1) { CHECK(false); return; }

    // Input a -> AND.A ; input b -> AND.B (creation order: a first, b second)
    CHECK(drives(*ins[0], ands[0], Port::A));
    CHECK(drives(*ins[1], ands[0], Port::B));
    CHECK(connectionsOf(*ins[0]).size() == 1);
    CHECK(connectionsOf(*ins[1]).size() == 1);

    // AND -> output node (primary outputs are connected on Port::O)
    CHECK(drives(*ands[0], outs[0], Port::O));
    CHECK(connectionsOf(*ands[0]).size() == 1);

    // Output node drives nothing
    CHECK(connectionsOf(*outs[0]).empty());
}

TEST(comments_and_blank_lines_ignored) {
    std::string nl =
        "# full-line comment\n"
        "\n"
        "   \t  \n" +
        row("test", "a", "-", "-", "pi", "w1") +
        "test\ty\t-\t-\tpo\tw2   # trailing comment\n" +
        row("test", "g", "$not", "A", "in", "w1") +
        row("test", "g", "$not", "Y", "out", "w2");
    auto r = run("comments", nl);
    CHECK(r.rc == 0);
    CHECK(r.nodes.size() == 3);
    CHECK(r.pi == 1 && r.po == 1);
}

TEST(extra_whitespace_between_columns_ok) {
    std::string nl =
        "test    a   -   -   pi   w1\n"
        "test    y   -   -   po   w2\n"
        "test    g   $not   A   in    w1\n"
        "test    g   $not   Y   out   w2\n";
    auto r = run("spaces", nl);
    CHECK(r.rc == 0);
}

TEST(not_gate_single_input) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "w1") +
        row("t", "y", "-", "-", "po", "w2") +
        row("t", "n", "$not", "A", "in", "w1") +
        row("t", "n", "$not", "Y", "out", "w2");
    auto r = run("not_ok", nl);
    CHECK(r.rc == 0);
    CHECK(nodesOfType<NOT>(r).size() == 1);
    auto ins = nodesOfType<InputNode>(r);
    auto nots = nodesOfType<NOT>(r);
    if (!ins.empty() && !nots.empty()) CHECK(drives(*ins[0], nots[0], Port::A));
}

TEST(mux_three_inputs_ports_mapped) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "b", "-", "-", "pi", "wb") +
        row("t", "s", "-", "-", "pi", "ws") +
        row("t", "y", "-", "-", "po", "wy") +
        row("t", "m", "$mux", "A", "in", "wa") +
        row("t", "m", "$mux", "B", "in", "wb") +
        row("t", "m", "$mux", "S", "in", "ws") +
        row("t", "m", "$mux", "Y", "out", "wy");
    auto r = run("mux_ok", nl);
    CHECK(r.rc == 0);
    auto ins = nodesOfType<InputNode>(r);
    auto mux = nodesOfType<MUX>(r);
    if (ins.size() == 3 && mux.size() == 1) {
        CHECK(drives(*ins[0], mux[0], Port::A));
        CHECK(drives(*ins[1], mux[0], Port::B));
        CHECK(drives(*ins[2], mux[0], Port::S));
    } else CHECK(false);
}

TEST(dff_p_ports_D_CLK_Q) {
    std::string nl =
        row("t", "d",   "-", "-", "pi", "wd") +
        row("t", "clk", "-", "-", "pi", "wc") +
        row("t", "q",   "-", "-", "po", "wq") +
        row("t", "ff", "$dff_p", "D",   "in",  "wd") +
        row("t", "ff", "$dff_p", "CLK", "in",  "wc") +
        row("t", "ff", "$dff_p", "Q",   "out", "wq");
    auto r = run("dff_ok", nl);
    CHECK(r.rc == 0);
    auto ins = nodesOfType<InputNode>(r);
    auto ffs = nodesOfType<DFF_P>(r);
    if (ins.size() == 2 && ffs.size() == 1) {
        CHECK(drives(*ins[0], ffs[0], Port::D));
        CHECK(drives(*ins[1], ffs[0], Port::CLK));
    } else CHECK(false);
}

TEST(fanout_one_wire_two_consumers) {
    // a drives both a NOT and the A pin of an OR
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "b", "-", "-", "pi", "wb") +
        row("t", "y1", "-", "-", "po", "w1") +
        row("t", "y2", "-", "-", "po", "w2") +
        row("t", "n", "$not", "A", "in", "wa") +
        row("t", "n", "$not", "Y", "out", "w1") +
        row("t", "o", "$or", "A", "in", "wa") +
        row("t", "o", "$or", "B", "in", "wb") +
        row("t", "o", "$or", "Y", "out", "w2");
    auto r = run("fanout", nl);
    CHECK(r.rc == 0);
    auto ins = nodesOfType<InputNode>(r);
    auto nots = nodesOfType<NOT>(r);
    auto ors = nodesOfType<OR>(r);
    if (ins.size() == 2 && nots.size() == 1 && ors.size() == 1) {
        CHECK(connectionsOf(*ins[0]).size() == 2);
        CHECK(drives(*ins[0], nots[0], Port::A));
        CHECK(drives(*ins[0], ors[0], Port::A));
        CHECK(connectionsOf(*ins[1]).size() == 1);
        CHECK(drives(*ins[1], ors[0], Port::B));
    } else CHECK(false);
}

TEST(chained_gates_and_then_not) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "b", "-", "-", "pi", "wb") +
        row("t", "y", "-", "-", "po", "wy") +
        row("t", "g1", "$and", "A", "in", "wa") +
        row("t", "g1", "$and", "B", "in", "wb") +
        row("t", "g1", "$and", "Y", "out", "wmid") +
        row("t", "g2", "$not", "A", "in", "wmid") +
        row("t", "g2", "$not", "Y", "out", "wy");
    auto r = run("chain", nl);
    CHECK(r.rc == 0);
    auto ands = nodesOfType<AND>(r);
    auto nots = nodesOfType<NOT>(r);
    auto outs = nodesOfType<OutputNode>(r);
    if (ands.size() == 1 && nots.size() == 1 && outs.size() == 1) {
        CHECK(drives(*ands[0], nots[0], Port::A));
        CHECK(drives(*nots[0], outs[0], Port::O));
    } else CHECK(false);
}

TEST(port_rows_in_any_order_same_gate) {
    // Y row first, then B, then A -- component created on first row seen
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "b", "-", "-", "pi", "wb") +
        row("t", "y", "-", "-", "po", "wy") +
        row("t", "g", "$and", "Y", "out", "wy") +
        row("t", "g", "$and", "B", "in", "wb") +
        row("t", "g", "$and", "A", "in", "wa");
    auto r = run("order", nl);
    CHECK(r.rc == 0);
    CHECK(nodesOfType<AND>(r).size() == 1);
}

TEST(same_gate_names_across_two_instances) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "y", "-", "-", "po", "wy") +
        row("t", "n1", "$not", "A", "in", "wa") +
        row("t", "n1", "$not", "Y", "out", "wm") +
        row("t", "n2", "$not", "A", "in", "wm") +
        row("t", "n2", "$not", "Y", "out", "wy");
    auto r = run("two_nots", nl);
    CHECK(r.rc == 0);
    CHECK(nodesOfType<NOT>(r).size() == 2);
}

// NEGATIVE: every error path in Parser::parse()

TEST(err_file_not_found) {
    Result r;
    std::ostringstream cap; auto* old = std::cerr.rdbuf(cap.rdbuf());
    r.rc = Parser::parse("/nonexistent/dir/nope.net", r.nodes, r.pi, r.po);
    std::cerr.rdbuf(old);
    CHECK(r.rc == -1);
    CHECK(cap.str().find("cannot open file") != std::string::npos);
}

TEST(err_wrong_column_count_too_few) {
    auto r = run("cols_few", "test\ta\t-\t-\tpi\n");
    CHECK(r.rc == -1);
    CHECK(errHas(r, "expected 6 columns"));
}

TEST(err_wrong_column_count_too_many) {
    auto r = run("cols_many", "test\ta\t-\t-\tpi\tw\textra\n");
    CHECK(r.rc == -1);
    CHECK(errHas(r, "expected 6 columns"));
}

TEST(err_multiple_top_modules) {
    std::string nl =
        row("modA", "a", "-", "-", "pi", "w") +
        row("modB", "b", "-", "-", "pi", "w2");
    auto r = run("multi_mod", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "multiple top-level modules"));
}

TEST(err_duplicate_primary_component) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "w1") +
        row("t", "a", "-", "-", "pi", "w2");
    auto r = run("dup_primary", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "duplicate primary component"));
}

TEST(err_unknown_primary_direction) {
    auto r = run("bad_pdir", row("t", "a", "-", "-", "inout", "w"));
    CHECK(r.rc == -1);
    CHECK(errHas(r, "unknown primary port direction"));
}

TEST(err_unknown_component_type) {
    auto r = run("bad_type", row("t", "g", "$foo", "A", "in", "w"));
    CHECK(r.rc == -1);
    CHECK(errHas(r, "unknown component type"));
}

TEST(err_unsupported_ff_types_currently_commented_out) {
    // $dff_n / $tff_* / $jkff_* are commented out in compTypeTable.
    // Delete/adjust this test when you enable them.
    auto r = run("dff_n", row("t", "ff", "$dff_n", "D", "in", "w"));
    CHECK(r.rc == -1);
    CHECK(errHas(r, "unknown component type"));
}

TEST(err_unknown_port_name) {
    auto r = run("bad_port", row("t", "g", "$and", "Z", "in", "w"));
    CHECK(r.rc == -1);
    CHECK(errHas(r, "unknown port name"));
}

TEST(err_port_not_valid_for_gate_type) {
    // $not has no B port
    auto r = run("port_not_on_gate", row("t", "g", "$not", "B", "in", "w"));
    CHECK(r.rc == -1);
    CHECK(errHas(r, "does not exist on"));
}

TEST(err_component_redefined_with_different_type) {
    std::string nl =
        row("t", "g", "$and", "A", "in", "w1") +
        row("t", "g", "$or",  "B", "in", "w2");
    auto r = run("retype", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "redefined with different type"));
}

TEST(err_port_redefined_on_component) {
    std::string nl =
        row("t", "g", "$and", "A", "in", "w1") +
        row("t", "g", "$and", "A", "in", "w2");
    auto r = run("port_redef", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "redefined on component"));
}

TEST(err_unknown_gate_direction) {
    auto r = run("bad_dir", row("t", "g", "$and", "A", "sideways", "w"));
    CHECK(r.rc == -1);
    CHECK(errHas(r, "unknown direction"));
}

TEST(err_multiple_drivers_two_primary_inputs) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "w") +
        row("t", "b", "-", "-", "pi", "w");
    auto r = run("multi_drv_pi", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "multiple drivers"));
}

TEST(err_multiple_drivers_two_gate_outputs) {
    std::string nl =
        row("t", "g1", "$not", "Y", "out", "w") +
        row("t", "g2", "$not", "Y", "out", "w");
    auto r = run("multi_drv_gate", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "multiple drivers"));
}

TEST(err_multiple_drivers_pi_and_gate_output) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "w") +
        row("t", "g", "$not", "Y", "out", "w");
    auto r = run("multi_drv_mixed", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "multiple drivers"));
}

TEST(err_missing_port_on_instance) {
    // $and with only A and Y -> B missing
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "y", "-", "-", "po", "wy") +
        row("t", "g", "$and", "A", "in", "wa") +
        row("t", "g", "$and", "Y", "out", "wy");
    auto r = run("missing_port", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "missing on instance"));
}

TEST(err_wire_has_no_driver) {
    // primary output fed by a wire nobody drives
    std::string nl = row("t", "y", "-", "-", "po", "floating");
    auto r = run("no_driver", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "has no driver"));
}

TEST(err_wire_has_no_consumers) {
    // primary input wire that goes nowhere
    std::string nl = row("t", "a", "-", "-", "pi", "dangling");
    auto r = run("no_consumer", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "has no consumers"));
}

TEST(err_gate_output_unconnected) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "g", "$not", "A", "in", "wa") +
        row("t", "g", "$not", "Y", "out", "unused");
    auto r = run("unused_out", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "has no consumers"));
}

TEST(empty_file_current_behaviour) {
    // Documents what the parser does today with an empty netlist.
    // If you decide an empty netlist should be an error, change rc to -1.
    auto r = run("empty", "");
    CHECK(r.rc == 0);
    CHECK(r.nodes.empty());
    CHECK(r.pi == 0 && r.po == 0);
}

// Netlist syntax with the $fsm block (from your netlist description):
//
//   $fsm start
//   .s S1
//   .f S2
//   $fsm end
//
// Parser::parse() does not handle this yet: those lines have 2-3 tokens, so the
// "expected 6 columns" check rejects the whole file. This test pins the CURRENT
// behaviour. When you add FSM-block support, flip it to expect rc == 0.
TEST(fsm_block_currently_rejected) {
    std::string nl = AND_NETLIST + "\n$fsm start\n.s S1\n.f S2\n\n$fsm end\n";
    auto r = run("fsm_block", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "expected 6 columns"));
}

// PENDING: cycle detection / topological sort is not implemented yet.
// Build with -DRUN_PENDING_TESTS once it is.
#ifdef RUN_PENDING_TESTS
TEST(pending_combinational_loop_rejected) {
    // not1 -> not2 -> not1 with no flip-flop in the loop
    std::string nl =
        row("t", "y", "-", "-", "po", "w1") +
        row("t", "n1", "$not", "A", "in", "w2") +
        row("t", "n1", "$not", "Y", "out", "w1") +
        row("t", "n2", "$not", "A", "in", "w1") +
        row("t", "n2", "$not", "Y", "out", "w2");
    auto r = run("comb_loop", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "cycle"));
}

TEST(pending_loop_through_dff_allowed) {
    // Q -> NOT -> D : legal sequential feedback (toggle)
    std::string nl =
        row("t", "clk", "-", "-", "pi", "wc") +
        row("t", "q",   "-", "-", "po", "wq2") +
        row("t", "ff", "$dff_p", "D",   "in",  "wd") +
        row("t", "ff", "$dff_p", "CLK", "in",  "wc") +
        row("t", "ff", "$dff_p", "Q",   "out", "wq") +
        row("t", "n",  "$not", "A", "in", "wq") +
        row("t", "n",  "$not", "Y", "out", "wd");
    // NOTE: wq also needs to reach the output; add a buffer/consumer as needed
    auto r = run("dff_loop", nl);
    CHECK(r.rc == 0);
}
#endif

int main() {
    for (auto& t : registry()) {
        int before = g_failed;
        t.fn();
        std::cout << (g_failed == before ? "[ PASS ] " : "[ FAIL ] ") << t.name << "\n";
    }
    std::cout << "\n" << registry().size() << " tests, " << g_checks << " checks, "
              << g_failed << " failed\n";
    return g_failed ? 1 : 0;
}
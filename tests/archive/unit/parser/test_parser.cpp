// tests/unit/parser/test_parser.cpp
//
// Tests for Parser::parse(): netlist validation + graph creation.
// Self-contained (no gtest). Each test writes a netlist to a temp file.
//
// Netlist format covered (6 columns, whitespace separated):
//   module  component  type  port  dir  wire
// Gate types use the "$_AND_" style names, the clock port is "C", wires may be
// multibit ("a [3:0]" / "a [2]"), and inputs may be constants (1'0, 1'1, 1'x).

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <utility>

#include "parser/Parser.hpp"
#include "core/Node.hpp"
#include "core/Port.hpp"
#include "core/Signal.hpp"
#include "combinational/AND.hpp"
#include "combinational/NOT.hpp"
#include "combinational/MUX.hpp"
#include "combinational/OR.hpp"
#include "sequential/DFF_P.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"

// Friend of Node and List (see core/Node.hpp, core/List.hpp): gives the tests
// read-only access to a node's outgoing connections.
struct ParserTestAccess {
    static std::vector<std::pair<const Node*, Port>> connections(const Node& n) {
        std::vector<std::pair<const Node*, Port>> out;
        for (auto* c = n.connections.head; c; c = c->next)
            out.push_back({&c->target, c->p});
        return out;
    }
};

static std::vector<std::pair<const Node*, Port>> connectionsOf(const Node& n) {
    return ParserTestAccess::connections(n);
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
using NodeMap = std::unordered_map<std::string, std::vector<Node*>>;

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
    NodeMap pi, po;           // primary inputs / outputs by netlist name
    std::string err;          // everything the parser wrote to std::cerr
};

// Runs the parser and captures std::cerr so messages can be asserted.
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

// Primary node vector for a name (empty if the name is absent)
static std::vector<Node*> vec(const NodeMap& m, const std::string& name) {
    auto it = m.find(name);
    return it == m.end() ? std::vector<Node*>{} : it->second;
}

static bool drives(const Node& from, const Node* to, Port p) {
    for (auto& [n, port] : connectionsOf(from)) if (n == to && port == p) return true;
    return false;
}

// Reusable netlist fragment: y = a & b
static const std::string AND_NETLIST =
    row("test", "a", "-", "-", "pi", "\\a") +
    row("test", "b", "-", "-", "pi", "\\b") +
    row("test", "y", "-", "-", "po", "$and$test.v:2$1_Y") +
    row("test", "$and$test.v:2$1", "$_AND_", "A", "in",  "\\a") +
    row("test", "$and$test.v:2$1", "$_AND_", "B", "in",  "\\b") +
    row("test", "$and$test.v:2$1", "$_AND_", "Y", "out", "$and$test.v:2$1_Y");

// ===========================================================================
// POSITIVE: parsing + graph creation
// ===========================================================================

TEST(and_gate_counts_and_node_types) {
    auto r = run("and_ok", AND_NETLIST);
    CHECK(r.rc == 0);
    CHECK(r.nodes.size() == 4);
    CHECK(r.pi.size() == 2);
    CHECK(r.po.size() == 1);
    CHECK(vec(r.pi, "a").size() == 1);
    CHECK(vec(r.pi, "b").size() == 1);
    CHECK(vec(r.po, "y").size() == 1);
    CHECK(nodesOfType<InputNode>(r).size() == 2);
    CHECK(nodesOfType<OutputNode>(r).size() == 1);
    CHECK(nodesOfType<AND>(r).size() == 1);
    CHECK(r.err.empty());
}

TEST(primary_maps_hold_right_node_types) {
    auto r = run("pi_po_types", AND_NETLIST);
    CHECK(r.rc == 0);
    auto a = vec(r.pi, "a"), y = vec(r.po, "y");
    if (a.size() != 1 || y.size() != 1) { CHECK(false); return; }
    CHECK(dynamic_cast<InputNode*>(a[0]) != nullptr);
    CHECK(dynamic_cast<OutputNode*>(y[0]) != nullptr);
    // input names are not in the output map and vice versa
    CHECK(r.po.count("a") == 0);
    CHECK(r.pi.count("y") == 0);
}

TEST(and_gate_graph_connections) {
    auto r = run("and_graph", AND_NETLIST);
    CHECK(r.rc == 0);
    auto a = vec(r.pi, "a"), b = vec(r.pi, "b"), y = vec(r.po, "y");
    auto ands = nodesOfType<AND>(r);
    if (a.size() != 1 || b.size() != 1 || y.size() != 1 || ands.size() != 1) { CHECK(false); return; }

    CHECK(drives(*a[0], ands[0], Port::A));
    CHECK(drives(*b[0], ands[0], Port::B));
    CHECK(connectionsOf(*a[0]).size() == 1);
    CHECK(connectionsOf(*b[0]).size() == 1);

    // gate output port "Y" is Port::O on the consumer side: AND -> output node
    CHECK(drives(*ands[0], y[0], Port::O));
    CHECK(connectionsOf(*ands[0]).size() == 1);
    CHECK(connectionsOf(*y[0]).empty());
}

TEST(comments_and_blank_lines_ignored) {
    std::string nl =
        "# full-line comment\n"
        "\n"
        "   \t  \n" +
        row("test", "a", "-", "-", "pi", "w1") +
        "test\ty\t-\t-\tpo\tw2   # trailing comment\n" +
        row("test", "g", "$_NOT_", "A", "in", "w1") +
        row("test", "g", "$_NOT_", "Y", "out", "w2");
    auto r = run("comments", nl);
    CHECK(r.rc == 0);
    CHECK(r.nodes.size() == 3);
    CHECK(r.pi.size() == 1 && r.po.size() == 1);
}

TEST(extra_whitespace_between_columns_ok) {
    std::string nl =
        "test    a   -   -   pi   w1\n"
        "test    y   -   -   po   w2\n"
        "test    g   $_NOT_   A   in    w1\n"
        "test    g   $_NOT_   Y   out   w2\n";
    auto r = run("spaces", nl);
    CHECK(r.rc == 0);
}

TEST(not_gate_single_input) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "w1") +
        row("t", "y", "-", "-", "po", "w2") +
        row("t", "n", "$_NOT_", "A", "in", "w1") +
        row("t", "n", "$_NOT_", "Y", "out", "w2");
    auto r = run("not_ok", nl);
    CHECK(r.rc == 0);
    auto nots = nodesOfType<NOT>(r);
    auto a = vec(r.pi, "a");
    if (nots.size() == 1 && a.size() == 1) CHECK(drives(*a[0], nots[0], Port::A));
    else CHECK(false);
}

TEST(mux_three_inputs_ports_mapped) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "b", "-", "-", "pi", "wb") +
        row("t", "s", "-", "-", "pi", "ws") +
        row("t", "y", "-", "-", "po", "wy") +
        row("t", "m", "$_MUX_", "A", "in", "wa") +
        row("t", "m", "$_MUX_", "B", "in", "wb") +
        row("t", "m", "$_MUX_", "S", "in", "ws") +
        row("t", "m", "$_MUX_", "Y", "out", "wy");
    auto r = run("mux_ok", nl);
    CHECK(r.rc == 0);
    auto mux = nodesOfType<MUX>(r);
    auto a = vec(r.pi, "a"), b = vec(r.pi, "b"), s = vec(r.pi, "s");
    if (mux.size() == 1 && a.size() == 1 && b.size() == 1 && s.size() == 1) {
        CHECK(drives(*a[0], mux[0], Port::A));
        CHECK(drives(*b[0], mux[0], Port::B));
        CHECK(drives(*s[0], mux[0], Port::S));
    } else CHECK(false);
}

TEST(dff_p_ports_D_C_Q) {
    // netlist clock port is "C"; it is mapped to Port::CLK
    std::string nl =
        row("t", "d",   "-", "-", "pi", "wd") +
        row("t", "clk", "-", "-", "pi", "wc") +
        row("t", "q",   "-", "-", "po", "wq") +
        row("t", "ff", "$_DFF_P_", "D", "in",  "wd") +
        row("t", "ff", "$_DFF_P_", "C", "in",  "wc") +
        row("t", "ff", "$_DFF_P_", "Q", "out", "wq");
    auto r = run("dff_ok", nl);
    CHECK(r.rc == 0);
    auto ffs = nodesOfType<DFF_P>(r);
    auto d = vec(r.pi, "d"), clk = vec(r.pi, "clk"), q = vec(r.po, "q");
    if (ffs.size() == 1 && d.size() == 1 && clk.size() == 1 && q.size() == 1) {
        CHECK(drives(*d[0], ffs[0], Port::D));
        CHECK(drives(*clk[0], ffs[0], Port::CLK));
        CHECK(drives(*ffs[0], q[0], Port::O));
    } else CHECK(false);
}

TEST(fanout_one_wire_two_consumers) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "b", "-", "-", "pi", "wb") +
        row("t", "y1", "-", "-", "po", "w1") +
        row("t", "y2", "-", "-", "po", "w2") +
        row("t", "n", "$_NOT_", "A", "in", "wa") +
        row("t", "n", "$_NOT_", "Y", "out", "w1") +
        row("t", "o", "$_OR_", "A", "in", "wa") +
        row("t", "o", "$_OR_", "B", "in", "wb") +
        row("t", "o", "$_OR_", "Y", "out", "w2");
    auto r = run("fanout", nl);
    CHECK(r.rc == 0);
    auto a = vec(r.pi, "a"), b = vec(r.pi, "b");
    auto nots = nodesOfType<NOT>(r);
    auto ors = nodesOfType<OR>(r);
    if (a.size() == 1 && b.size() == 1 && nots.size() == 1 && ors.size() == 1) {
        CHECK(connectionsOf(*a[0]).size() == 2);
        CHECK(drives(*a[0], nots[0], Port::A));
        CHECK(drives(*a[0], ors[0], Port::A));
        CHECK(connectionsOf(*b[0]).size() == 1);
        CHECK(drives(*b[0], ors[0], Port::B));
    } else CHECK(false);
}

TEST(chained_gates_and_then_not) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "b", "-", "-", "pi", "wb") +
        row("t", "y", "-", "-", "po", "wy") +
        row("t", "g1", "$_AND_", "A", "in", "wa") +
        row("t", "g1", "$_AND_", "B", "in", "wb") +
        row("t", "g1", "$_AND_", "Y", "out", "wmid") +
        row("t", "g2", "$_NOT_", "A", "in", "wmid") +
        row("t", "g2", "$_NOT_", "Y", "out", "wy");
    auto r = run("chain", nl);
    CHECK(r.rc == 0);
    auto ands = nodesOfType<AND>(r);
    auto nots = nodesOfType<NOT>(r);
    auto y = vec(r.po, "y");
    if (ands.size() == 1 && nots.size() == 1 && y.size() == 1) {
        CHECK(drives(*ands[0], nots[0], Port::A));
        CHECK(drives(*nots[0], y[0], Port::O));
    } else CHECK(false);
}

TEST(port_rows_in_any_order_same_gate) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "b", "-", "-", "pi", "wb") +
        row("t", "y", "-", "-", "po", "wy") +
        row("t", "g", "$_AND_", "Y", "out", "wy") +
        row("t", "g", "$_AND_", "B", "in", "wb") +
        row("t", "g", "$_AND_", "A", "in", "wa");
    auto r = run("order", nl);
    CHECK(r.rc == 0);
    CHECK(nodesOfType<AND>(r).size() == 1);
}

TEST(two_gate_instances_chained) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "y", "-", "-", "po", "wy") +
        row("t", "n1", "$_NOT_", "A", "in", "wa") +
        row("t", "n1", "$_NOT_", "Y", "out", "wm") +
        row("t", "n2", "$_NOT_", "A", "in", "wm") +
        row("t", "n2", "$_NOT_", "Y", "out", "wy");
    auto r = run("two_nots", nl);
    CHECK(r.rc == 0);
    auto nots = nodesOfType<NOT>(r);
    CHECK(nots.size() == 2);
    if (nots.size() == 2) CHECK(drives(*nots[0], nots[1], Port::A));
}

// ---------------------------------------------------------------------------
// Warnings (not errors): the parser now only warns about unused wires
// ---------------------------------------------------------------------------

TEST(warn_primary_input_with_no_consumers) {
    auto r = run("pi_unused", row("t", "a", "-", "-", "pi", "dangling"));
    CHECK(r.rc == 0);
    CHECK(errHas(r, "warning"));
    CHECK(errHas(r, "has no consumers"));
    CHECK(vec(r.pi, "a").size() == 1);
}

TEST(warn_gate_output_unconnected) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "g", "$_NOT_", "A", "in", "wa") +
        row("t", "g", "$_NOT_", "Y", "out", "unused");
    auto r = run("unused_out", nl);
    CHECK(r.rc == 0);
    CHECK(errHas(r, "warning"));
    auto nots = nodesOfType<NOT>(r);
    if (nots.size() == 1) CHECK(connectionsOf(*nots[0]).empty());
    else CHECK(false);
}

// ---------------------------------------------------------------------------
// Multibit wires
// ---------------------------------------------------------------------------

// n_i = NOT(a [i]) -> y [i], for i in 0..width-1
static std::string notBank(int width, const std::string& piWire, const std::string& poWire) {
    std::string nl = row("t", "a", "-", "-", "pi", piWire) +
                     row("t", "y", "-", "-", "po", poWire);
    for (int i = 0; i < width; i++) {
        std::string g = "n" + std::to_string(i);
        nl += row("t", g, "$_NOT_", "A", "in",  "a [" + std::to_string(i) + "]");
        nl += row("t", g, "$_NOT_", "Y", "out", "y [" + std::to_string(i) + "]");
    }
    return nl;
}

static void checkBank(const Result& r, int width) {
    auto a = vec(r.pi, "a"), y = vec(r.po, "y");
    auto nots = nodesOfType<NOT>(r);
    CHECK((int)a.size() == width);
    CHECK((int)y.size() == width);
    CHECK((int)nots.size() == width);
    if ((int)a.size() != width || (int)y.size() != width || (int)nots.size() != width) return;
    // Bits are stored in ascending index order: element i is bit i.
    // Gate i (created in file order) is wired to bit i.
    for (int i = 0; i < width; i++) {
        CHECK(drives(*a[i], nots[i], Port::A));
        CHECK(connectionsOf(*a[i]).size() == 1);
        CHECK(drives(*nots[i], y[i], Port::O));
    }
}

TEST(multibit_range_input_and_output) {
    auto r = run("range4", notBank(4, "a [3:0]", "y [3:0]"));
    CHECK(r.rc == 0);
    CHECK(r.pi.size() == 1 && r.po.size() == 1);   // one entry per name, not per bit
    checkBank(r, 4);
}

TEST(multibit_inferred_from_indexed_wires) {
    // port wire is just "a"/"y"; width inferred from "a [i]" / "y [i]" wires
    auto r = run("inferred4", notBank(4, "a", "y"));
    CHECK(r.rc == 0);
    checkBank(r, 4);
}

TEST(multibit_two_bit_range) {
    auto r = run("range2", notBank(2, "a [1:0]", "y [1:0]"));
    CHECK(r.rc == 0);
    checkBank(r, 2);
}

TEST(multibit_node_types) {
    auto r = run("range_types", notBank(3, "a [2:0]", "y [2:0]"));
    CHECK(r.rc == 0);
    for (auto* n : vec(r.pi, "a")) CHECK(dynamic_cast<InputNode*>(n) != nullptr);
    for (auto* n : vec(r.po, "y")) CHECK(dynamic_cast<OutputNode*>(n) != nullptr);
}

TEST(multibit_single_bit_and_bus_together) {
    // 1-bit "en" next to a 2-bit bus "a"
    std::string nl =
        row("t", "en", "-", "-", "pi", "en") +
        row("t", "a",  "-", "-", "pi", "a [1:0]") +
        row("t", "y",  "-", "-", "po", "y [1:0]") +
        row("t", "g0", "$_AND_", "A", "in", "a [0]") +
        row("t", "g0", "$_AND_", "B", "in", "en") +
        row("t", "g0", "$_AND_", "Y", "out", "y [0]") +
        row("t", "g1", "$_AND_", "A", "in", "a [1]") +
        row("t", "g1", "$_AND_", "B", "in", "en") +
        row("t", "g1", "$_AND_", "Y", "out", "y [1]");
    auto r = run("bus_plus_bit", nl);
    CHECK(r.rc == 0);
    CHECK(vec(r.pi, "en").size() == 1);
    CHECK(vec(r.pi, "a").size() == 2);
    CHECK(vec(r.po, "y").size() == 2);
    auto en = vec(r.pi, "en");
    if (!en.empty()) CHECK(connectionsOf(*en[0]).size() == 2);   // enable fans out to both ANDs
}

TEST(multibit_range_missing_wire_is_error) {
    // a [3:0] declared but only bits 0 and 1 are ever used
    std::string nl =
        row("t", "a", "-", "-", "pi", "a [3:0]") +
        row("t", "y", "-", "-", "po", "y [1:0]") +
        row("t", "n0", "$_NOT_", "A", "in", "a [0]") +
        row("t", "n0", "$_NOT_", "Y", "out", "y [0]") +
        row("t", "n1", "$_NOT_", "A", "in", "a [1]") +
        row("t", "n1", "$_NOT_", "Y", "out", "y [1]");
    auto r = run("range_missing", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "expected wire"));
}

// ---------------------------------------------------------------------------
// Constant inputs: 1'0, 1'1, 1'x
// ---------------------------------------------------------------------------

// y = <constWire> & a
static std::string constAnd(const std::string& constWire) {
    return row("t", "a", "-", "-", "pi", "wa") +
           row("t", "y", "-", "-", "po", "wy") +
           row("t", "g", "$_AND_", "A", "in", constWire) +
           row("t", "g", "$_AND_", "B", "in", "wa") +
           row("t", "g", "$_AND_", "Y", "out", "wy");
}

// the InputNode that is not a registered primary input
static Node* constantNode(const Result& r) {
    for (auto* in : nodesOfType<InputNode>(r)) {
        bool primary = false;
        for (auto& [name, v] : r.pi) for (auto* n : v) if (n == in) primary = true;
        if (!primary) return in;
    }
    return nullptr;
}

TEST(constant_one_creates_driver_node_with_high) {
    auto r = run("const1", constAnd("1'1"));
    CHECK(r.rc == 0);
    CHECK(r.nodes.size() == 4);                         // and + const + a + y
    CHECK(r.pi.size() == 1);                            // constant is not a named primary input
    Node* c = constantNode(r);
    auto ands = nodesOfType<AND>(r);
    if (c && ands.size() == 1) {
        CHECK(c->getResult() == Signal::HIGH);
        CHECK(drives(*c, ands[0], Port::A));
    } else CHECK(false);
}

TEST(constant_zero_is_low) {
    auto r = run("const0", constAnd("1'0"));
    CHECK(r.rc == 0);
    Node* c = constantNode(r);
    auto ands = nodesOfType<AND>(r);
    if (c && ands.size() == 1) {
        CHECK(c->getResult() == Signal::LOW);
        CHECK(drives(*c, ands[0], Port::A));
    } else CHECK(false);
}

TEST(constant_x_is_treated_as_low) {
    auto r = run("constx", constAnd("1'x"));
    CHECK(r.rc == 0);
    Node* c = constantNode(r);
    if (c) CHECK(c->getResult() == Signal::LOW);
    else CHECK(false);
}

TEST(constant_shared_between_two_gates_is_one_node) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "y1", "-", "-", "po", "w1") +
        row("t", "y2", "-", "-", "po", "w2") +
        row("t", "g1", "$_AND_", "A", "in", "1'1") +
        row("t", "g1", "$_AND_", "B", "in", "wa") +
        row("t", "g1", "$_AND_", "Y", "out", "w1") +
        row("t", "g2", "$_OR_", "A", "in", "1'1") +
        row("t", "g2", "$_OR_", "B", "in", "wa") +
        row("t", "g2", "$_OR_", "Y", "out", "w2");
    auto r = run("const_shared", nl);
    CHECK(r.rc == 0);
    CHECK(nodesOfType<InputNode>(r).size() == 2);       // 'a' + one shared constant
    Node* c = constantNode(r);
    if (c) CHECK(connectionsOf(*c).size() == 2);
    else CHECK(false);
}

TEST(constant_one_and_zero_are_separate_nodes) {
    std::string nl =
        row("t", "y", "-", "-", "po", "wy") +
        row("t", "g", "$_AND_", "A", "in", "1'1") +
        row("t", "g", "$_AND_", "B", "in", "1'0") +
        row("t", "g", "$_AND_", "Y", "out", "wy");
    auto r = run("const_both", nl);
    CHECK(r.rc == 0);
    CHECK(nodesOfType<InputNode>(r).size() == 2);
    int high = 0, low = 0;
    for (auto* in : nodesOfType<InputNode>(r)) (in->getResult() == Signal::HIGH ? high : low)++;
    CHECK(high == 1 && low == 1);
}

// ===========================================================================
// NEGATIVE: every error path in Parser::parse()
// ===========================================================================

TEST(err_file_not_found) {
    Result r;
    std::ostringstream cap; auto* old = std::cerr.rdbuf(cap.rdbuf());
    r.rc = Parser::parse("/nonexistent/dir/nope.net", r.nodes, r.pi, r.po);
    std::cerr.rdbuf(old);
    CHECK(r.rc == -1);
    CHECK(cap.str().find("cannot open file") != std::string::npos);
}

TEST(err_too_few_columns) {
    auto r = run("cols_few", "test\ta\t-\t-\tpi\n");
    CHECK(r.rc == -1);
    CHECK(errHas(r, "expected 6 columns"));
}

TEST(extra_columns_are_joined_into_wire_name) {
    // >6 tokens: tokens 6.. are merged into the wire name ("w extra").
    // As a primary output it has no driver, so parsing fails for that reason,
    // not because of the column count.
    auto r = run("cols_many", "test\ty\t-\t-\tpo\tw extra\n");
    CHECK(r.rc == -1);
    CHECK(!errHas(r, "expected 6 columns"));
    CHECK(errHas(r, "w extra"));
    CHECK(errHas(r, "has no driver"));
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

TEST(err_gate_name_clashes_with_primary_name) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "w1") +
        row("t", "a", "$_NOT_", "A", "in", "w1");
    auto r = run("gate_vs_primary", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "redefined with different type"));
}

TEST(err_unknown_component_type) {
    auto r = run("bad_type", row("t", "g", "$_FOO_", "A", "in", "w"));
    CHECK(r.rc == -1);
    CHECK(errHas(r, "unknown component type"));
}

TEST(err_legacy_gate_names_rejected) {
    // old-style names ($and, $not, ...) are no longer accepted
    auto r = run("legacy_type", row("t", "g", "$and", "A", "in", "w"));
    CHECK(r.rc == -1);
    CHECK(errHas(r, "unknown component type"));
}

TEST(err_unsupported_ff_types_currently_commented_out) {
    // $_DFF_N_ / $_TFF_*_ / $_JKFF_*_ are commented out in compTypeTable.
    // Delete/adjust these checks when you enable them.
    const char* types[] = {"$_DFF_N_", "$_TFF_P_", "$_TFF_N_", "$_JKFF_P_", "$_JKFF_N_"};
    for (auto* t : types) {
        auto r = run("ff_unsup", row("t", "ff", t, "D", "in", "w"));
        CHECK(r.rc == -1);
        CHECK(errHas(r, "unknown component type"));
    }
}

TEST(err_unknown_port_name) {
    auto r = run("bad_port", row("t", "g", "$_AND_", "Z", "in", "w"));
    CHECK(r.rc == -1);
    CHECK(errHas(r, "unknown port name"));
}

TEST(err_legacy_clock_port_name_rejected) {
    // the clock port is now "C"; "CLK" is not a valid netlist port name
    auto r = run("legacy_clk", row("t", "ff", "$_DFF_P_", "CLK", "in", "w"));
    CHECK(r.rc == -1);
    CHECK(errHas(r, "unknown port name"));
}

TEST(err_port_not_valid_for_gate_type) {
    // $_NOT_ has no B port
    auto r = run("port_not_on_gate", row("t", "g", "$_NOT_", "B", "in", "w"));
    CHECK(r.rc == -1);
    CHECK(errHas(r, "does not exist on"));
}

TEST(err_component_redefined_with_different_type) {
    std::string nl =
        row("t", "g", "$_AND_", "A", "in", "w1") +
        row("t", "g", "$_OR_",  "B", "in", "w2");
    auto r = run("retype", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "redefined with different type"));
}

TEST(err_port_redefined_on_component) {
    std::string nl =
        row("t", "g", "$_AND_", "A", "in", "w1") +
        row("t", "g", "$_AND_", "A", "in", "w2");
    auto r = run("port_redef", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "redefined on component"));
}

TEST(err_unknown_gate_direction) {
    auto r = run("bad_dir", row("t", "g", "$_AND_", "A", "sideways", "w"));
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
        row("t", "g1", "$_NOT_", "Y", "out", "w") +
        row("t", "g2", "$_NOT_", "Y", "out", "w");
    auto r = run("multi_drv_gate", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "multiple drivers"));
}

TEST(err_multiple_drivers_pi_and_gate_output) {
    std::string nl =
        row("t", "a", "-", "-", "pi", "w") +
        row("t", "g", "$_NOT_", "Y", "out", "w");
    auto r = run("multi_drv_mixed", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "multiple drivers"));
}

TEST(err_multiple_drivers_on_a_multibit_wire) {
    // bit "a [0]" is driven by the bus input and also by a gate output
    std::string nl =
        row("t", "a", "-", "-", "pi", "a [1:0]") +
        row("t", "g", "$_NOT_", "A", "in", "a [1]") +
        row("t", "g", "$_NOT_", "Y", "out", "a [0]") +
        row("t", "h", "$_NOT_", "A", "in", "a [0]") +
        row("t", "h", "$_NOT_", "Y", "out", "z");
    auto r = run("multi_drv_bit", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "multiple drivers"));
}

TEST(err_missing_port_on_instance) {
    // $_AND_ with only A and Y -> B missing
    std::string nl =
        row("t", "a", "-", "-", "pi", "wa") +
        row("t", "y", "-", "-", "po", "wy") +
        row("t", "g", "$_AND_", "A", "in", "wa") +
        row("t", "g", "$_AND_", "Y", "out", "wy");
    auto r = run("missing_port", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "missing on instance"));
}

TEST(err_dff_missing_clock) {
    std::string nl =
        row("t", "d", "-", "-", "pi", "wd") +
        row("t", "q", "-", "-", "po", "wq") +
        row("t", "ff", "$_DFF_P_", "D", "in",  "wd") +
        row("t", "ff", "$_DFF_P_", "Q", "out", "wq");
    auto r = run("dff_no_clk", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "port C missing"));
}

TEST(err_wire_has_no_driver) {
    auto r = run("no_driver", row("t", "y", "-", "-", "po", "floating"));
    CHECK(r.rc == -1);
    CHECK(errHas(r, "has no driver"));
}

TEST(err_gate_input_wire_has_no_driver) {
    std::string nl =
        row("t", "y", "-", "-", "po", "wy") +
        row("t", "g", "$_NOT_", "A", "in", "nobody_drives_me") +
        row("t", "g", "$_NOT_", "Y", "out", "wy");
    auto r = run("input_no_driver", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "nobody_drives_me"));
    CHECK(errHas(r, "has no driver"));
}

// ---------------------------------------------------------------------------
// Current behaviours worth knowing about (pin them so changes are deliberate)
// ---------------------------------------------------------------------------

TEST(unknown_primary_direction_is_currently_accepted_as_output) {
    // The old "unknown primary port direction" error is gone: any direction other
    // than "pi" is treated as an output. If you want it rejected again, change
    // rc to -1 here and add the check back in Parser.cpp.
    std::string nl =
        row("t", "a", "-", "-", "pi", "w") +
        row("t", "y", "-", "-", "inout", "w");
    auto r = run("bad_pdir", nl);
    CHECK(r.rc == 0);
    CHECK(vec(r.po, "y").size() == 1);
}

TEST(empty_file_current_behaviour) {
    auto r = run("empty", "");
    CHECK(r.rc == 0);
    CHECK(r.nodes.empty());
    CHECK(r.pi.empty() && r.po.empty());
}

// Netlist syntax with the $fsm block:
//
//   $fsm start
//   .s S1
//   .f S2
//   $fsm end
//
// Parser::parse() does not handle this yet: those lines have 2-3 tokens, so the
// "expected 6 columns" check rejects the whole file. Pins the CURRENT behaviour;
// flip to rc == 0 once FSM blocks are supported.
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
    // n1 -> n2 -> n1 with no flip-flop in the loop
    std::string nl =
        row("t", "y", "-", "-", "po", "w1") +
        row("t", "n1", "$_NOT_", "A", "in", "w2") +
        row("t", "n1", "$_NOT_", "Y", "out", "w1") +
        row("t", "n2", "$_NOT_", "A", "in", "w1") +
        row("t", "n2", "$_NOT_", "Y", "out", "w2");
    auto r = run("comb_loop", nl);
    CHECK(r.rc == -1);
    CHECK(errHas(r, "cycle"));
}

TEST(pending_loop_through_dff_allowed) {
    // Q -> NOT -> D : legal sequential feedback (toggle flip-flop)
    std::string nl =
        row("t", "clk", "-", "-", "pi", "wc") +
        row("t", "q",   "-", "-", "po", "wq") +
        row("t", "ff", "$_DFF_P_", "D", "in",  "wd") +
        row("t", "ff", "$_DFF_P_", "C", "in",  "wc") +
        row("t", "ff", "$_DFF_P_", "Q", "out", "wq") +
        row("t", "n",  "$_NOT_", "A", "in", "wq") +
        row("t", "n",  "$_NOT_", "Y", "out", "wd");
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

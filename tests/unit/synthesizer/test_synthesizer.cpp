// Synthesizer::synthesize(): FSM table -> netlist. Behaviour of the generated
// circuits is checked in tests/integration/test_fsm_flow.cpp; this file covers
// the output format and every error path.

#include <set>

#include "support/helpers.hpp"

#include "parser/Parser.hpp"
#include "synthesizer/Synthesizer.hpp"

using namespace hdltest;

namespace {

const char *TRAFFIC =
    "inputs:  en\n"
    "outputs: R Y G\n"
    "reset:   Sg\n"
    "\n"
    "# current | en | next | R Y G\n"
    "Sg | 0 | Sg | 0 0 1\n"
    "Sg | 1 | Sy | 0 0 1\n"
    "Sy | 0 | Sy | 0 1 0\n"
    "Sy | 1 | Sr | 0 1 0\n"
    "Sr | 0 | Sr | 1 0 0\n"
    "Sr | 1 | Sg | 1 0 0\n";

struct Synth {
    std::string table;   // returned path ("" on error)
    std::string err;     // std::cerr output
};

Synth synth(const TempDir &dir, const std::string &content, const std::string &name = "fsm.net") {
    std::string path = dir.write(name, content);
    Capture cap(std::cerr);
    Synth s;
    s.table = Synthesizer::synthesize(path);
    s.err = cap.str();
    return s;
}

// Expects synthesis to fail with `message` and to leave no .table behind
void expectError(const std::string &content, const std::string &message) {
    TempDir dir;
    auto s = synth(dir, content);
    CHECK_EQ(s.table, "");
    CHECK_CONTAINS(s.err, message);
    CHECK(!fileExists(dir.path("fsm.table")));
}

// Netlist rows without comments: {cell, type, port, dir, wire}
std::vector<std::vector<std::string>> rows(const std::string &netlist) {
    std::vector<std::vector<std::string>> out;
    std::istringstream in(netlist);
    for (std::string line; std::getline(in, line);) {
        line = line.substr(0, line.find('#'));
        std::istringstream ls(line);
        std::vector<std::string> t;
        for (std::string tok; ls >> tok;) t.push_back(tok);
        if (!t.empty()) out.push_back(t);
    }
    return out;
}

std::string withoutComments(const std::string &netlist) {
    std::string out;
    for (auto &r : rows(netlist)) {
        for (auto &t : r) out += t + " ";
        out += "\n";
    }
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// output
// ---------------------------------------------------------------------------

TEST(synthesizer, writes_table_next_to_input_and_returns_its_path) {
    TempDir dir;
    auto s = synth(dir, TRAFFIC, "traffic.net");
    CHECK_EQ(s.table, dir.path("traffic.table"));
    CHECK(fileExists(s.table));
    CHECK_EQ(s.err, "");
}

TEST(synthesizer, output_parses) {
    TempDir dir;
    auto s = synth(dir, TRAFFIC);
    REQUIRE_NE(s.table, "");
    std::vector<std::unique_ptr<Node>> nodes;
    std::unordered_map<std::string, std::vector<Node *>> pi, po;
    Capture cap(std::cerr);
    CHECK_EQ(Parser::parse(s.table, nodes, pi, po), 0);
    CHECK_EQ(cap.str(), "");   // not even an unused-wire warning

    std::set<std::string> inNames, outNames;
    for (auto &[n, v] : pi) inNames.insert(n);
    for (auto &[n, v] : po) outNames.insert(n);
    CHECK_EQ(inNames, (std::set<std::string>{"en", "clk", "reset"}));
    CHECK_EQ(outNames, (std::set<std::string>{"R", "Y", "G"}));
}

TEST(synthesizer, uses_only_documented_cells) {
    TempDir dir;
    auto s = synth(dir, TRAFFIC);
    REQUIRE_NE(s.table, "");
    for (auto &r : rows(readFile(s.table))) {
        REQUIRE_EQ(r.size(), 6u);
        static const std::set<std::string> ok = {"-", "$_AND_", "$_OR_", "$_NOT_", "$_DFF_P_"};
        INFO("row cell " << r[1]);
        CHECK(ok.count(r[2]));
    }
}

TEST(synthesizer, one_dff_per_state_bit) {
    struct Case { int states; int bits; };
    for (auto [states, bits] : {Case{1, 1}, Case{2, 1}, Case{3, 2}, Case{4, 2}, Case{5, 3}, Case{8, 3}, Case{9, 4}}) {
        // a counter through `states` states, advancing when en = 1
        std::string fsm = "inputs: en\noutputs: z\nreset: S0\n";
        for (int i = 0; i < states; i++) {
            std::string cur = "S" + std::to_string(i), next = "S" + std::to_string((i + 1) % states);
            fsm += cur + " | 0 | " + cur + " | " + (i == 0 ? "1" : "0") + "\n";
            fsm += cur + " | 1 | " + next + " | " + (i == 0 ? "1" : "0") + "\n";
        }
        TempDir dir;
        auto s = synth(dir, fsm);
        INFO(states << " states");
        REQUIRE_NE(s.table, "");
        std::set<std::string> dffs;
        for (auto &r : rows(readFile(s.table)))
            if (r[2] == "$_DFF_P_") dffs.insert(r[1]);
        CHECK_EQ(static_cast<int>(dffs.size()), bits);
    }
}

TEST(synthesizer, header_documents_state_encoding) {
    TempDir dir;
    auto s = synth(dir, TRAFFIC);
    REQUIRE_NE(s.table, "");
    CHECK_CONTAINS(readFile(s.table), "Sg=00 Sy=01 Sr=10");
    CHECK_CONTAINS(readFile(s.table), "(reset: Sg)");
}

TEST(synthesizer, reset_state_is_encoded_as_zero_even_if_listed_late) {
    std::string fsm =
        "inputs: x\noutputs: z\nreset: B\n"
        "A | 0 | B | 0\nA | 1 | A | 1\nB | 0 | A | 0\nB | 1 | B | 1\n";
    TempDir dir;
    auto s = synth(dir, fsm);
    REQUIRE_NE(s.table, "");
    CHECK_CONTAINS(readFile(s.table), "B=0 A=1");
}

TEST(synthesizer, module_name_is_sanitized_file_stem) {
    TempDir dir;
    auto s = synth(dir, TRAFFIC, "my-fsm.v1.net");
    REQUIRE_NE(s.table, "");
    auto r = rows(readFile(s.table));
    REQUIRE(!r.empty());
    CHECK_EQ(r[0][0], "my_fsm_v1");
}

TEST(synthesizer, is_deterministic) {
    TempDir a, b;
    auto s1 = synth(a, TRAFFIC), s2 = synth(b, TRAFFIC);
    REQUIRE_NE(s1.table, "");
    REQUIRE_NE(s2.table, "");
    CHECK_EQ(withoutComments(readFile(s1.table)), withoutComments(readFile(s2.table)));
}

TEST(synthesizer, spaced_and_packed_bits_are_equivalent) {
    std::string packed =
        "inputs: a b\noutputs: x y\nreset: S\n"
        "S | 01 | T | 10\nS | 10 | S | 01\nT | 11 | S | 11\n";
    std::string spaced =
        "inputs: a b\noutputs: x y\nreset: S\n"
        "S | 0 1 | T | 1 0\nS | 1 0 | S | 0 1\nT | 1 1 | S | 1 1\n";
    TempDir d1, d2;
    auto s1 = synth(d1, packed), s2 = synth(d2, spaced);
    REQUIRE_NE(s1.table, "");
    REQUIRE_NE(s2.table, "");
    CHECK_EQ(withoutComments(readFile(s1.table)), withoutComments(readFile(s2.table)));
}

TEST(synthesizer, comments_and_blank_lines_are_ignored) {
    std::string fsm =
        "# a comment\n\n   \n"
        "inputs: en   # trailing comment\n"
        "outputs: z\n"
        "reset: A\n"
        "A | 1 | B | 1   # row comment\n"
        "B | 1 | A | 0\n";
    TempDir dir;
    auto s = synth(dir, fsm);
    CHECK_NE(s.table, "");
    CHECK_EQ(s.err, "");
}

TEST(synthesizer, fsm_without_inputs) {
    std::string fsm = "inputs:\noutputs: z\nreset: A\nA | | B | 0\nB | | C | 1\nC | | A | 0\n";
    TempDir dir;
    auto s = synth(dir, fsm);
    CHECK_NE(s.table, "");
    CHECK_EQ(s.err, "");
}

TEST(synthesizer, fsm_without_outputs) {
    std::string fsm = "inputs: x\noutputs:\nreset: A\nA | 1 | B |\nB | 1 | A |\n";
    TempDir dir;
    auto s = synth(dir, fsm);
    CHECK_NE(s.table, "");
    CHECK_EQ(s.err, "");
}

TEST(synthesizer, output_that_is_never_one_is_tied_low) {
    std::string fsm = "inputs: x\noutputs: z never\nreset: A\nA | 1 | B | 1 0\nB | 1 | A | 0 0\n";
    TempDir dir;
    auto s = synth(dir, fsm);
    REQUIRE_NE(s.table, "");
    // the constant is reset AND NOT reset
    bool found = false;
    for (auto &r : rows(readFile(s.table)))
        if (r[1] == "never") found = true;
    CHECK(found);
}

// ---------------------------------------------------------------------------
// errors
// ---------------------------------------------------------------------------

TEST(synthesizer, err_missing_file) {
    Capture cap(std::cerr);
    CHECK_EQ(Synthesizer::synthesize("/nonexistent/fsm.net"), "");
    CHECK_CONTAINS(cap.str(), "cannot open file");
}

TEST(synthesizer, err_missing_reset) {
    expectError("inputs: a\noutputs: z\nS | 1 | S | 1\n", "missing 'reset:'");
}

TEST(synthesizer, err_reset_with_two_states) {
    expectError("inputs: a\noutputs: z\nreset: A B\nA | 1 | B | 1\n", "exactly one state");
}

TEST(synthesizer, err_duplicate_key) {
    expectError("inputs: a\ninputs: b\noutputs: z\nreset: A\n", "given more than once");
}

TEST(synthesizer, err_unknown_key) {
    expectError("inputs: a\noutputs: z\nreset: A\nclock: c\n", "unknown key 'clock'");
}

TEST(synthesizer, err_line_without_colon_or_bar) {
    expectError("inputs: a\noutputs: z\nreset: A\nhello world\n", "expected 'inputs:'");
}

TEST(synthesizer, err_invalid_signal_name) {
    expectError("inputs: 1a\noutputs: z\nreset: A\n", "invalid name '1a'");
}

TEST(synthesizer, err_invalid_state_name) {
    expectError("inputs: a\noutputs: z\nreset: A\nA | 1 | B-2 | 1\n", "invalid state name 'B-2'");
}

TEST(synthesizer, err_reserved_signal_names) {
    expectError("inputs: clk\noutputs: z\nreset: A\nA | 1 | A | 1\n", "used twice or reserved");
    expectError("inputs: a\noutputs: reset\nreset: A\nA | 1 | A | 1\n", "used twice or reserved");
}

TEST(synthesizer, err_duplicate_signal_name) {
    expectError("inputs: a\noutputs: a\nreset: A\nA | 1 | A | 1\n", "used twice or reserved");
}

TEST(synthesizer, err_wrong_number_of_fields) {
    expectError("inputs: a\noutputs: z\nreset: A\nA | 1 | A\n", "expected 'current | inputs | next | outputs'");
}

TEST(synthesizer, err_wrong_number_of_input_bits) {
    expectError("inputs: a b\noutputs: z\nreset: A\nA | 1 | A | 1\n", "expected 2 input bits");
}

TEST(synthesizer, err_wrong_number_of_output_bits) {
    expectError("inputs: a\noutputs: y z\nreset: A\nA | 1 | A | 1\n", "expected 2 output bits");
}

TEST(synthesizer, err_bit_that_is_not_0_or_1) {
    expectError("inputs: a\noutputs: z\nreset: A\nA | x | A | 1\n", "input bits (0/1)");
    expectError("inputs: a\noutputs: z\nreset: A\nA | 1 | A | 2\n", "output bits (0/1)");
}

TEST(synthesizer, err_duplicate_transition) {
    expectError("inputs: a\noutputs: z\nreset: A\nA | 1 | A | 1\nA | 1 | A | 0\n", "duplicate transition");
}

TEST(synthesizer, err_errors_report_line_numbers) {
    TempDir dir;
    auto s = synth(dir, "inputs: a\noutputs: z\nreset: A\nA | 1 | A | 1\nA | 11 | A | 1\n");
    CHECK_CONTAINS(s.err, "fsm.net:5:");
}

TEST(synthesizer, err_input_that_affects_nothing) {
    // every row goes to the reset state with all outputs 0: no logic reads `a`
    expectError("inputs: a\noutputs: z\nreset: A\nA | 1 | A | 0\n", "does not affect");
}

TEST(synthesizer, err_refuses_to_overwrite_a_table_input) {
    TempDir dir;
    auto s = synth(dir, TRAFFIC, "fsm.table");
    CHECK_EQ(s.table, "");
    CHECK_CONTAINS(s.err, "refusing to overwrite");
    CHECK_EQ(readFile(dir.path("fsm.table")), TRAFFIC);   // input untouched
}

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <map>
#include <set>
#include <vector>
#include <utility>
#include <cctype>

#include "synthesizer/Synthesizer.hpp"

// Converts an FSM table (docs/fsm.net) into a netlist the Parser accepts.
// Algorithm: docs/synthesis_algorithm.txt
//  - states are binary encoded, reset state = all zeros, one $dff_p per bit
//  - every FSM row becomes one minterm over the state bits and the inputs
//  - each D input / output line is the OR of the minterms that set it to 1
//  - gates have two inputs, so wider ANDs/ORs are built as chains
// Reset is synchronous for now: every D input is ANDed with NOT reset, so a
// clock edge while reset is high moves the FSM to the reset state.

namespace {
    // One transition row: current | inputs | next | outputs
    struct Row {
        int line;
        std::string current, inputs, next, outputs;
    };

    struct Fsm {
        std::vector<std::string> inputs, outputs;
        std::string reset;
        std::vector<Row> rows;
    };

    // Literals ANDed together: (wire, value the wire must have)
    using Term = std::vector<std::pair<std::string, bool>>;

    std::string trim(const std::string &s) {
        auto b = s.find_first_not_of(" \t\r");
        if (b == std::string::npos)
            return "";
        auto e = s.find_last_not_of(" \t\r");
        return s.substr(b, e - b + 1);
    }

    std::vector<std::string> splitWords(const std::string &s) {
        std::istringstream iss(s);
        std::vector<std::string> words;
        std::string w;
        while (iss >> w)
            words.push_back(w);
        return words;
    }

    // "0 0 1" and "001" are both accepted for a bit field
    std::string removeSpaces(const std::string &s) {
        std::string r;
        for (char c : s)
            if (!std::isspace(static_cast<unsigned char>(c)))
                r += c;
        return r;
    }

    bool isBits(const std::string &s, size_t n) {
        if (s.size() != n)
            return false;
        for (char c : s)
            if (c != '0' && c != '1')
                return false;
        return true;
    }

    // Names end up as netlist tokens, so keep them to plain identifiers
    bool isIdentifier(const std::string &s) {
        if (s.empty() || !(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_'))
            return false;
        for (char c : s)
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
                return false;
        return true;
    }

    bool error(const std::string &path, int line, const std::string &msg) {
        std::cerr << path << ':' << line << ": " << msg << '\n';
        return false;
    }

    // Reads header lines (inputs:/outputs:/reset:) and transition rows.
    // Bit widths are checked later, once all headers are known.
    bool readFsm(const std::string &path, Fsm &fsm) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "cannot open file " << path << '\n';
            return false;
        }

        std::set<std::string> seenKeys;
        std::string line;
        int lineNo = 0;

        while (std::getline(file, line)) {
            lineNo++;
            line = trim(line.substr(0, line.find('#')));
            if (line.empty())
                continue;

            if (line.find('|') != std::string::npos) {
                std::vector<std::string> fields;
                std::istringstream iss(line);
                std::string f;
                while (std::getline(iss, f, '|'))
                    fields.push_back(f);
                if (fields.size() != 4)
                    return error(path, lineNo, "expected 'current | inputs | next | outputs'");

                fsm.rows.push_back({lineNo, trim(fields[0]), removeSpaces(fields[1]), trim(fields[2]), removeSpaces(fields[3])});
                continue;
            }

            auto colon = line.find(':');
            if (colon == std::string::npos)
                return error(path, lineNo, "expected 'inputs:', 'outputs:', 'reset:' or a transition row");

            std::string key = trim(line.substr(0, colon));
            std::vector<std::string> values = splitWords(line.substr(colon + 1));

            if (!seenKeys.insert(key).second)
                return error(path, lineNo, "'" + key + ":' given more than once");

            for (auto &v : values)
                if (!isIdentifier(v))
                    return error(path, lineNo, "invalid name '" + v + "'");

            if (key == "inputs")
                fsm.inputs = values;
            else if (key == "outputs")
                fsm.outputs = values;
            else if (key == "reset") {
                if (values.size() != 1)
                    return error(path, lineNo, "'reset:' takes exactly one state");
                fsm.reset = values[0];
            } else
                return error(path, lineNo, "unknown key '" + key + "'");
        }

        if (fsm.reset.empty())
            return error(path, lineNo, "missing 'reset:' line");
        return true;
    }

    // Emits netlist rows in the Parser's 6-column format:
    //   module  cell  type  port  direction  wire
    // Generated cells are named $<type>$<n> and their outputs <cell>_<port>;
    // '$' can't appear in FSM names, so these never collide with user signals.
    class NetlistWriter {
        std::string module;
        std::ostringstream out;
        int count = 0;
        std::unordered_map<std::string, std::string> inverted;  // wire -> wire carrying NOT wire
        std::unordered_map<std::string, std::string> built;     // "type a b" -> output wire
        std::map<std::string, int> consumers;                   // every driven wire -> number of readers

        void row(const std::string &cell, const std::string &type, const std::string &port,
                 const std::string &dir, const std::string &wire) {
            out << module << '\t' << cell << '\t' << type << '\t' << port << '\t' << dir << '\t' << wire << '\n';
            if (dir == "in" || dir == "po")
                consumers[wire]++;
            else
                consumers.emplace(wire, 0);
        }

        std::string freshCell(const std::string &type) {
            return type + "$" + std::to_string(count++);
        }

    public:
        explicit NetlistWriter(std::string module) : module(std::move(module)) {}

        // Starts a new section; a blank line separates it from the previous one
        void comment(const std::string &text) {
            if (out.tellp() > 0)
                out << '\n';
            out << "# " << text << '\n';
        }

        void primaryInput(const std::string &name) {
            row(name, "-", "-", "pi", "\\" + name);
        }

        void primaryOutput(const std::string &name, const std::string &wire) {
            row(name, "-", "-", "po", wire);
        }

        // NOT gates are shared: each wire is inverted at most once
        std::string notGate(const std::string &a) {
            auto it = inverted.find(a);
            if (it != inverted.end())
                return it->second;

            std::string cell = freshCell("$not"), y = cell + "_Y";
            row(cell, "$not", "A", "in", a);
            row(cell, "$not", "Y", "out", y);
            return inverted[a] = y;
        }

        // Gates are shared too: same type and inputs -> same output wire
        std::string gate(const std::string &type, const std::string &a, const std::string &b) {
            auto key = type + ' ' + a + ' ' + b;
            auto it = built.find(key);
            if (it != built.end())
                return it->second;

            std::string cell = freshCell(type), y = cell + "_Y";
            row(cell, type, "A", "in", a);
            row(cell, type, "B", "in", b);
            row(cell, type, "Y", "out", y);
            return built[key] = y;
        }

        // AND of all literals, chained two at a time
        std::string andTerm(const Term &term) {
            std::string acc;
            for (auto &[wire, value] : term) {
                std::string lit = value ? wire : notGate(wire);
                acc = acc.empty() ? lit : gate("$and", acc, lit);
            }
            return acc;
        }

        // OR of all wires, chained two at a time
        std::string orAll(const std::vector<std::string> &wires) {
            std::string acc;
            for (auto &w : wires)
                acc = acc.empty() ? w : gate("$or", acc, w);
            return acc;
        }

        void dff(const std::string &cell, const std::string &d, const std::string &clk, const std::string &q) {
            row(cell, "$dff_p", "D", "in", d);
            row(cell, "$dff_p", "CLK", "in", clk);
            row(cell, "$dff_p", "Q", "out", q);
        }

        // The Parser rejects wires nobody reads
        std::vector<std::string> unreadWires() const {
            std::vector<std::string> unread;
            for (auto &[wire, n] : consumers)
                if (n == 0)
                    unread.push_back(wire);
            return unread;
        }

        std::string str() const {
            return out.str();
        }
    };

    std::string stateWire(int bit) {
        return "$dff_p$q" + std::to_string(bit) + "_Q";
    }
}

std::string Synthesizer::synthesize(const std::string &path) {
    Fsm fsm;
    if (!readFsm(path, fsm))
        return "";

    // All primary I/O shares one namespace in the netlist
    std::set<std::string> names = {"clk", "reset"};
    for (auto *list : {&fsm.inputs, &fsm.outputs}) {
        for (auto &n : *list) {
            if (!names.insert(n).second) {
                std::cerr << path << ": signal name '" << n << "' is used twice or reserved (clk, reset)\n";
                return "";
            }
        }
    }

    // Encode states: reset state = 0, the rest in order of first appearance
    std::unordered_map<std::string, int> code = {{fsm.reset, 0}};
    std::vector<std::string> states = {fsm.reset};
    for (auto &r : fsm.rows) {
        for (auto *s : {&r.current, &r.next}) {
            if (!isIdentifier(*s)) {
                error(path, r.line, "invalid state name '" + *s + "'");
                return "";
            }
            if (!code.count(*s)) {
                code[*s] = states.size();
                states.push_back(*s);
            }
        }
    }

    int bits = 1;
    while ((size_t(1) << bits) < states.size())
        bits++;

    // One minterm per row; collect it wherever the row produces a 1.
    // (state, input) pairs with no row go to the reset state with all outputs 0.
    std::vector<std::vector<Term>> dTerms(bits), outTerms(fsm.outputs.size());
    std::set<std::pair<int, std::string>> seen;

    for (auto &r : fsm.rows) {
        if (!isBits(r.inputs, fsm.inputs.size())) {
            error(path, r.line, "expected " + std::to_string(fsm.inputs.size()) + " input bits (0/1)");
            return "";
        }
        if (!isBits(r.outputs, fsm.outputs.size())) {
            error(path, r.line, "expected " + std::to_string(fsm.outputs.size()) + " output bits (0/1)");
            return "";
        }

        int cur = code[r.current], next = code[r.next];
        if (!seen.insert({cur, r.inputs}).second) {
            error(path, r.line, "duplicate transition for state " + r.current + " on inputs " + r.inputs);
            return "";
        }

        Term term;
        for (int i = 0; i < bits; i++)
            term.push_back({stateWire(i), ((cur >> i) & 1) != 0});
        for (size_t k = 0; k < fsm.inputs.size(); k++)
            term.push_back({"\\" + fsm.inputs[k], r.inputs[k] == '1'});

        for (int i = 0; i < bits; i++)
            if ((next >> i) & 1)
                dTerms[i].push_back(term);
        for (size_t j = 0; j < fsm.outputs.size(); j++)
            if (r.outputs[j] == '1')
                outTerms[j].push_back(term);
    }

    std::string module = std::filesystem::path(path).stem().string();
    for (char &c : module)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
            c = '_';

    NetlistWriter net(module);

    std::string encoding;
    for (size_t s = 0; s < states.size(); s++) {
        encoding += " " + states[s] + "=";
        for (int i = bits - 1; i >= 0; i--)
            encoding += ((s >> i) & 1) ? '1' : '0';
    }
    net.comment("synthesized from " + path + "\n# state encoding (Q" + std::to_string(bits - 1) + "..Q0):" + encoding + "  (reset: " + fsm.reset + ")");

    net.comment("primary inputs / clock / reset");
    for (auto &in : fsm.inputs)
        net.primaryInput(in);
    net.primaryInput("clk");
    net.primaryInput("reset");

    // Constant 0 (reset AND NOT reset) for lines that are never 1; built once
    std::string zero;
    auto constZero = [&] {
        if (zero.empty())
            zero = net.gate("$and", "\\reset", net.notGate("\\reset"));
        return zero;
    };

    auto sumOfProducts = [&](const std::vector<Term> &terms) {
        if (terms.empty())
            return constZero();
        std::vector<std::string> products;
        for (auto &t : terms)
            products.push_back(net.andTerm(t));
        return net.orAll(products);
    };

    for (int i = 0; i < bits; i++) {
        net.comment("state bit Q" + std::to_string(i) + ": D = (next-state logic) AND NOT reset");
        std::string d = dTerms[i].empty() ? constZero()
                                          : net.gate("$and", sumOfProducts(dTerms[i]), net.notGate("\\reset"));
        net.dff("$dff_p$q" + std::to_string(i), d, "\\clk", stateWire(i));
    }

    for (size_t j = 0; j < fsm.outputs.size(); j++) {
        net.comment("output " + fsm.outputs[j]);
        net.primaryOutput(fsm.outputs[j], sumOfProducts(outTerms[j]));
    }

    auto unread = net.unreadWires();
    if (!unread.empty()) {
        for (auto &w : unread)
            std::cerr << path << ": '" << w << "' does not affect the next state or any output\n";
        return "";
    }

    std::filesystem::path outPath = std::filesystem::path(path).replace_extension(".table");
    if (outPath == std::filesystem::path(path)) {
        std::cerr << path << ": input already has the .table extension, refusing to overwrite it\n";
        return "";
    }

    std::ofstream file(outPath);
    if (!file.is_open()) {
        std::cerr << "cannot write file " << outPath.string() << '\n';
        return "";
    }
    file << net.str();
    return outPath.string();
}

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>
#include <utility>
#include <cctype>

#include "synthesizer/Synthesizer.hpp"

// Converts an FSM table (docs/fsm.net) into a netlist the Parser accepts.
// Algorithm: docs/synthesis_algorithm.txt
//  - states are binary encoded, reset state = all zeros, one $_DFF_P_ per bit
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
                if (line.back() == '|')         // getline drops a trailing empty field
                    fields.push_back("");
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

    // Emits netlist rows in the Parser's 6-column, Yosys write_table format:
    //   module  cell  type  port  direction  wire
    // Generated cells are named $<gate>$<n> (e.g. $and$4 for a $_AND_) and their
    // outputs <cell>_<port>; '$' can't appear in FSM names, so these never
    // collide with user signals.
    // Wires are passed around as numbers, because a large FSM has millions of
    // them and looking gates up by name takes far more memory.
    class NetlistWriter {
        struct Wire {
            std::string name;
            int readers = 0;
            int inverted = -1;      // wire carrying NOT this wire, once built
        };
        using GateTable = std::unordered_map<uint64_t, int>;    // (a, b) -> output wire

        std::string module;
        std::string text;
        int count = 0;
        std::vector<Wire> wires;
        GateTable builtAnd, builtOr;

        void row(const std::string &cell, const char *type, const char *port, const char *dir, const std::string &wire) {
            text.append(module).append(1, '\t').append(cell).append(1, '\t').append(type).append(1, '\t')
                .append(port).append(1, '\t').append(dir).append(1, '\t').append(wire).append(1, '\n');
        }

        void readRow(const std::string &cell, const char *type, const char *port, int wire) {
            wires[wire].readers++;
            row(cell, type, port, "in", wires[wire].name);
        }

        // "$_AND_" -> "$and$<n>"
        std::string freshCell(const std::string &type) {
            std::string name = "$";
            for (char c : type.substr(2, type.size() - 3))
                name += std::tolower(static_cast<unsigned char>(c));
            return name + "$" + std::to_string(count++);
        }

        // Gates are shared: same type and inputs -> same output wire
        int gate(GateTable &built, const char *type, int a, int b) {
            uint64_t key = (static_cast<uint64_t>(a) << 32) | static_cast<uint32_t>(b);
            auto it = built.find(key);
            if (it != built.end())
                return it->second;

            std::string cell = freshCell(type);
            readRow(cell, type, "A", a);
            readRow(cell, type, "B", b);
            int y = addWire(cell + "_Y");
            row(cell, type, "Y", "out", wires[y].name);
            built.emplace(key, y);
            return y;
        }

    public:
        explicit NetlistWriter(std::string module) : module(std::move(module)) {}

        int addWire(std::string name) {
            wires.push_back({std::move(name)});
            return wires.size() - 1;
        }

        // Starts a new section; a blank line separates it from the previous one
        void comment(const std::string &line) {
            if (!text.empty())
                text += '\n';
            text.append("# ").append(line).append(1, '\n');
        }

        int primaryInput(const std::string &name) {
            int wire = addWire("\\" + name);
            row(name, "-", "-", "pi", wires[wire].name);
            return wire;
        }

        void primaryOutput(const std::string &name, int wire) {
            wires[wire].readers++;
            row(name, "-", "-", "po", wires[wire].name);
        }

        // NOT gates are shared: each wire is inverted at most once
        int notGate(int a) {
            if (wires[a].inverted != -1)
                return wires[a].inverted;

            std::string cell = freshCell("$_NOT_");
            readRow(cell, "$_NOT_", "A", a);
            int y = addWire(cell + "_Y");
            row(cell, "$_NOT_", "Y", "out", wires[y].name);
            wires[a].inverted = y;
            return y;
        }

        int andGate(int a, int b) {
            return gate(builtAnd, "$_AND_", a, b);
        }

        int orGate(int a, int b) {
            return gate(builtOr, "$_OR_", a, b);
        }

        // OR of all wires, chained two at a time
        int orAll(const std::vector<int> &inputs) {
            int acc = -1;
            for (int wire : inputs)
                acc = acc == -1 ? wire : orGate(acc, wire);
            return acc;
        }

        void dff(const std::string &cell, int d, int clk, int q) {
            readRow(cell, "$_DFF_P_", "D", d);
            readRow(cell, "$_DFF_P_", "C", clk);
            row(cell, "$_DFF_P_", "Q", "out", wires[q].name);
        }

        // The Parser warns about wires nobody reads; sorted, so errors keep one order
        std::vector<std::string> unreadWires() const {
            std::vector<std::string> unread;
            for (auto &wire : wires)
                if (wire.readers == 0)
                    unread.push_back(wire.name);
            std::sort(unread.begin(), unread.end());
            return unread;
        }

        std::string takeText() {
            return std::move(text);
        }
    };

    std::string stateWire(int bit) {
        return "$dff$q" + std::to_string(bit) + "_Q";
    }

    bool buildNetlist(const std::string &path, std::string &netlist) {
        Fsm fsm;
        if (!readFsm(path, fsm))
            return false;

        // All primary I/O shares one namespace in the netlist
        std::set<std::string> names = {"clk", "reset"};
        for (auto *list : {&fsm.inputs, &fsm.outputs}) {
            for (auto &n : *list) {
                if (!names.insert(n).second) {
                    std::cerr << path << ": signal name '" << n << "' is used twice or reserved (clk, reset)\n";
                    return false;
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
                    return false;
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

        // Each line lists the rows that set it to 1, as row numbers.
        // (state, input) pairs with no row go to the reset state with all outputs 0.
        std::vector<std::vector<int>> dRows(bits), outRows(fsm.outputs.size());
        std::vector<int> currentCode(fsm.rows.size());
        std::set<std::pair<int, std::string>> seen;

        for (size_t n = 0; n < fsm.rows.size(); n++) {
            auto &r = fsm.rows[n];
            if (!isBits(r.inputs, fsm.inputs.size())) {
                error(path, r.line, "expected " + std::to_string(fsm.inputs.size()) + " input bits (0/1)");
                return false;
            }
            if (!isBits(r.outputs, fsm.outputs.size())) {
                error(path, r.line, "expected " + std::to_string(fsm.outputs.size()) + " output bits (0/1)");
                return false;
            }

            int cur = code[r.current], next = code[r.next];
            if (!seen.insert({cur, r.inputs}).second) {
                error(path, r.line, "duplicate transition for state " + r.current + " on inputs " + r.inputs);
                return false;
            }

            currentCode[n] = cur;
            for (int i = 0; i < bits; i++)
                if ((next >> i) & 1)
                    dRows[i].push_back(n);
            for (size_t j = 0; j < fsm.outputs.size(); j++)
                if (r.outputs[j] == '1')
                    outRows[j].push_back(n);
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
        std::vector<int> inputWires;
        for (auto &in : fsm.inputs)
            inputWires.push_back(net.primaryInput(in));
        int clk = net.primaryInput("clk");
        int reset = net.primaryInput("reset");

        std::vector<int> stateWires;
        for (int i = 0; i < bits; i++)
            stateWires.push_back(net.addWire(stateWire(i)));

        // Constant 0 (reset AND NOT reset) for lines that are never 1; built once
        int zero = -1;
        auto constZero = [&] {
            if (zero == -1)
                zero = net.andGate(reset, net.notGate(reset));
            return zero;
        };

        // The minterm of a row: an AND over every state bit and every input,
        // chained two at a time. A row feeding many lines is built only once.
        std::vector<int> minterm(fsm.rows.size(), -1);
        auto mintermOf = [&](int n) {
            if (minterm[n] != -1)
                return minterm[n];
            int acc = -1;
            auto addLiteral = [&](int wire, bool value) {
                int literal = value ? wire : net.notGate(wire);
                acc = acc == -1 ? literal : net.andGate(acc, literal);
            };
            for (int i = 0; i < bits; i++)
                addLiteral(stateWires[i], ((currentCode[n] >> i) & 1) != 0);
            for (size_t k = 0; k < fsm.inputs.size(); k++)
                addLiteral(inputWires[k], fsm.rows[n].inputs[k] == '1');
            return minterm[n] = acc;
        };

        auto sumOfProducts = [&](const std::vector<int> &rows) {
            if (rows.empty())
                return constZero();
            std::vector<int> products;
            for (int n : rows)
                products.push_back(mintermOf(n));
            return net.orAll(products);
        };

        for (int i = 0; i < bits; i++) {
            net.comment("state bit Q" + std::to_string(i) + ": D = (next-state logic) AND NOT reset");
            int d;
            if (dRows[i].empty()) {
                d = constZero();
            } else {
                int notReset = net.notGate(reset);
                d = net.andGate(sumOfProducts(dRows[i]), notReset);
            }
            net.dff("$dff$q" + std::to_string(i), d, clk, stateWires[i]);
        }

        for (size_t j = 0; j < fsm.outputs.size(); j++) {
            net.comment("output " + fsm.outputs[j]);
            net.primaryOutput(fsm.outputs[j], sumOfProducts(outRows[j]));
        }

        auto unread = net.unreadWires();
        if (!unread.empty()) {
            for (auto &w : unread)
                std::cerr << path << ": '" << w << "' does not affect the next state or any output\n";
            return false;
        }

        netlist = net.takeText();
        return true;
    }
}

bool Synthesizer::synthesize(const std::string &path, std::ostream &out) {
    std::string netlist;
    if (!buildNetlist(path, netlist))
        return false;
    out << netlist;
    return true;
}

std::string Synthesizer::synthesize(const std::string &path) {
    std::string netlist;
    if (!buildNetlist(path, netlist))
        return "";

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
    file << netlist;
    return outPath.string();
}

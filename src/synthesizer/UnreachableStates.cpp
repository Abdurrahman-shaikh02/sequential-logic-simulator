#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>
#include <cctype>

#include "synthesizer/UnreachableStates.hpp"

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
        std::vector<std::string> states;    // reset first, then in order of first appearance
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

    // Reads and validates the FSM: headers, row shape, bit widths, state
    // names and duplicate (state, inputs) rows.
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

        std::set<std::string> known = {fsm.reset};
        std::set<std::pair<std::string, std::string>> seen;
        fsm.states.push_back(fsm.reset);

        for (auto &r : fsm.rows) {
            for (auto *s : {&r.current, &r.next}) {
                if (!isIdentifier(*s))
                    return error(path, r.line, "invalid state name '" + *s + "'");
                if (known.insert(*s).second)
                    fsm.states.push_back(*s);
            }
            if (!isBits(r.inputs, fsm.inputs.size()))
                return error(path, r.line, "expected " + std::to_string(fsm.inputs.size()) + " input bits (0/1)");
            if (!isBits(r.outputs, fsm.outputs.size()))
                return error(path, r.line, "expected " + std::to_string(fsm.outputs.size()) + " output bits (0/1)");
            if (!seen.insert({r.current, r.inputs}).second)
                return error(path, r.line, "duplicate transition for state " + r.current + " on inputs " + r.inputs);
        }
        return true;
    }
}

bool UnreachableStates::find(const std::string &path, std::vector<std::string> &states, bool print) {
    states.clear();
    Fsm fsm;
    if (!readFsm(path, fsm))
        return false;

    std::map<std::string, std::vector<std::string>> successors;
    for (auto &r : fsm.rows)
        successors[r.current].push_back(r.next);

    // Breadth-first search from the reset state
    std::set<std::string> reached = {fsm.reset};
    std::queue<std::string> todo;
    todo.push(fsm.reset);
    while (!todo.empty()) {
        std::string s = todo.front();
        todo.pop();
        for (auto &next : successors[s])
            if (reached.insert(next).second)
                todo.push(next);
    }

    std::string reachable;
    for (auto &s : fsm.states) {
        if (reached.count(s))
            reachable += " " + s;
        else
            states.push_back(s);
    }

    if (print) {
        std::cout << "reset state: " << fsm.reset << '\n';
        std::cout << "reachable:  " << reachable << '\n';
        std::cout << "unreachable:";
        if (states.empty())
            std::cout << " none";
        for (auto &s : states)
            std::cout << ' ' << s;
        std::cout << '\n';
    }
    return true;
}

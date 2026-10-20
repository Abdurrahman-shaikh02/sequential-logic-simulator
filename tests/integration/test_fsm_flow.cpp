// End to end: FSM table -> Synthesizer -> Parser -> Simulator, checked against
// a direct interpretation of the same table.

#include <map>
#include <set>

#include "support/helpers.hpp"

#include "synthesizer/Synthesizer.hpp"

using namespace hdltest;

namespace {

struct Fsm {
    std::vector<std::string> inputs, outputs, states;   // states[0] is the reset state
    struct Row { std::string next, outputs; };
    std::map<std::pair<std::string, std::string>, Row> rows;   // (state, input bits) -> row

    std::string text() const {
        std::string s = "inputs:";
        for (auto &i : inputs) s += " " + i;
        s += "\noutputs:";
        for (auto &o : outputs) s += " " + o;
        s += "\nreset: " + states[0] + "\n";
        for (auto &[key, r] : rows) s += key.first + " | " + key.second + " | " + r.next + " | " + r.outputs + "\n";
        return s;
    }
};

// Synthesizes `fsm` and loads it into the simulator
struct FsmSim {
    TempDir dir;
    std::string table;
    std::unique_ptr<NetlistSim> sim;

    explicit FsmSim(const std::string &fsmText) {
        std::string path = dir.write("fsm.net", fsmText);
        std::string err;
        {
            Capture cap(std::cerr);
            table = Synthesizer::synthesize(path);
            err = cap.str();
        }
        if (table.empty()) FAIL("synthesis failed:\n" << err << "\nFSM:\n" << fsmText);
        sim = std::make_unique<NetlistSim>(table);
    }
};

// Runs `cycles` random cycles and compares outputs (before each edge) with the table
void checkAgainstModel(const Fsm &fsm, int cycles, double resetChance) {
    FsmSim fs(fsm.text());
    INFO("FSM:\n" << fsm.text());
    std::string state = fsm.states[0];

    for (int cycle = 0; cycle < cycles; cycle++) {
        std::string bits;
        for (auto &in : fsm.inputs) {
            int v = randomBit();
            bits += static_cast<char>('0' + v);
            NetlistSim::drive(in, v);
        }
        int reset = std::uniform_real_distribution<double>(0, 1)(rng()) < resetChance;
        NetlistSim::drive("reset", reset);
        NetlistSim::settle();

        auto it = fsm.rows.find({state, bits});
        std::string expectOut = it != fsm.rows.end() ? it->second.outputs : std::string(fsm.outputs.size(), '0');
        std::string gotOut;
        for (auto &o : fsm.outputs) gotOut += static_cast<char>('0' + NetlistSim::get(o));
        INFO("cycle " << cycle << " state=" << state << " inputs=" << bits << " reset=" << reset);
        REQUIRE_EQ(gotOut, expectOut);

        NetlistSim::clock();
        state = reset || it == fsm.rows.end() ? fsm.states[0] : it->second.next;
    }
}

std::string bitsOf(unsigned v, size_t n) {
    std::string s;
    for (size_t i = 0; i < n; i++) s += static_cast<char>('0' + ((v >> i) & 1));
    return s;
}

Fsm randomFsm() {
    auto pick = [](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng()); };
    Fsm f;
    int nStates = pick(1, 6), nIn = pick(0, 3), nOut = pick(1, 3);
    for (int i = 0; i < nStates; i++) f.states.push_back("S" + std::to_string(i));
    for (int i = 0; i < nIn; i++) f.inputs.push_back("i" + std::to_string(i));
    for (int i = 0; i < nOut; i++) f.outputs.push_back("o" + std::to_string(i));

    for (auto &s : f.states)
        for (unsigned in = 0; in < (1u << nIn); in++)
            if (pick(0, 9) < 8)   // ~20% of transitions left out: they go to reset
                f.rows[{s, bitsOf(in, nIn)}] = {f.states[pick(0, nStates - 1)], bitsOf(pick(0, (1 << nOut) - 1), nOut)};

    // The synthesizer rejects FSMs where no logic reads the inputs; one 1 in
    // some output column guarantees every input and state bit is read.
    if (f.rows.empty()) f.rows[{f.states[0], bitsOf(0, nIn)}] = {f.states[0], std::string(nOut, '0')};
    bool anyOne = false;
    for (auto &[k, r] : f.rows) anyOne |= r.outputs.find('1') != std::string::npos;
    if (!anyOne) f.rows.begin()->second.outputs[0] = '1';
    return f;
}

}  // namespace

TEST(fsm_flow, traffic_light_fixture) {
    FsmSim fs(readFile(fixture("fsm/traffic_light_fsm.net")));
    // Sg -> Sy -> Sr -> Sg while en = 1; outputs R Y G are Moore outputs
    struct Step { int en, reset; const char *ryg; };
    const Step steps[] = {
        {0, 0, "001"}, {1, 0, "001"},   // hold green, then advance
        {0, 0, "010"}, {1, 0, "010"},   // yellow
        {1, 0, "100"},                  // red
        {1, 0, "001"},                  // green again
        {1, 0, "010"},                  // yellow
        {0, 1, "100"},                  // red; reset is applied at this edge
        {0, 0, "001"},                  // back in green
    };
    int i = 0;
    for (auto &s : steps) {
        NetlistSim::drive("en", s.en);
        NetlistSim::drive("reset", s.reset);
        NetlistSim::settle();
        std::string got;
        for (auto *o : {"R", "Y", "G"}) got += static_cast<char>('0' + NetlistSim::get(o));
        INFO("step " << i++);
        CHECK_EQ(got, std::string(s.ryg));
        NetlistSim::clock();
    }
}

TEST(fsm_flow, traffic_light_random_against_table) {
    Fsm f;
    f.inputs = {"en"};
    f.outputs = {"R", "Y", "G"};
    f.states = {"Sg", "Sy", "Sr"};
    f.rows = {{{"Sg", "0"}, {"Sg", "001"}}, {{"Sg", "1"}, {"Sy", "001"}},
              {{"Sy", "0"}, {"Sy", "010"}}, {{"Sy", "1"}, {"Sr", "010"}},
              {{"Sr", "0"}, {"Sr", "100"}}, {{"Sr", "1"}, {"Sg", "100"}}};
    checkAgainstModel(f, 500, 0.05);
}

TEST(fsm_flow, missing_transitions_go_to_reset_with_zero_outputs) {
    Fsm f;
    f.inputs = {"x"};
    f.outputs = {"z"};
    f.states = {"A", "B"};
    f.rows = {{{"A", "1"}, {"B", "1"}}, {{"B", "1"}, {"B", "1"}}};   // x = 0 has no row anywhere
    checkAgainstModel(f, 300, 0.0);
}

TEST(fsm_flow, reset_returns_to_reset_state_from_anywhere) {
    Fsm f;
    f.inputs = {"go"};
    f.outputs = {"s1", "s0"};
    f.states = {"Z", "O", "T", "H"};   // a 2-bit counter that shows its state
    f.rows = {{{"Z", "0"}, {"Z", "00"}}, {{"Z", "1"}, {"O", "00"}}, {{"O", "0"}, {"O", "01"}},
              {{"O", "1"}, {"T", "01"}}, {{"T", "0"}, {"T", "10"}}, {{"T", "1"}, {"H", "10"}},
              {{"H", "0"}, {"H", "11"}}, {{"H", "1"}, {"Z", "11"}}};
    checkAgainstModel(f, 500, 0.15);
}

TEST(fsm_flow, random_fsms_match_their_tables) {
    for (int i = 0; i < 40; i++) {
        INFO("random FSM #" << i);
        checkAgainstModel(randomFsm(), 150, 0.05);
    }
}

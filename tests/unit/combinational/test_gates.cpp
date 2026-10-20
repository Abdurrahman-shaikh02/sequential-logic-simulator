// Every combinational gate, against a reference function:
//   - exhaustive truth table on a fresh gate
//   - every single-port transition between any two input states
//   - output change => node is scheduled in the topological queue, no change => not
//   - inside a real simulated circuit (InputNodes -> gate -> OutputNode)
//   - a two-level tree of the same gate, exhaustive over 3-4 inputs
//   - invalid ports are reported and do not corrupt the gate

#include <functional>

#include "support/helpers.hpp"

#include "combinational/AND.hpp"
#include "combinational/ANDNOT.hpp"
#include "combinational/MUX.hpp"
#include "combinational/NAND.hpp"
#include "combinational/NOR.hpp"
#include "combinational/NOT.hpp"
#include "combinational/OR.hpp"
#include "combinational/ORNOT.hpp"
#include "combinational/XOR.hpp"

using namespace hdltest;

namespace {

struct GateSpec {
    const char *name;
    std::function<std::unique_ptr<Node>()> make;
    std::vector<Port> ports;
    std::function<int(const std::vector<int> &)> ref;   // inputs in `ports` order
};

template <class T>
std::unique_ptr<Node> make() { return std::make_unique<T>(); }

// Yosys cell semantics (yosys/techlibs/common/simcells.v)
const std::vector<GateSpec> &gates() {
    static const std::vector<GateSpec> g = {
        {"AND",    make<AND>,    {Port::A, Port::B},          [](auto &v) { return v[0] & v[1]; }},
        {"NAND",   make<NAND>,   {Port::A, Port::B},          [](auto &v) { return !(v[0] & v[1]); }},
        {"OR",     make<OR>,     {Port::A, Port::B},          [](auto &v) { return v[0] | v[1]; }},
        {"NOR",    make<NOR>,    {Port::A, Port::B},          [](auto &v) { return !(v[0] | v[1]); }},
        {"XOR",    make<XOR>,    {Port::A, Port::B},          [](auto &v) { return v[0] ^ v[1]; }},
        {"ANDNOT", make<ANDNOT>, {Port::A, Port::B},          [](auto &v) { return v[0] & !v[1]; }},
        {"ORNOT",  make<ORNOT>,  {Port::A, Port::B},          [](auto &v) { return v[0] | !v[1]; }},
        {"NOT",    make<NOT>,    {Port::A},                   [](auto &v) { return !v[0]; }},
        {"MUX",    make<MUX>,    {Port::A, Port::B, Port::S}, [](auto &v) { return v[2] ? v[1] : v[0]; }},
    };
    return g;
}

std::vector<int> bitsOf(unsigned row, size_t n) {
    std::vector<int> v(n);
    for (size_t i = 0; i < n; i++) v[i] = (row >> i) & 1;
    return v;
}

std::string rowText(const GateSpec &g, const std::vector<int> &v) {
    std::string s;
    for (size_t i = 0; i < v.size(); i++) s += toString(g.ports[i]) + "=" + std::to_string(v[i]) + " ";
    return s;
}

// Drives every port of `n` (A first) and returns the result
Signal apply(Node &n, const GateSpec &g, const std::vector<int> &v) {
    for (size_t i = 0; i < v.size(); i++) n.evaluate(g.ports[i], sig(v[i]));
    return n.getResult();
}

}  // namespace

TEST(gates, truth_table_on_fresh_gate) {
    for (auto &g : gates()) {
        unsigned rows = 1u << g.ports.size();
        for (unsigned r = 0; r < rows; r++) {
            Simulator::clear();
            auto v = bitsOf(r, g.ports.size());
            INFO(g.name << " " << rowText(g, v));
            auto n = g.make();
            CHECK_EQ(bit(apply(*n, g, v)), g.ref(v));
        }
    }
}

TEST(gates, every_single_port_transition) {
    // from every input state, flip each port once and compare
    for (auto &g : gates()) {
        unsigned rows = 1u << g.ports.size();
        for (unsigned from = 0; from < rows; from++) {
            for (size_t p = 0; p < g.ports.size(); p++) {
                Simulator::clear();
                auto n = g.make();
                auto v = bitsOf(from, g.ports.size());
                apply(*n, g, v);
                v[p] ^= 1;
                INFO(g.name << " flip " << toString(g.ports[p]) << " -> " << rowText(g, v));
                n->evaluate(g.ports[p], sig(v[p]));
                CHECK_EQ(bit(n->getResult()), g.ref(v));
            }
        }
    }
}

TEST(gates, random_walk_matches_reference) {
    for (auto &g : gates()) {
        Simulator::clear();
        auto n = g.make();
        std::vector<int> v(g.ports.size(), 0);
        apply(*n, g, v);
        for (int step = 0; step < 300; step++) {
            size_t p = rng()() % g.ports.size();
            v[p] = randomBit();
            n->evaluate(g.ports[p], sig(v[p]));
            INFO(g.name << " step " << step << ": " << rowText(g, v));
            CHECK_EQ(bit(n->getResult()), g.ref(v));
        }
    }
}

TEST(gates, schedules_commit_only_when_output_changes) {
    for (auto &g : gates()) {
        unsigned rows = 1u << g.ports.size();
        for (unsigned from = 0; from < rows; from++) {
            for (size_t p = 0; p < g.ports.size(); p++) {
                for (int value : {0, 1}) {
                    Simulator::clear();
                    auto n = g.make();
                    auto v = bitsOf(from, g.ports.size());
                    apply(*n, g, v);
                    int before = bit(n->getResult());
                    Simulator::topologicalQueue.clear();

                    v[p] = value;
                    INFO(g.name << " set " << toString(g.ports[p]) << "=" << value << " -> " << rowText(g, v));
                    n->evaluate(g.ports[p], sig(value));
                    bool changed = bit(n->getResult()) != before;
                    CHECK_EQ(!Simulator::topologicalQueue.isEmpty(), changed);
                    // gates never commit directly or touch the other queues
                    CHECK(Simulator::combinationalQueue.isEmpty());
                    CHECK(Simulator::sequentialQueue.isEmpty());
                }
            }
        }
    }
}

TEST(gates, power_up_output_is_low_before_any_evaluation) {
    // Even inverting gates start LOW; Simulator::initialize() fixes that up.
    for (auto &g : gates()) {
        INFO(g.name);
        CHECK_EQ(g.make()->getResult(), Signal::LOW);
    }
}

TEST(gates, initialize_settles_inverting_gates_high) {
    // all inputs 0 after initialize(): NOT/NAND/NOR/ORNOT must output 1
    for (auto &g : gates()) {
        INFO(g.name);
        Circuit c;
        std::vector<InputNode *> ins;
        for (size_t i = 0; i < g.ports.size(); i++) ins.push_back(&c.input("in" + std::to_string(i)));
        Node &gate = *Simulator::nodes.emplace_back(g.make());
        gate.priority = static_cast<int>(Simulator::nodes.size()) - 1;
        for (size_t i = 0; i < g.ports.size(); i++) Circuit::connect(*ins[i], gate, g.ports[i]);
        auto &out = c.probe(gate, "y");
        c.init();
        CHECK_EQ(Circuit::read(out), g.ref(std::vector<int>(g.ports.size(), 0)));
    }
}

TEST(gates, truth_table_through_the_simulator) {
    // inputs -> gate -> output, walked in Gray-code order so exactly one input
    // changes per step, then back through every row in random order
    for (auto &g : gates()) {
        Circuit c;
        std::vector<InputNode *> ins;
        for (size_t i = 0; i < g.ports.size(); i++) ins.push_back(&c.input("in" + std::to_string(i)));
        Node &gate = *Simulator::nodes.emplace_back(g.make());
        gate.priority = static_cast<int>(Simulator::nodes.size()) - 1;
        for (size_t i = 0; i < g.ports.size(); i++) Circuit::connect(*ins[i], gate, g.ports[i]);
        auto &out = c.probe(gate, "y");
        c.init();

        unsigned rows = 1u << g.ports.size();
        std::vector<unsigned> order;
        for (unsigned i = 0; i < rows; i++) order.push_back(i ^ (i >> 1));   // Gray code
        for (unsigned i = 0; i < 4 * rows; i++) order.push_back(rng()() % rows);

        for (unsigned r : order) {
            auto v = bitsOf(r, g.ports.size());
            std::vector<std::pair<InputNode *, int>> drive;
            for (size_t i = 0; i < v.size(); i++) drive.push_back({ins[i], v[i]});
            Circuit::setAll(drive);
            INFO(g.name << " " << rowText(g, v));
            CHECK_EQ(Circuit::read(out), g.ref(v));
        }
    }
}

TEST(gates, two_level_tree_exhaustive) {
    // out = G(G(x0, x1, ...), G(...), ...) for gates with 2+ inputs: exercises
    // gate-to-gate propagation and topological commit order
    for (auto &g : gates()) {
        if (g.ports.size() < 2) continue;
        size_t k = g.ports.size();
        Circuit c;
        std::vector<InputNode *> ins;
        for (size_t i = 0; i < k * k; i++) ins.push_back(&c.input("x" + std::to_string(i)));

        std::vector<Node *> level1;
        for (size_t j = 0; j < k; j++) {
            Node &n = *Simulator::nodes.emplace_back(g.make());
            n.priority = static_cast<int>(Simulator::nodes.size()) - 1;
            for (size_t i = 0; i < k; i++) Circuit::connect(*ins[j * k + i], n, g.ports[i]);
            level1.push_back(&n);
        }
        Node &root = *Simulator::nodes.emplace_back(g.make());
        root.priority = static_cast<int>(Simulator::nodes.size()) - 1;
        for (size_t j = 0; j < k; j++) Circuit::connect(*level1[j], root, g.ports[j]);
        auto &out = c.probe(root, "y");
        c.init();

        unsigned rows = 1u << (k * k);
        unsigned step = rows > 512 ? 7 : 1;   // MUX tree has 9 inputs: sample every 7th row
        for (unsigned r = 0; r < rows; r += step) {
            auto x = bitsOf(r, k * k);
            std::vector<std::pair<InputNode *, int>> drive;
            for (size_t i = 0; i < x.size(); i++) drive.push_back({ins[i], x[i]});
            Circuit::setAll(drive);

            std::vector<int> mid;
            for (size_t j = 0; j < k; j++)
                mid.push_back(g.ref(std::vector<int>(x.begin() + j * k, x.begin() + (j + 1) * k)));
            INFO(g.name << " tree row " << r);
            CHECK_EQ(Circuit::read(out), g.ref(mid));
        }
    }
}

TEST(gates, invalid_port_is_reported_and_keeps_state) {
    for (auto &g : gates()) {
        Simulator::clear();
        auto n = g.make();
        auto v = std::vector<int>(g.ports.size(), 1);
        apply(*n, g, v);
        Signal before = n->getResult();

        Capture err(std::cerr);
        n->evaluate(Port::CLK, Signal::LOW);
        INFO(g.name);
        CHECK_CONTAINS(err.str(), "does not exist");
        CHECK_EQ(n->getResult(), before);
    }
}

TEST(gates, mux_selects_a_when_s_low_and_b_when_s_high) {
    // spelled out separately because A/B/S order is easy to get backwards
    Simulator::clear();
    MUX m;
    m.evaluate(Port::A, Signal::HIGH);
    m.evaluate(Port::B, Signal::LOW);
    m.evaluate(Port::S, Signal::LOW);
    CHECK_EQ(m.getResult(), Signal::HIGH);
    m.evaluate(Port::S, Signal::HIGH);
    CHECK_EQ(m.getResult(), Signal::LOW);
}

TEST(gates, andnot_and_ornot_invert_b_not_a) {
    Simulator::clear();
    ANDNOT an;
    an.evaluate(Port::A, Signal::HIGH);
    an.evaluate(Port::B, Signal::LOW);
    CHECK_EQ(an.getResult(), Signal::HIGH);   // A & ~B

    ORNOT on;
    on.evaluate(Port::A, Signal::LOW);
    on.evaluate(Port::B, Signal::HIGH);
    CHECK_EQ(on.getResult(), Signal::LOW);    // A | ~B
}

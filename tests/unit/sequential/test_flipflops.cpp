// Specs for DFF_N, TFF_P/TFF_N and JKFF_P/JKFF_N.
//
// They assume the same shape as DFF_P: a default constructor, the parser's port
// names (D/T/J/K + CLK, see Parser.cpp) and Q as the result. A clock edge samples
// the inputs driven in the same step.

#include "support/helpers.hpp"
#include "sequential/DFF_N.hpp"
#include "sequential/TFF_P.hpp"
#include "sequential/TFF_N.hpp"
#include "sequential/JKFF_P.hpp"
#include "sequential/JKFF_N.hpp"

using namespace hdltest;

namespace {

// inputs (by port) + clk -> FF -> q
template <class FF>
struct Bench {
    Circuit c;
    std::vector<std::pair<Port, InputNode *>> ins;
    InputNode &clk;
    FF &ff;
    OutputNode &q;

    explicit Bench(std::vector<Port> ports)
        : clk(makeInputs(ports)), ff(c.add<FF>()), q(c.probe(ff, "q")) {
        for (auto &[p, in] : ins) Circuit::connect(*in, ff, p);
        Circuit::connect(clk, ff, Port::CLK);
        Circuit::init();
    }

    InputNode &makeInputs(const std::vector<Port> &ports) {
        for (auto p : ports) ins.push_back({p, &c.input(toString(p))});
        return c.input("clk");
    }

    void set(Port p, int v) {
        for (auto &[port, in] : ins)
            if (port == p) Circuit::set(*in, v);
    }
    int Q() { return Circuit::read(q); }
};

}  // namespace

TEST(dff_n, falling_edge_captures_d) {
    Bench<DFF_N> b({Port::D});
    b.set(Port::D, 1);
    Circuit::set(b.clk, 1);
    CHECK_EQ(b.Q(), 0);   // rising edge: nothing
    Circuit::set(b.clk, 0);
    CHECK_EQ(b.Q(), 1);
}

TEST(dff_n, random_stimulus_matches_reference_model) {
    Bench<DFF_N> b({Port::D});
    Capture err(std::cerr);
    int clk = 0, q = 0;
    for (int step = 0; step < 1000; step++) {
        int nd = randomBit(), nclk = randomBit();
        if (clk && !nclk) q = nd;   // edge samples the D of this step
        Circuit::setAll({{b.ins[0].second, nd}, {&b.clk, nclk}});
        clk = nclk;
        INFO("step " << step);
        REQUIRE_EQ(b.Q(), q);
    }
}

template <class FF>
void tffSpec(bool positive) {
    Bench<FF> b({Port::T});
    Capture err(std::cerr);
    int t = 0, clk = 0, q = 0;
    for (int step = 0; step < 1000; step++) {
        int nt = randomBit(), nclk = randomBit();
        bool edge = positive ? (!clk && nclk) : (clk && !nclk);
        if (edge && nt) q ^= 1;
        Circuit::setAll({{b.ins[0].second, nt}, {&b.clk, nclk}});
        t = nt;
        clk = nclk;
        INFO("step " << step << " t=" << t << " clk=" << clk);
        REQUIRE_EQ(b.Q(), q);
    }
}

TEST(tff_p, toggles_on_rising_edge_when_t_high) {
    Bench<TFF_P> b({Port::T});
    b.set(Port::T, 1);
    for (int i = 1; i <= 6; i++) {
        Circuit::pulse(b.clk);
        CHECK_EQ(b.Q(), i % 2);
    }
    b.set(Port::T, 0);
    Circuit::pulse(b.clk);
    CHECK_EQ(b.Q(), 0);
}

TEST(tff_p, random_stimulus_matches_reference_model) { tffSpec<TFF_P>(true); }

TEST(tff_n, random_stimulus_matches_reference_model) { tffSpec<TFF_N>(false); }

template <class FF>
void jkffSpec(bool positive) {
    Bench<FF> b({Port::J, Port::K});
    Capture err(std::cerr);
    int j = 0, k = 0, clk = 0, q = 0;
    for (int step = 0; step < 1000; step++) {
        int nj = randomBit(), nk = randomBit(), nclk = randomBit();
        bool edge = positive ? (!clk && nclk) : (clk && !nclk);
        if (edge) q = nj && nk ? !q : nj ? 1 : nk ? 0 : q;   // 00 hold, 10 set, 01 reset, 11 toggle
        Circuit::setAll({{b.ins[0].second, nj}, {b.ins[1].second, nk}, {&b.clk, nclk}});
        j = nj;
        k = nk;
        clk = nclk;
        INFO("step " << step << " j=" << j << " k=" << k << " clk=" << clk);
        REQUIRE_EQ(b.Q(), q);
    }
}

TEST(jkff_p, set_reset_hold_toggle) {
    Bench<JKFF_P> b({Port::J, Port::K});
    b.set(Port::J, 1);
    Circuit::pulse(b.clk);
    CHECK_EQ(b.Q(), 1);   // set
    b.set(Port::J, 0);
    Circuit::pulse(b.clk);
    CHECK_EQ(b.Q(), 1);   // hold
    b.set(Port::K, 1);
    Circuit::pulse(b.clk);
    CHECK_EQ(b.Q(), 0);   // reset
    b.set(Port::J, 1);
    Circuit::pulse(b.clk);
    CHECK_EQ(b.Q(), 1);   // toggle
    Circuit::pulse(b.clk);
    CHECK_EQ(b.Q(), 0);   // toggle
}

TEST(jkff_p, random_stimulus_matches_reference_model) { jkffSpec<JKFF_P>(true); }

TEST(jkff_n, random_stimulus_matches_reference_model) { jkffSpec<JKFF_N>(false); }

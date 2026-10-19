// DFF_P: positive-edge D flip-flop.

#include "support/helpers.hpp"

#include "combinational/NOT.hpp"
#include "sequential/DFF_P.hpp"

using namespace hdltest;

namespace {

// d, clk -> DFF_P -> q
struct DffBench {
    Circuit c;
    InputNode &d = c.input("d");
    InputNode &clk = c.input("clk");
    DFF_P &ff = c.add<DFF_P>();
    OutputNode &q = c.probe(ff, "q");

    DffBench() {
        Circuit::connect(d, ff, Port::D);
        Circuit::connect(clk, ff, Port::CLK);
        Circuit::init();
    }
    int Q() { return Circuit::read(q); }
};

}  // namespace

// ---------------------------------------------------------------------------
// direct (no simulator loop)
// ---------------------------------------------------------------------------

TEST(dff_p, set_port_d_then_change_state_loads_d) {
    Simulator::clear();
    DFF_P ff;
    ff.setPort(Port::D, Signal::HIGH);
    CHECK_EQ(ff.getResult(), Signal::LOW);   // setPort alone does not change Q
    ff.changeState();
    CHECK_EQ(ff.getResult(), Signal::HIGH);
}

TEST(dff_p, change_state_schedules_commit_only_on_change) {
    Simulator::clear();
    DFF_P ff;
    ff.changeState();   // D = 0 = Q
    CHECK(Simulator::sequentialQueue.isEmpty());

    ff.setPort(Port::D, Signal::HIGH);
    ff.changeState();
    CHECK(!Simulator::sequentialQueue.isEmpty());
    CHECK_EQ(drain(Simulator::sequentialQueue), 1);
}

TEST(dff_p, set_port_ignores_other_ports) {
    Simulator::clear();
    DFF_P ff;
    ff.setPort(Port::CLK, Signal::HIGH);
    ff.setPort(Port::J, Signal::HIGH);
    ff.changeState();
    CHECK_EQ(ff.getResult(), Signal::LOW);
}

TEST(dff_p, invalid_port_is_reported) {
    Simulator::clear();
    DFF_P ff;
    Capture err(std::cerr);
    ff.evaluate(Port::A, Signal::HIGH);
    CHECK_CONTAINS(err.str(), "port does not exist");
    CHECK(Simulator::sequentialQueue.isEmpty());
}

TEST(dff_p, uninitialized_ff_only_latches_inputs) {
    // Before its first commit (done by Simulator::initialize) a flip-flop just
    // records D/CLK and never triggers.
    Simulator::clear();
    DFF_P ff;
    ff.evaluate(Port::D, Signal::HIGH);
    ff.evaluate(Port::CLK, Signal::HIGH);
    CHECK(Simulator::sequentialQueue.isEmpty());
    CHECK_EQ(ff.getResult(), Signal::LOW);
}

// ---------------------------------------------------------------------------
// through the simulator
// ---------------------------------------------------------------------------

TEST(dff_p, powers_up_low) {
    DffBench b;
    CHECK_EQ(b.Q(), 0);
}

TEST(dff_p, rising_edge_captures_d) {
    DffBench b;
    Circuit::set(b.d, 1);
    CHECK_EQ(b.Q(), 0);   // D alone does nothing
    Circuit::set(b.clk, 1);
    CHECK_EQ(b.Q(), 1);
}

TEST(dff_p, captures_zero_too) {
    DffBench b;
    Circuit::set(b.d, 1);
    Circuit::pulse(b.clk);
    REQUIRE_EQ(b.Q(), 1);
    Circuit::set(b.d, 0);
    Circuit::pulse(b.clk);
    CHECK_EQ(b.Q(), 0);
}

TEST(dff_p, falling_edge_does_not_capture) {
    DffBench b;
    Circuit::set(b.clk, 1);
    Circuit::set(b.d, 1);
    Circuit::set(b.clk, 0);
    CHECK_EQ(b.Q(), 0);
}

TEST(dff_p, holds_while_clock_high) {
    DffBench b;
    Circuit::set(b.d, 1);
    Circuit::set(b.clk, 1);
    REQUIRE_EQ(b.Q(), 1);
    for (int v : {0, 1, 0, 0}) {
        Circuit::set(b.d, v);
        CHECK_EQ(b.Q(), 1);
    }
}

TEST(dff_p, holds_while_clock_low) {
    DffBench b;
    for (int v : {1, 0, 1, 1, 0, 1}) {
        Circuit::set(b.d, v);
        CHECK_EQ(b.Q(), 0);
    }
}

TEST(dff_p, repeated_clock_edges_with_same_d_keep_q) {
    DffBench b;
    Circuit::set(b.d, 1);
    for (int i = 0; i < 5; i++) {
        Circuit::pulse(b.clk);
        CHECK_EQ(b.Q(), 1);
    }
}

TEST(dff_p, d_change_with_rising_edge_samples_old_d_d_first) {
    // Zero-delay model: an edge and a D change in the same step are a
    // hold-time race; the flip-flop must sample the value from before the step.
    DffBench b;
    Capture err(std::cerr);
    b.d.evaluate(Port::O, Signal::HIGH);
    b.clk.evaluate(Port::O, Signal::HIGH);
    Simulator::run();
    CHECK_EQ(b.Q(), 0);
    CHECK_CONTAINS(err.str(), "Hold-time violation");
}

TEST(dff_p, d_change_with_rising_edge_samples_old_d_clock_first) {
    // same as above with the evaluate() calls swapped
    DffBench b;
    Capture err(std::cerr);
    b.clk.evaluate(Port::O, Signal::HIGH);
    b.d.evaluate(Port::O, Signal::HIGH);
    Simulator::run();
    CHECK_EQ(b.Q(), 0);
    CHECK_CONTAINS(err.str(), "Hold-time violation");
}

TEST(dff_p, d_change_with_rising_edge_keeps_the_new_d_for_the_next_edge) {
    DffBench b;
    Capture err(std::cerr);
    Circuit::setAll({{&b.d, 1}, {&b.clk, 1}});
    REQUIRE_EQ(b.Q(), 0);
    Circuit::set(b.clk, 0);
    Circuit::set(b.clk, 1);
    CHECK_EQ(b.Q(), 1);
}

TEST(dff_p, last_d_change_in_a_step_wins) {
    // several D updates queued before one edge: the edge sees the D from before
    // the step, and the latest update is what the next edge loads
    DffBench b;
    Capture err(std::cerr);
    for (int v : {1, 0, 1}) b.d.evaluate(Port::O, sig(v));
    Simulator::run();
    Circuit::pulse(b.clk);
    CHECK_EQ(b.Q(), 1);
    for (int v : {0, 1, 0}) b.d.evaluate(Port::O, sig(v));
    Simulator::run();
    Circuit::pulse(b.clk);
    CHECK_EQ(b.Q(), 0);
}

TEST(dff_p, repeated_clock_evaluations_in_a_step_act_as_one_edge) {
    // two rising-edge evaluates before the queue drains queue two ChangeStates;
    // the second must not load anything new
    DffBench b;
    Circuit::set(b.d, 1);
    b.ff.evaluate(Port::CLK, Signal::HIGH);
    b.ff.evaluate(Port::CLK, Signal::HIGH);
    Simulator::run();
    CHECK_EQ(b.Q(), 1);
}

TEST(dff_p, glitch_pulse_within_one_step_counts_as_an_edge) {
    // clk 0 -> 1 -> 0 before the simulator runs: the rising edge is still seen
    DffBench b;
    Circuit::set(b.d, 1);
    b.clk.evaluate(Port::O, Signal::HIGH);
    b.clk.evaluate(Port::O, Signal::LOW);
    Simulator::run();
    CHECK_EQ(b.Q(), 1);
}

TEST(dff_p, random_stimulus_matches_reference_model) {
    DffBench b;
    Capture err(std::cerr);   // hold-time warnings are expected here
    int d = 0, clk = 0, q = 0;
    for (int step = 0; step < 2000; step++) {
        int nd = randomBit(), nclk = randomBit();
        if (!clk && nclk) q = d;   // edge samples D from before this step

        // drive in a random order
        if (randomBit()) Circuit::setAll({{&b.d, nd}, {&b.clk, nclk}});
        else Circuit::setAll({{&b.clk, nclk}, {&b.d, nd}});
        d = nd;
        clk = nclk;

        INFO("step " << step << " d=" << d << " clk=" << clk);
        REQUIRE_EQ(b.Q(), q);
    }
}

TEST(dff_p, toggle_with_inverter_feedback) {
    // Q -> NOT -> D: divides the clock by two
    Circuit c;
    auto &clk = c.input("clk");
    auto &ff = c.add<DFF_P>();
    auto &inv = c.add<NOT>();
    auto &q = c.probe(ff, "q");
    Circuit::connect(clk, ff, Port::CLK);
    Circuit::connect(ff, inv, Port::A);
    Circuit::connect(inv, ff, Port::D);
    c.init();

    for (int cycle = 1; cycle <= 10; cycle++) {
        Circuit::pulse(clk);
        INFO("cycle " << cycle);
        CHECK_EQ(Circuit::read(q), cycle % 2);
    }
}

TEST(dff_p, chained_stage_does_not_race_through) {
    // ff1.D = ff0.Q: one edge must move the old ff0 value into ff1, not the new one
    Circuit c;
    auto &d = c.input("d");
    auto &clk = c.input("clk");
    auto &ff0 = c.add<DFF_P>();
    auto &ff1 = c.add<DFF_P>();
    auto &q0 = c.probe(ff0, "q0");
    auto &q1 = c.probe(ff1, "q1");
    Circuit::connect(d, ff0, Port::D);
    Circuit::connect(clk, ff0, Port::CLK);
    Circuit::connect(clk, ff1, Port::CLK);
    Circuit::connect(ff0, ff1, Port::D);
    c.init();

    Circuit::set(d, 1);
    Circuit::pulse(clk);
    CHECK_EQ(Circuit::read(q0), 1);
    CHECK_EQ(Circuit::read(q1), 0);
    Circuit::set(d, 0);
    Circuit::pulse(clk);
    CHECK_EQ(Circuit::read(q0), 0);
    CHECK_EQ(Circuit::read(q1), 1);
}

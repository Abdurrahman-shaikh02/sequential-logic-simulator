// InputNode / OutputNode.

#include "support/helpers.hpp"

using namespace hdltest;

TEST(input_node, change_updates_result_and_fans_out) {
    Simulator::clear();
    InputNode in;
    ProbeNode sink;
    in.addConnection(sink, Port::A);

    in.evaluate(Port::O, Signal::HIGH);
    CHECK_EQ(in.getResult(), Signal::HIGH);
    drain(Simulator::combinationalQueue);
    CHECK_EQ(sink.calls, (std::vector<Call>{{Port::A, Signal::HIGH}}));
}

TEST(input_node, same_value_does_not_fan_out) {
    Simulator::clear();
    InputNode in;
    ProbeNode sink;
    in.addConnection(sink, Port::A);

    in.evaluate(Port::O, Signal::LOW);   // already LOW at power-up
    CHECK(Simulator::combinationalQueue.isEmpty());

    in.evaluate(Port::O, Signal::HIGH);
    in.evaluate(Port::O, Signal::HIGH);
    CHECK_EQ(drain(Simulator::combinationalQueue), 1);
}

TEST(input_node, every_change_fans_out) {
    Simulator::clear();
    InputNode in;
    ProbeNode sink;
    in.addConnection(sink, Port::B);
    for (int v : {1, 0, 1, 0}) in.evaluate(Port::O, sig(v));
    drain(Simulator::combinationalQueue);
    CHECK_EQ(sink.calls, (std::vector<Call>{{Port::B, Signal::HIGH}, {Port::B, Signal::LOW},
                                            {Port::B, Signal::HIGH}, {Port::B, Signal::LOW}}));
}

TEST(input_node, port_argument_is_ignored) {
    Simulator::clear();
    InputNode in;
    in.evaluate(Port::CLK, Signal::HIGH);
    CHECK_EQ(in.getResult(), Signal::HIGH);
}

TEST(input_node, does_not_go_through_the_topological_queue) {
    Simulator::clear();
    InputNode in;
    ProbeNode sink;
    in.addConnection(sink, Port::A);
    in.evaluate(Port::O, Signal::HIGH);
    CHECK(Simulator::topologicalQueue.isEmpty());
    CHECK(!Simulator::combinationalQueue.isEmpty());
}

TEST(output_node, latches_value) {
    Simulator::clear();
    OutputNode out;
    CHECK_EQ(out.getResult(), Signal::LOW);
    out.evaluate(Port::O, Signal::HIGH);
    CHECK_EQ(out.getResult(), Signal::HIGH);
    out.evaluate(Port::O, Signal::LOW);
    CHECK_EQ(out.getResult(), Signal::LOW);
}

TEST(output_node, never_schedules_anything) {
    Simulator::clear();
    OutputNode out;
    ProbeNode sink;
    out.addConnection(sink, Port::A);   // even if something is wired after it
    out.evaluate(Port::O, Signal::HIGH);
    CHECK(Simulator::combinationalQueue.isEmpty());
    CHECK(Simulator::sequentialQueue.isEmpty());
    CHECK(Simulator::topologicalQueue.isEmpty());
}

TEST(primary_nodes, input_drives_output_through_simulator) {
    Circuit c;
    auto &in = c.input("a");
    auto &out = c.probe(in, "y");
    c.init();
    CHECK_EQ(Circuit::read(out), 0);
    for (int v : {1, 1, 0, 1, 0, 0}) {
        INFO("drive " << v);
        Circuit::set(in, v);
        CHECK_EQ(Circuit::read(out), v);
    }
}

TEST(primary_nodes, one_input_fans_out_to_many_outputs) {
    Circuit c;
    auto &in = c.input("a");
    std::vector<OutputNode *> outs;
    for (int i = 0; i < 8; i++) outs.push_back(&c.probe(in, "y" + std::to_string(i)));
    c.init();
    Circuit::set(in, 1);
    for (auto *o : outs) CHECK_EQ(Circuit::read(*o), 1);
    Circuit::set(in, 0);
    for (auto *o : outs) CHECK_EQ(Circuit::read(*o), 0);
}

// Node + List: connections and commit fan-out.

#include "support/helpers.hpp"

#include "operations/EvaluateOperation.hpp"

using namespace hdltest;

TEST(node, starts_low_with_no_priority) {
    ProbeNode n;
    CHECK_EQ(n.getResult(), Signal::LOW);
    CHECK_EQ(n.priority, -1);
    CHECK(connectionsOf(n).empty());
}

TEST(node, add_connection_records_target_and_port) {
    ProbeNode src, dst;
    src.addConnection(dst, Port::B);
    auto c = connectionsOf(src);
    REQUIRE_EQ(c.size(), 1u);
    CHECK(c[0].first == &dst);
    CHECK_EQ(c[0].second, Port::B);
    CHECK(connectionsOf(dst).empty());   // connections are one-way
}

TEST(node, commit_without_connections_enqueues_nothing) {
    Simulator::clear();
    ProbeNode n;
    n.commit(Signal::HIGH);
    CHECK(Simulator::combinationalQueue.isEmpty());
    CHECK(Simulator::sequentialQueue.isEmpty());
}

TEST(node, commit_enqueues_one_evaluate_per_connection) {
    Simulator::clear();
    ProbeNode src, a, b;
    src.addConnection(a, Port::A);
    src.addConnection(b, Port::S);
    src.commit(Signal::HIGH);

    // nothing runs until the queue is drained
    CHECK(a.calls.empty());
    CHECK(b.calls.empty());
    CHECK_EQ(drain(Simulator::combinationalQueue), 2);

    CHECK_EQ(a.calls, (std::vector<Call>{{Port::A, Signal::HIGH}}));
    CHECK_EQ(b.calls, (std::vector<Call>{{Port::S, Signal::HIGH}}));
    CHECK(Simulator::sequentialQueue.isEmpty());   // commits only feed the combinational queue
}

TEST(node, commit_sends_the_given_value_not_the_stored_result) {
    Simulator::clear();
    ProbeNode src, dst;
    src.setResult(Signal::HIGH);
    src.addConnection(dst, Port::A);
    src.commit(Signal::LOW);
    drain(Simulator::combinationalQueue);
    CHECK_EQ(dst.calls, (std::vector<Call>{{Port::A, Signal::LOW}}));
}

TEST(node, same_target_on_two_ports_gets_both_evaluates) {
    Simulator::clear();
    ProbeNode src, dst;
    src.addConnection(dst, Port::A);
    src.addConnection(dst, Port::B);
    src.commit(Signal::HIGH);
    drain(Simulator::combinationalQueue);
    REQUIRE_EQ(dst.calls.size(), 2u);
    CHECK((dst.calls[0].port == Port::A) != (dst.calls[1].port == Port::A));
}

TEST(node, connections_fan_out_in_reverse_insertion_order) {
    // List::addElement prepends, so the newest connection is evaluated first.
    // Correctness must not depend on this; the test pins it so a change is visible.
    Simulator::clear();
    ProbeNode src, log;
    src.addConnection(log, Port::A);
    src.addConnection(log, Port::B);
    src.addConnection(log, Port::S);
    src.commit(Signal::HIGH);
    drain(Simulator::combinationalQueue);
    CHECK_EQ(log.calls, (std::vector<Call>{{Port::S, Signal::HIGH}, {Port::B, Signal::HIGH}, {Port::A, Signal::HIGH}}));
}

TEST(node, every_commit_fans_out_again) {
    Simulator::clear();
    ProbeNode src, dst;
    src.addConnection(dst, Port::A);
    src.commit(Signal::HIGH);
    src.commit(Signal::HIGH);
    src.commit(Signal::LOW);
    drain(Simulator::combinationalQueue);
    CHECK_EQ(dst.calls.size(), 3u);   // Node::commit itself does no change detection
}

TEST(node, large_fanout) {
    Simulator::clear();
    ProbeNode src;
    std::vector<std::unique_ptr<ProbeNode>> sinks;
    for (int i = 0; i < 500; i++) {
        sinks.push_back(std::make_unique<ProbeNode>());
        src.addConnection(*sinks.back(), Port::A);
    }
    src.commit(Signal::HIGH);
    CHECK_EQ(drain(Simulator::combinationalQueue), 500);
    for (auto &s : sinks)
        CHECK_EQ(s->calls.size(), 1u);
}

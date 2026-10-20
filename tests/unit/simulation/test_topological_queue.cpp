// TopologicalQueue: commits pending nodes in ascending priority order.

#include "support/helpers.hpp"

#include "simulation/TopologicalQueue.hpp"

using namespace hdltest;

namespace {

// Each source node drives `log` on its own port, so the order of the evaluate
// calls on `log` is the order in which the sources were committed.
struct Fixture {
    ProbeNode log;
    ProbeNode src[4];
    const Port ports[4] = {Port::A, Port::B, Port::S, Port::D};

    Fixture() {
        Simulator::clear();
        for (int i = 0; i < 4; i++) src[i].addConnection(log, ports[i]);
    }

    std::vector<Port> commitOrder() {
        drain(Simulator::combinationalQueue);
        std::vector<Port> order;
        for (auto &c : log.calls) order.push_back(c.port);
        return order;
    }
};

}  // namespace

TEST(topo_queue, empty_queue) {
    TopologicalQueue q;
    CHECK(q.isEmpty());
    q.commitNext();   // no-op, must not crash
    CHECK(q.isEmpty());
}

TEST(topo_queue, commits_lowest_priority_first) {
    Fixture f;
    TopologicalQueue q;
    f.src[0].priority = 30;
    f.src[1].priority = 10;
    f.src[2].priority = 20;
    f.src[3].priority = 0;
    for (auto &s : f.src) q.enqueue(&s);

    while (!q.isEmpty()) q.commitNext();
    CHECK_EQ(f.commitOrder(), (std::vector<Port>{Port::D, Port::B, Port::S, Port::A}));
}

TEST(topo_queue, commit_next_commits_exactly_one_node) {
    Fixture f;
    TopologicalQueue q;
    f.src[0].priority = 1;
    f.src[1].priority = 2;
    q.enqueue(&f.src[0]);
    q.enqueue(&f.src[1]);
    q.commitNext();
    CHECK(!q.isEmpty());
    CHECK_EQ(f.commitOrder(), (std::vector<Port>{Port::A}));
}

TEST(topo_queue, enqueueing_a_node_twice_commits_it_once) {
    Fixture f;
    TopologicalQueue q;
    f.src[0].priority = 5;
    q.enqueue(&f.src[0]);
    q.enqueue(&f.src[0]);
    q.commitNext();
    CHECK(q.isEmpty());
    CHECK_EQ(f.commitOrder().size(), 1u);
}

TEST(topo_queue, equal_priorities_are_all_kept) {
    Fixture f;
    TopologicalQueue q;
    for (auto &s : f.src) {
        s.priority = 7;
        q.enqueue(&s);
    }
    int commits = 0;
    while (!q.isEmpty()) {
        q.commitNext();
        commits++;
    }
    CHECK_EQ(commits, 4);
    CHECK_EQ(f.commitOrder().size(), 4u);
}

TEST(topo_queue, commits_the_nodes_current_result) {
    Fixture f;
    TopologicalQueue q;
    f.src[0].setResult(Signal::HIGH);
    q.enqueue(&f.src[0]);
    q.commitNext();
    drain(Simulator::combinationalQueue);
    CHECK_EQ(f.log.calls, (std::vector<Call>{{Port::A, Signal::HIGH}}));
}

TEST(topo_queue, node_can_be_requeued_after_commit) {
    Fixture f;
    TopologicalQueue q;
    q.enqueue(&f.src[0]);
    q.commitNext();
    q.enqueue(&f.src[0]);
    CHECK(!q.isEmpty());
    q.commitNext();
    CHECK_EQ(f.commitOrder().size(), 2u);
}

TEST(topo_queue, clear_drops_pending_nodes_without_committing) {
    Fixture f;
    TopologicalQueue q;
    for (auto &s : f.src) q.enqueue(&s);
    q.clear();
    CHECK(q.isEmpty());
    CHECK(f.commitOrder().empty());
}

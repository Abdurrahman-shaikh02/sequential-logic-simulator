// OperationQueue: FIFO behaviour.

#include "support/helpers.hpp"

#include "operations/EvaluateOperation.hpp"
#include "simulation/OperationQueue.hpp"

using namespace hdltest;

namespace {

std::unique_ptr<Operation> eval(Node &n, Port p, int v = 1) {
    return std::make_unique<EvaluateOperation>(n, p, sig(v));
}

}  // namespace

TEST(operation_queue, new_queue_is_empty) {
    OperationQueue q;
    CHECK(q.isEmpty());
    CHECK(q.dequeue() == nullptr);
}

TEST(operation_queue, null_operation_is_ignored) {
    OperationQueue q;
    q.enqueue(nullptr);
    CHECK(q.isEmpty());
}

TEST(operation_queue, dequeues_in_fifo_order) {
    OperationQueue q;
    ProbeNode n;
    q.enqueue(eval(n, Port::A));
    q.enqueue(eval(n, Port::B));
    q.enqueue(eval(n, Port::S));
    CHECK(!q.isEmpty());
    drain(q);
    CHECK(q.isEmpty());
    CHECK_EQ(n.calls, (std::vector<Call>{{Port::A, Signal::HIGH}, {Port::B, Signal::HIGH}, {Port::S, Signal::HIGH}}));
}

TEST(operation_queue, operations_enqueued_while_draining_run_too) {
    OperationQueue q;
    ProbeNode n;
    q.enqueue(eval(n, Port::A));
    auto op = q.dequeue();
    q.enqueue(eval(n, Port::B));
    op->run();
    drain(q);
    CHECK_EQ(n.calls.size(), 2u);
}

TEST(operation_queue, clear_empties_the_queue) {
    OperationQueue q;
    ProbeNode n;
    for (int i = 0; i < 5; i++) q.enqueue(eval(n, Port::A));
    q.clear();
    CHECK(q.isEmpty());
    CHECK(n.calls.empty());
}

// OperationQueue: FIFO behaviour and the hold-time reorder().

#include <algorithm>

#include "support/helpers.hpp"

#include "operations/ChangeStateOperation.hpp"
#include "operations/CommitOperation.hpp"
#include "operations/EvaluateOperation.hpp"
#include "operations/SetPortOperation.hpp"
#include "simulation/OperationQueue.hpp"

using namespace hdltest;

namespace {

std::unique_ptr<Operation> eval(Node &n, Port p, int v = 1) {
    return std::make_unique<EvaluateOperation>(n, p, sig(v));
}
std::unique_ptr<Operation> setPort(Node &n, Port p = Port::D, int v = 1) {
    return std::make_unique<SetPortOperation>(n, p, sig(v));
}
std::unique_ptr<Operation> changeState(Node &n) {
    return std::make_unique<ChangeStateOperation>(n, Port::CLK, Signal::HIGH);
}
std::unique_ptr<Operation> commit(Node &n) {
    return std::make_unique<CommitOperation>(n, Port::O, Signal::HIGH);
}

// Describes the queue as "kind@node" entries, emptying it. Nodes are named by
// their index in `nodes`.
std::vector<std::string> describe(OperationQueue &q, const std::vector<Node *> &nodes) {
    std::vector<std::string> out;
    while (auto op = q.dequeue()) {
        std::string kind = dynamic_cast<SetPortOperation *>(op.get())       ? "set"
                           : dynamic_cast<ChangeStateOperation *>(op.get()) ? "change"
                           : dynamic_cast<CommitOperation *>(op.get())      ? "commit"
                                                                            : "eval";
        int idx = -1;
        for (size_t i = 0; i < nodes.size(); i++)
            if (nodes[i] == &op->getTarget()) idx = static_cast<int>(i);
        out.push_back(kind + "@" + std::to_string(idx));
    }
    return out;
}

using Strings = std::vector<std::string>;

}  // namespace

// ---------------------------------------------------------------------------
// basic queue behaviour
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// reorder: CHANGE_STATE must run before SET_PORT on the same flip-flop
// ---------------------------------------------------------------------------

TEST(operation_queue, reorder_moves_change_state_before_set_port_on_same_ff) {
    OperationQueue q;
    ProbeSeqNode ff;
    q.enqueue(setPort(ff));
    q.enqueue(changeState(ff));
    Capture err(std::cerr);
    q.reorder();
    CHECK_EQ(describe(q, {&ff}), (Strings{"change@0", "set@0"}));
    CHECK_CONTAINS(err.str(), "Hold-time violation");
}

TEST(operation_queue, reorder_leaves_correct_order_alone) {
    OperationQueue q;
    ProbeSeqNode ff;
    q.enqueue(changeState(ff));
    q.enqueue(setPort(ff));
    Capture err(std::cerr);
    q.reorder();
    CHECK_EQ(describe(q, {&ff}), (Strings{"change@0", "set@0"}));
}

TEST(operation_queue, reorder_ignores_different_flip_flops) {
    OperationQueue q;
    ProbeSeqNode a, b;
    q.enqueue(setPort(a));
    q.enqueue(changeState(b));
    Capture err(std::cerr);
    q.reorder();
    CHECK_EQ(describe(q, {&a, &b}), (Strings{"set@0", "change@1"}));
    CHECK_EQ(err.str(), "");   // no violation, no warning
}

TEST(operation_queue, reorder_on_empty_queue_is_a_noop) {
    OperationQueue q;
    Capture err(std::cerr);
    q.reorder();
    CHECK(q.isEmpty());
    CHECK_EQ(err.str(), "");
}

TEST(operation_queue, reorder_handles_many_flip_flops) {
    // every FF has its D update queued before its clock edge
    OperationQueue q;
    ProbeSeqNode ff[4];
    std::vector<Node *> nodes = {&ff[0], &ff[1], &ff[2], &ff[3]};
    for (auto &f : ff) q.enqueue(setPort(f));
    for (auto &f : ff) q.enqueue(changeState(f));
    Capture err(std::cerr);
    q.reorder();
    auto order = describe(q, nodes);

    // each FF's change must come before its set
    for (int i = 0; i < 4; i++) {
        INFO("ff " << i << ", order " << toString(order));
        auto pos = [&](const std::string &s) { return std::find(order.begin(), order.end(), s) - order.begin(); };
        CHECK_LT(pos("change@" + std::to_string(i)), pos("set@" + std::to_string(i)));
    }
    CHECK_EQ(order.size(), 8u);
}

TEST(operation_queue, reorder_keeps_commits_in_the_queue) {
    OperationQueue q;
    ProbeSeqNode ff;
    q.enqueue(commit(ff));
    q.enqueue(setPort(ff));
    q.enqueue(changeState(ff));
    Capture err(std::cerr);
    q.reorder();
    auto order = describe(q, {&ff});
    CHECK_EQ(order.size(), 3u);
    CHECK_EQ(std::count(order.begin(), order.end(), "commit@0"), 1);
}

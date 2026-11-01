// Operation subclasses: each one must call exactly the right hook on its target.

#include <typeinfo>

#include "support/helpers.hpp"

#include "operations/ChangeStateOperation.hpp"
#include "operations/CommitOperation.hpp"
#include "operations/EvaluateOperation.hpp"

using namespace hdltest;

TEST(operations, evaluate_calls_evaluate_with_port_and_value) {
    ProbeNode n;
    EvaluateOperation(n, Port::S, Signal::HIGH).run();
    EvaluateOperation(n, Port::A, Signal::LOW).run();
    CHECK_EQ(n.calls, (std::vector<Call>{{Port::S, Signal::HIGH}, {Port::A, Signal::LOW}}));
}

TEST(operations, change_state_calls_change_state) {
    ProbeSeqNode ff;
    ChangeStateOperation(ff, Port::CLK, Signal::HIGH).run();
    CHECK_EQ(ff.log, (std::vector<std::string>{"changeState"}));
}

TEST(operations, commit_fans_out_the_targets_current_result) {
    // CommitOperation ignores its stored value and commits target.getResult()
    Simulator::clear();
    ProbeNode src, dst;
    src.addConnection(dst, Port::A);
    src.setResult(Signal::HIGH);
    CommitOperation(src, Port::O, Signal::LOW).run();
    drain(Simulator::combinationalQueue);
    CHECK_EQ(dst.calls, (std::vector<Call>{{Port::A, Signal::HIGH}}));
}

TEST(operations, sequential_ops_on_combinational_node_throw_bad_cast) {
    ProbeNode n;
    CHECK_THROWS(ChangeStateOperation(n, Port::CLK, Signal::HIGH).run());
}

TEST(operations, get_target_returns_the_node) {
    ProbeNode n;
    EvaluateOperation op(n, Port::A, Signal::HIGH);
    CHECK(&op.getTarget() == &n);
}

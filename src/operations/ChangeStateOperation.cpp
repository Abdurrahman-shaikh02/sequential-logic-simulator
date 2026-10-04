#include "operations/ChangeStateOperation.hpp"
#include "sequential/SequentialNode.hpp"

ChangeStateOperation::ChangeStateOperation(Node &target, Port p, Signal value)
    : Operation(target, p, value) {}

void ChangeStateOperation::run() {
    dynamic_cast<SequentialNode &>(target).changeState();
}

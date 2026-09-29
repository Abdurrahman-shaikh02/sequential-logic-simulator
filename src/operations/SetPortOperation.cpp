#include "operations/SetPortOperation.hpp"
#include "sequential/SequentialNode.hpp"

SetPortOperation::SetPortOperation(Node &target, Port p, Signal value)
    : Operation(target, p, value) {}

void SetPortOperation::run() {
    dynamic_cast<SequentialNode &>(target).setPort(p, value);
}

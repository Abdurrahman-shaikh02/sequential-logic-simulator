#include "operations/ChangeStateOperation.hpp"

ChangeStateOperation::ChangeStateOperation(Node &target, Port p, Signal value)
    : Operation(target, p, value) {}

void ChangeStateOperation::run() {
    // Calculates internal next state without updating output Q pin yet
    target.commit();
}
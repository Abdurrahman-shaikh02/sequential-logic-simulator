#include "operations/EvaluateOperation.hpp"

EvaluateOperation::EvaluateOperation(Node &target, Port p, Signal value)
    : Operation(target, p, value) {}

void EvaluateOperation::run() {
    // Recalculates output based on current input pin values
    target.evaluate(p, value);
}

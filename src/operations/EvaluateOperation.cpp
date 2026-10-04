#include "operations/EvaluateOperation.hpp"

EvaluateOperation::EvaluateOperation(Node &target, Port p, Signal value)
    : Operation(target, p, value) {}

void EvaluateOperation::run() {
    target.evaluate(p, value);
}

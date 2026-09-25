#include "operations/SetPortOperation.hpp"

SetPortOperation::SetPortOperation(Node &target, Port p, Signal value)
    : Operation(target, p, value) {}

void SetPortOperation::run() {
    // Sets the pin signal value on the target node
    target.evaluate(p, value);
}
#include "operations/CommitOperation.hpp"

CommitOperation::CommitOperation(Node &target, Port p, Signal value)
    : Operation(target, p, value) {}

void CommitOperation::run() {
    // Flushes calculated internal state to output pin 
    target.commit();
}
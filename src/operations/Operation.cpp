#include "operations/Operation.hpp"

Operation::Operation(Node& target, Port p, Signal value)
    : target(target), p(p), value(value) {}

#include "operations/Operation.hpp"

Operation::Operation(Node& target, Port p, Signal value)
    : target(target), p(p), value(value) {}

bool Operation::operator==(const Operation &op) {
    return (&this->target == &op.target) && 
           (this->p == op.p);
}

#pragma once

#include "core/Node.hpp"

// Marker base for primary I/O nodes. No extra state/behavior of its own —
// stays abstract since Node::evaluate() is still unimplemented.
// evaluate() on these is never queued as an EvaluateOperation; the
// Parser/Simulator call it directly to drive inputs / read outputs.
class PrimaryNode : public Node {
};

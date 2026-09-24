#include "primary/InputNode.hpp"

void InputNode::evaluate(Port p, Signal value) {
	(void)p; // input node has a single driven value, port is unused
	result = value;
	commit();
}

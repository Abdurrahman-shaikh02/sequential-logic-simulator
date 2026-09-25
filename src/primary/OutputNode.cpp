#include "primary/OutputNode.hpp"

void OutputNode::evaluate(Port p, Signal value) {
	(void)p; // output node just latches whatever reaches it
	result = value;
}

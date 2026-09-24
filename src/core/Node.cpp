#include "core/Node.hpp"

Node::Node() : result(Signal::LOW) {}

void Node::add_connection(Node &node, Port p) {
	connections.addElement(node, p);
}

Signal Node::get_result() {
	return result;
}

void Node::commit() {
	connections.loopAndEvaluate(result);
}

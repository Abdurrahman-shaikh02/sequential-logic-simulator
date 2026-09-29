#include "core/Node.hpp"

Node::Node() : result(Signal::LOW) {}

void Node::addConnection(Node &node, Port p) {
	connections.addElement(node, p);
}

Signal Node::getResult() {
	return result;
}

void Node::commit(Signal value) {
	connections.loopAndEvaluate(value);
}

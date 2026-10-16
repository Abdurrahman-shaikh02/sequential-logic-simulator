#include "core/Node.hpp"

Node::Node() : result(Signal::LOW), initialized(false) {}

void Node::addConnection(Node &node, Port p) {
	connections.addElement(node, p);
}

Signal Node::getResult() {
	return result;
}

void Node::commit(Signal value) {
	initialized = true;
	connections.loopAndEvaluate(value);
}

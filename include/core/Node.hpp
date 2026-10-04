#pragma once

#include "core/List.hpp"
#include "core/Signal.hpp"
#include "core/Port.hpp"

class Node {
private:
	List connections;
protected:
	Signal result;
	Node();
public:
	void addConnection(Node &node, Port p);
	Signal getResult();
	void commit(Signal value);
	virtual void evaluate(Port p, Signal value) = 0;
};

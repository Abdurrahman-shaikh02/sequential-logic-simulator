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
	void add_connection(Node &node, Port p);
	Signal get_result();
	void commit();
	virtual void evaluate(Port p, Signal value) = 0;
};

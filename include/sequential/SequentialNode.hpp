#pragma once

#include "core/Port.hpp"
#include "core/Signal.hpp"
#include "core/Node.hpp"

class SequentialNode : public Node {
	virtual void setPort(Port p, Signal value) = 0;
	virtual void changeState() = 0;
};

#pragma once

#include <queue>
#include <memory>
#include <iostream>

#include "operations/Operation.hpp"
#include "operations/SetPortOperation.hpp"
#include "operations/ChangeStateOperation.hpp"

class OperationQueue {
private:
	// why deque? it allows iteration, swapping(reorder) and cancel operations easily!
	std::deque<std::unique_ptr<Operation>> q;
public:
	OperationQueue() = default;

	void enqueue(std::unique_ptr<Operation> op);
	std::unique_ptr<Operation> dequeue();
	
	bool cancel(const Operation &op);
	bool isEmpty() const {
		return q.empty();
	}

	void clear() {
		q.clear();
	}

	// it will detect if the same ff has both set port and update state
	// if it does -> issue warning then reorder the queue. (all change_states first... then port)
	//obviously dequeueing will automatically cause the commits to be at the end
	void reorder();		
};

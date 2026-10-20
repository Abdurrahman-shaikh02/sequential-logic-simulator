#pragma once

#include <queue>
#include <memory>

#include "operations/Operation.hpp"

class OperationQueue {
private:
	// why deque? it allows iteration and swapping (reorder) easily!
	std::deque<std::unique_ptr<Operation>> q;
public:
	OperationQueue() = default;

	void enqueue(std::unique_ptr<Operation> op);

	std::unique_ptr<Operation> dequeue();
	
	bool isEmpty() const {
		return q.empty();
	}

	void clear() {
		q.clear();
	}

	void reorder();		
};

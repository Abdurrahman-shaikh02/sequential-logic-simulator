#include "simulation/TopologicalQueue.hpp"

void TopologicalQueue::enqueue(Node* node) {
    queue.insert(node);
}

bool TopologicalQueue::isEmpty() const {
    return queue.empty();
}

void TopologicalQueue::commitNext() {
    if (queue.empty())
        return;

    Node* x = *queue.begin();
    queue.erase(queue.begin());

    x->commit(x->getResult());
}

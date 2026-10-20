#pragma once

#include <set>
#include <functional>
#include "core/Node.hpp"

class TopologicalQueue {
private:
    struct Compare {
        bool operator()(const Node* a, const Node* b) const {
            if (a->priority != b->priority)
                return a->priority < b->priority;

            return std::less<const Node*>{}(a, b);
        }
    };

    std::set<Node*, Compare> queue;

public:
    void enqueue(Node* node);

    bool isEmpty() const;

    void commitNext();

    void clear();
};

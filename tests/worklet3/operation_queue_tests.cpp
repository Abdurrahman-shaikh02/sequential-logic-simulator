#include <iostream>
#include <memory>

#include "core/Node.hpp"
#include "operations/EvaluateOperation.hpp"
#include "operations/SetPortOperation.hpp"
#include "operations/ChangeStateOperation.hpp"
#include "sequential/SequentialNode.hpp"
#include "simulation/OperationQueue.hpp"

class TestNode : public Node {
public:
    void evaluate(Port, Signal) override {}
};

class TestSequentialNode : public SequentialNode {
public:
    void evaluate(Port, Signal) override {}
    void setPort(Port, Signal) override {}
    void changeState() override {}
};

int passed = 0;
int failed = 0;

void check(bool condition, const char* message) {
    if (condition) {
        std::cout << "[PASS] " << message << '\n';
        ++passed;
    } else {
        std::cerr << "[FAIL] " << message << '\n';
        ++failed;
    }
}

int main() {
    // Test 1: New queue is empty
    {
        OperationQueue queue;
        check(queue.isEmpty(), "New queue is empty");
        check(queue.dequeue() == nullptr,
              "Dequeue from empty queue returns null");
    }

    // Test 2: FIFO ordering
    {
        OperationQueue queue;
        TestNode first, second;

        queue.enqueue(std::make_unique<EvaluateOperation>(
            first, Port::A, Signal::LOW));
        queue.enqueue(std::make_unique<EvaluateOperation>(
            second, Port::B, Signal::HIGH));

        auto op1 = queue.dequeue();
        auto op2 = queue.dequeue();

        check(op1 && &op1->getTarget() == &first,
              "First enqueued operation is dequeued first");
        check(op2 && &op2->getTarget() == &second,
              "Second enqueued operation is dequeued second");
        check(queue.isEmpty(), "Queue is empty after both operations");
    }

    // Test 3: Null operation is ignored
    {
        OperationQueue queue;
        queue.enqueue(nullptr);
        check(queue.isEmpty(), "Null operation is ignored");
    }

    // Test 4: Clear queue
    {
        OperationQueue queue;
        TestNode node;

        queue.enqueue(std::make_unique<EvaluateOperation>(
            node, Port::A, Signal::LOW));
        queue.enqueue(std::make_unique<EvaluateOperation>(
            node, Port::B, Signal::HIGH));

        queue.clear();

        check(queue.isEmpty(), "Clear removes queued operations");
        check(queue.dequeue() == nullptr,
              "Dequeue after clear returns null");
    }

    // Test 5: Cancel all operations matching the same node and port
    {
        OperationQueue queue;
        TestNode node;

        queue.enqueue(std::make_unique<EvaluateOperation>(
            node, Port::A, Signal::LOW));
        queue.enqueue(std::make_unique<EvaluateOperation>(
            node, Port::A, Signal::HIGH));
        queue.enqueue(std::make_unique<EvaluateOperation>(
            node, Port::B, Signal::HIGH));

        EvaluateOperation pending(node, Port::A, Signal::HIGH);
        queue.cancel(pending);

        auto remaining = queue.dequeue();

        check(remaining && !queue.isEmpty() == false,
              "Cancellation leaves exactly one operation");
        check(remaining && &remaining->getTarget() == &node,
              "Remaining operation targets the expected node");

        EvaluateOperation pendingB(node, Port::B, Signal::LOW);
        queue.cancel(pendingB);

        check(queue.isEmpty(),
              "Cancellation matches node and port regardless of signal");
    }

    // Test 6: Cancellation does not affect another node
    {
        OperationQueue queue;
        TestNode first, second;

        queue.enqueue(std::make_unique<EvaluateOperation>(
            first, Port::A, Signal::LOW));
        queue.enqueue(std::make_unique<EvaluateOperation>(
            second, Port::A, Signal::HIGH));

        EvaluateOperation pending(first, Port::A, Signal::HIGH);
        queue.cancel(pending);

        auto remaining = queue.dequeue();

        check(remaining && &remaining->getTarget() == &second,
              "Other node's operation remains queued");
        check(queue.isEmpty(), "Only the unrelated operation remains");
    }

    // Test 7: ChangeState precedes SetPort for the same node
    {
        OperationQueue queue;
        TestSequentialNode node;

        queue.enqueue(std::make_unique<SetPortOperation>(
            node, Port::D, Signal::HIGH));
        queue.enqueue(std::make_unique<ChangeStateOperation>(
            node, Port::CLK, Signal::HIGH));

        queue.reorder();

        auto first = queue.dequeue();
        auto second = queue.dequeue();

        check(first &&
              dynamic_cast<ChangeStateOperation*>(first.get()) != nullptr,
              "ChangeState is first after reordering");
        check(second &&
              dynamic_cast<SetPortOperation*>(second.get()) != nullptr,
              "SetPort is second after reordering");
    }

    // Test 8: Different nodes are not reordered
    {
        OperationQueue queue;
        TestSequentialNode firstNode, secondNode;

        queue.enqueue(std::make_unique<SetPortOperation>(
            firstNode, Port::D, Signal::HIGH));
        queue.enqueue(std::make_unique<ChangeStateOperation>(
            secondNode, Port::CLK, Signal::HIGH));

        queue.reorder();

        auto first = queue.dequeue();
        auto second = queue.dequeue();

        check(first &&
              dynamic_cast<SetPortOperation*>(first.get()) != nullptr,
              "SetPort for first node keeps its position");
        check(second &&
              dynamic_cast<ChangeStateOperation*>(second.get()) != nullptr,
              "ChangeState for second node keeps its position");
    }

    std::cout << "\nResults: " << passed << " passed, "
              << failed << " failed.\n";

    return failed == 0 ? 0 : 1;
}

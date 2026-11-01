#include "simulation/OperationQueue.hpp"

/* pushes an operation at the back of the queue
 * uses std::move(). See online, how it works :)
 * in short : we used unique_ptr which allows only one pointer
 * to own that memory address at any given time. 
 * Because of this we can't copy unique_ptr -> compilation error.
 * so std::unique_ptr<Operation> op2 = std::move(op1);
 * 1. op2 takes over the memory address of the operation object (op1).
 * 2. op1 is automatically set to nullptr (it becomes empty).
 * std::move() is also extremely fast i.e. O(1) TC
 */
void OperationQueue::enqueue(std::unique_ptr<Operation> op) {
    // needs a STRICTLY DYNAMICALLY allocated object of type Operation
    if(op) {
        q.push_back(std::move(op));
    }
}

std::unique_ptr<Operation> OperationQueue::dequeue() {
    if (q.empty()) 
        return nullptr;
 
    std::unique_ptr<Operation> op = std::move(q.front());
    q.pop_front();
    return op;
}

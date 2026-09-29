#include <iostream>
#include "simulation/OperationQueue.hpp"
#include "operations/SetPortOperation.hpp"
#include "operations/ChangeStateOperation.hpp"

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

void OperationQueue::cancel(const Operation &op) {
    for (auto it = q.begin(); it != q.end(); ++it) {
        if (**it == op) { // Uses Operation::operator==
            q.erase(it);
        }
    }
}

/* goes in a loop and checks if 
 * set_port appears before a change_state
 * for the exact same node. If so, give warning and swaps them
 */
void OperationQueue::reorder() {
    // ok represents if there is set_port before change_state on exact same node
    bool ok = false;

    for(int i = 0; i < q.size(); i++) {
        auto* setPortOp = dynamic_cast<SetPortOperation*>(q[i].get());

        if(!setPortOp) 
            continue;
        
        for(int j = i + 1; j < q.size(); j++) {
            auto* changeStateOp = dynamic_cast<ChangeStateOperation*>(q[j].get());
            if (!changeStateOp) 
                continue;

            // check : same exact node? via memory addresses
            if(&setPortOp->getTarget() == &changeStateOp->getTarget()) {
                if(!ok) {
                    std::cerr << "[WARNING] Timing hazard detected: SET_PORT scheduled before "
                              << "CHANGE_STATE on the same Flip-Flop! Reordering operations.\n";
                    
                    ok = true;
                }

                // swap so CHANGE_STATE executes first
                std::swap(q[i], q[j]);
                break;
            }
        }
    }
}

#include "operations/EvaluateOperation.hpp"
#include "simulation/Simulator.hpp"
#include "core/List.hpp"
#include "core/Node.hpp"
#include "operations/Operation.hpp"
#include <memory>

List::ListNode::ListNode(Node &target, Port p, ListNode *next)
	: target(target), p(p), next(next)
{
}
 

List::List() : head(nullptr)
{
}

void List::addElement(Node &node, Port p)
{
	head = new ListNode(node, p, head);
}

void List::loopAndEvaluate(Signal value){
	ListNode *temp = head;
	while(temp){
		//enqueue the operation
		std::unique_ptr<Operation> op = std::make_unique<EvaluateOperation>(temp->target, temp->p, value);
		Simulator::combinationalQueue.enqueue(std::move(op));

		temp = temp->next;
	}
}


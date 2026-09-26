
#include "core/List.hpp"
#include "core/Node.hpp"

List::List() : head(nullptr)
{
}

void List::addElement(Node &node, Port p)
{
	head = new ListNode(node, p, head);
}

void List::loopAndEvaluate(Signal value)
{
	ListNode *current = head;
 
	while (current != nullptr)
	{
		current->target.evaluate(current->p, value);
		current = current->next;
	}
}
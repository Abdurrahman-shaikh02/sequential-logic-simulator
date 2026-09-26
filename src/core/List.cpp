
#include "core/List.hpp"
#include "core/Node.hpp"

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


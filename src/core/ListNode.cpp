#include "core/List.hpp"

List::ListNode::ListNode(Node &target, Port p, ListNode *next)
	: target(target), p(p), next(next)
{
}
 
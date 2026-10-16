#pragma once

#include "core/Port.hpp"
#include "core/Signal.hpp"

class Node;
struct ParserTestAccess;

class List {
	friend struct ParserTestAccess; // test-only read access to the list
	class ListNode {
	public:
		Node &target;
		Port p;
		ListNode *next;

		ListNode(Node &target, Port p, ListNode *next);
	};
private:
	ListNode *head;
public:
	List();
	void addElement(Node &node, Port p);
	void loopAndEvaluate(Signal value);
};
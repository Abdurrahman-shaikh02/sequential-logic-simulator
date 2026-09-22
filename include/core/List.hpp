#pragma once

#include "core/Port.hpp"
#include "core/Signal.hpp"

class Node;

class List {
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



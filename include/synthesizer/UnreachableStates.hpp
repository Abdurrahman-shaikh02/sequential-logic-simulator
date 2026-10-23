#pragma once

#include <string>
#include <vector>

class UnreachableStates {
public:
	// Finds the states of the FSM in `path` (format: docs/fsm.net) that no
	// sequence of inputs leads to from the reset state. Only rows written in
	// the table are considered.
	// Fills `states` (in order of first appearance in the table).
	// print == true also writes a report to std::cout.
	// On error, writes to std error and returns false.
	static bool find(const std::string &path, std::vector<std::string> &states, bool print = false);
};

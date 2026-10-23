#pragma once

#include <string>
#include <vector>

class DeadStates {
public:
	// Finds the dead (trap) states of the FSM in `path` (format: docs/fsm.net):
	// states the FSM can never leave once entered, because every transition
	// listed for them goes back to themselves or none is listed. Only rows
	// written in the table are considered.
	// Fills `states` (reset state first, then in order of first appearance).
	// print == true also writes a report to std::cout.
	// On error, writes to std error and returns false.
	static bool find(const std::string &path, std::vector<std::string> &states, bool print = false);
};

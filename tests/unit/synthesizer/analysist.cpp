#include "synthesizer/DeadStates.hpp"
#include "synthesizer/UnreachableStates.hpp"
#include <iostream>
#include <string>
#include <vector>

// Runs dead/unreachable state detection on docs/fsm_example/dead_unreachable_fsm.net
// (run from the repo root), first printing the reports, then using the returned states.

int main(){
	std::string fsm = "docs/fsm_example/dead_unreachable_fsm.net";
	std::vector<std::string> dead, unreachable;

	// print mode
	if(!DeadStates::find(fsm, dead, true) || !UnreachableStates::find(fsm, unreachable, true))
		return 1;

	// return mode: nothing printed by the functions
	DeadStates::find(fsm, dead);
	UnreachableStates::find(fsm, unreachable);

	std::cout << "returned dead:";
	for(auto &s : dead) std::cout << ' ' << s;
	std::cout << "\nreturned unreachable:";
	for(auto &s : unreachable) std::cout << ' ' << s;
	std::cout << std::endl;
}

#include "synthesizer/Synthesizer.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <vector>

// Synthesizes docs/fsm_example/traffic_light_fsm.net (run from the repo root),
// then evaluates the generated netlist directly, one clock cycle per input line.
// stdin:  "reset en" per cycle
// stdout: "R Y G" before that cycle's clock edge

struct Cell {
	std::string type;
	std::map<std::string, std::string> port;	// port name -> wire
};

int main(){
	std::string table = Synthesizer::synthesize("docs/fsm_example/traffic_light_fsm.net");
	if(table.empty())
		return 1;

	// read netlist
	std::map<std::string, Cell> cells;
	std::map<std::string, std::string> inputs, outputs;	// primary name -> wire
	std::ifstream file(table);
	std::string line;
	while(std::getline(file, line)){
		std::istringstream iss(line.substr(0, line.find('#')));
		std::vector<std::string> t;
		std::string tok;
		while(iss >> tok)
			t.push_back(tok);
		if(t.empty())
			continue;
		if(t[4] == "pi") inputs[t[1]] = t[5];
		else if(t[4] == "po") outputs[t[1]] = t[5];
		else {
			cells[t[1]].type = t[2];
			cells[t[1]].port[t[3]] = t[5];
		}
	}

	std::map<std::string, bool> wire;	// flip-flop Q wires start at 0
	int reset = 0, en = 0;
	while(scanf("%d %d", &reset, &en) != EOF){
		wire[inputs["reset"]] = reset;
		wire[inputs["en"]] = en;

		// settle combinational logic (acyclic, so one pass per cell is enough)
		for(size_t pass = 0; pass < cells.size(); pass++){
			for(auto &[name, c] : cells){
				if(c.type == "$dff_p")
					continue;
				bool a = wire[c.port["A"]];
				if(c.type == "$not") wire[c.port["Y"]] = !a;
				else if(c.type == "$and") wire[c.port["Y"]] = a && wire[c.port["B"]];
				else if(c.type == "$or") wire[c.port["Y"]] = a || wire[c.port["B"]];
			}
		}

		std::cout << wire[outputs["R"]] << " " << wire[outputs["Y"]] << " " << wire[outputs["G"]] << std::endl;

		// clock edge: every flip-flop loads D at once
		std::map<std::string, bool> next;
		for(auto &[name, c] : cells)
			if(c.type == "$dff_p")
				next[c.port["Q"]] = wire[c.port["D"]];
		for(auto &[q, v] : next)
			wire[q] = v;
	}
}

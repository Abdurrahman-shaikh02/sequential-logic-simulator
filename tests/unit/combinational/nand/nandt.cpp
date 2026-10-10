#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "combinational/NAND.hpp"
#include <iostream>
#include <memory>
#include "core/Node.hpp"

int main(){
	// make all nodes
	Simulator::nodes.push_back(std::make_unique<InputNode>());
	Simulator::nodes.push_back(std::make_unique<InputNode>());
	Simulator::nodes.push_back(std::make_unique<NAND>());
	Simulator::nodes.push_back(std::make_unique<OutputNode>());

	// make all required connections
	Simulator::nodes[0]->addConnection(*Simulator::nodes[2], Port::A);
	Simulator::nodes[1]->addConnection(*Simulator::nodes[2], Port::B);
	Simulator::nodes[2]->addConnection(*Simulator::nodes[3], Port::O);

	// Prime both inputs with HIGH so the first real value (which may
	// be LOW, the compile-time default) is always treated as a change
	// by InputNode::evaluate()'s guard, and always gets propagated.
	static_cast<InputNode *>(Simulator::nodes[0].get())->evaluate(Port::O, Signal::HIGH);
	static_cast<InputNode *>(Simulator::nodes[1].get())->evaluate(Port::O, Signal::HIGH);

	int a = 0, b = 0, c = 0, d = 0;
	while(scanf("%d %d %d %d", &a, &b, &c, &d) != EOF){
		// NAND only uses a and b; c, d are read to stay aligned
		// with input.txt's 4-column format but are otherwise unused.
		Signal aa = (a == 0) ? Signal::LOW : Signal::HIGH;
		Signal bb = (b == 0) ? Signal::LOW : Signal::HIGH;

		// test inputs
		static_cast<InputNode *>(Simulator::nodes[0].get())->evaluate(Port::O, aa);
		static_cast<InputNode *>(Simulator::nodes[1].get())->evaluate(Port::O, bb);

		//simulate
		Simulator::run();

		// examine outputs
		std::cout << static_cast<int>(Simulator::nodes[3]->getResult()) << std::endl;
	}
}

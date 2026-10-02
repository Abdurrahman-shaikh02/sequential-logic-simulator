#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "combinational/NOT.hpp"
#include <iostream>
#include <memory>
#include "core/Node.hpp"

int main(){
	// make all nodes
	Simulator::nodes.push_back(std::make_unique<InputNode>());
	Simulator::nodes.push_back(std::make_unique<NOT>());
	Simulator::nodes.push_back(std::make_unique<OutputNode>());

	// make all required connections
	Simulator::nodes[0]->addConnection(*Simulator::nodes[1], Port::A);
	Simulator::nodes[1]->addConnection(*Simulator::nodes[2], Port::O);

	// Prime the input with HIGH so the first real value (which may be
	// LOW, the compile-time default) is always treated as a change by
	// InputNode::evaluate()'s "if(value == result) return;" guard, and
	// therefore always gets committed/propagated through the circuit.
	static_cast<InputNode *>(Simulator::nodes[0].get())->evaluate(Port::O, Signal::HIGH);

	int a = 0, b = 0, c = 0, d = 0;
	while(scanf("%d %d %d %d", &a, &b, &c, &d) != EOF){
		// NOT only has one input; b, c, d are read to stay aligned
		// with input.txt's 4-column format but are otherwise unused.
		Signal aa = (a == 0) ? Signal::LOW : Signal::HIGH;

		// test inputs
		static_cast<InputNode *>(Simulator::nodes[0].get())->evaluate(Port::O, aa);

		//simulate
		Simulator::run();

		// examine outputs
		std::cout << static_cast<int>(Simulator::nodes[2]->getResult()) << std::endl;
	}
}
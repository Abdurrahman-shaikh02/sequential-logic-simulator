#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "sequential/DFF_P.hpp"
#include <iostream>
#include <memory>
#include "core/Node.hpp"

int main(){
	// make all nodes
	Simulator::nodes.push_back(std::make_unique<InputNode>());
	Simulator::nodes.push_back(std::make_unique<InputNode>());
	Simulator::nodes.push_back(std::make_unique<DFF_P>());
	Simulator::nodes.push_back(std::make_unique<OutputNode>());

	// make all required connections
	Simulator::nodes[0]->addConnection(*Simulator::nodes[2], Port::D);
	Simulator::nodes[1]->addConnection(*Simulator::nodes[2], Port::CLK);
	Simulator::nodes[2]->addConnection(*Simulator::nodes[3], Port::CLK);

	int a = 0, b = 0;
	while(scanf("%d %d", &a, &b) != EOF){
		Signal aa = Signal::LOW, bb = Signal::LOW;
		aa = (a == 0) ? Signal::LOW : Signal::HIGH;
		bb = (b == 0) ? Signal::LOW : Signal::HIGH;

		// test inputs
		static_cast<InputNode *>(Simulator::nodes[0].get())->evaluate(Port::O, aa);
		static_cast<InputNode *>(Simulator::nodes[1].get())->evaluate(Port::O, bb);

		//simulate
		Simulator::run();

		// examine outputs
		std::cout << static_cast<int>(Simulator::nodes[3]->getResult()) << std::endl;
	}
}

#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "combinational/OR.hpp"
#include <iostream>
#include <memory>
#include "core/Node.hpp"

int main(){
	// make all nodes
	Simulator::nodes.push_back(std::make_unique<InputNode>());
	Simulator::nodes.push_back(std::make_unique<InputNode>());
	Simulator::nodes.push_back(std::make_unique<OR>());
	Simulator::nodes.push_back(std::make_unique<OutputNode>());

	// make all required connections
	Simulator::nodes[0]->addConnection(*Simulator::nodes[2], Port::A);
	Simulator::nodes[1]->addConnection(*Simulator::nodes[2], Port::B);
	Simulator::nodes[2]->addConnection(*Simulator::nodes[3], Port::O);

	int a = 0, b = 0;
	while(scanf("%d %d", &a, &b) != EOF){
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
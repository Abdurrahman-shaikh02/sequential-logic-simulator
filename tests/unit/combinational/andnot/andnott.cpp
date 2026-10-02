#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "combinational/ANDNOT.hpp"
#include <iostream>
#include <memory>
#include "core/Node.hpp"

int main(){
	// make all nodes
	Simulator::nodes.push_back(std::make_unique<InputNode>());
	Simulator::nodes.push_back(std::make_unique<InputNode>());
	Simulator::nodes.push_back(std::make_unique<InputNode>());
	Simulator::nodes.push_back(std::make_unique<InputNode>());
	Simulator::nodes.push_back(std::make_unique<ANDNOT>());
	Simulator::nodes.push_back(std::make_unique<ANDNOT>());
	Simulator::nodes.push_back(std::make_unique<ANDNOT>());
	Simulator::nodes.push_back(std::make_unique<OutputNode>());

	// make all required connections
	Simulator::nodes[0]->addConnection(*Simulator::nodes[4], Port::A);
	Simulator::nodes[1]->addConnection(*Simulator::nodes[4], Port::B);
	Simulator::nodes[2]->addConnection(*Simulator::nodes[5], Port::A);
	Simulator::nodes[3]->addConnection(*Simulator::nodes[5], Port::B);
	Simulator::nodes[4]->addConnection(*Simulator::nodes[6], Port::A);
	Simulator::nodes[5]->addConnection(*Simulator::nodes[6], Port::B);
	Simulator::nodes[6]->addConnection(*Simulator::nodes[7], Port::O);

	int a = 0, b = 0, c = 0, d = 0;
	while(scanf("%d %d %d %d", &a, &b, &c, &d) != EOF){
		Signal aa = Signal::LOW, bb = Signal::LOW, cc = Signal::LOW, dd = Signal::LOW;
		aa = (a == 0) ? Signal::LOW : Signal::HIGH;
		bb = (b == 0) ? Signal::LOW : Signal::HIGH;
		cc = (c == 0) ? Signal::LOW : Signal::HIGH;
		dd = (d == 0) ? Signal::LOW : Signal::HIGH;

		// test inputs
		static_cast<InputNode *>(Simulator::nodes[0].get())->evaluate(Port::O, aa);
		static_cast<InputNode *>(Simulator::nodes[1].get())->evaluate(Port::O, bb);
		static_cast<InputNode *>(Simulator::nodes[2].get())->evaluate(Port::O, cc);
		static_cast<InputNode *>(Simulator::nodes[3].get())->evaluate(Port::O, dd);

		//simulate
		Simulator::run();

		// examine outputs
		std::cout << static_cast<int>(Simulator::nodes[7]->getResult()) << std::endl;
	}
}
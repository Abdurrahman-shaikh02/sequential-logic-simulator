#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "sequential/DFF_P.hpp"
#include <iostream>
#include <memory>
#include "core/Node.hpp"
#include "combinational/NOT.hpp"

int main(){
	// make all nodes
	Simulator::nodes.push_back(std::make_unique<InputNode>());	//0
	Simulator::nodes.push_back(std::make_unique<InputNode>());	//1clock
	Simulator::nodes.push_back(std::make_unique<DFF_P>());		//2
	Simulator::nodes.push_back(std::make_unique<DFF_P>());		//3
	Simulator::nodes.push_back(std::make_unique<DFF_P>());		//4
	Simulator::nodes.push_back(std::make_unique<DFF_P>());		//5
	Simulator::nodes.push_back(std::make_unique<OutputNode>());	//6
	Simulator::nodes.push_back(std::make_unique<OutputNode>());	//7
	Simulator::nodes.push_back(std::make_unique<OutputNode>());	//8
	Simulator::nodes.push_back(std::make_unique<OutputNode>());	//9

	// make all required connections
	Simulator::nodes[0]->addConnection(*Simulator::nodes[2], Port::D);
	Simulator::nodes[2]->addConnection(*Simulator::nodes[3], Port::D);
	Simulator::nodes[3]->addConnection(*Simulator::nodes[4], Port::D);
	Simulator::nodes[4]->addConnection(*Simulator::nodes[5], Port::D);

	Simulator::nodes[2]->addConnection(*Simulator::nodes[6], Port::O);
	Simulator::nodes[3]->addConnection(*Simulator::nodes[7], Port::O);
	Simulator::nodes[4]->addConnection(*Simulator::nodes[8], Port::O);
	Simulator::nodes[5]->addConnection(*Simulator::nodes[9], Port::O);

	Simulator::nodes[1]->addConnection(*Simulator::nodes[2], Port::CLK);
	Simulator::nodes[1]->addConnection(*Simulator::nodes[3], Port::CLK);
	Simulator::nodes[1]->addConnection(*Simulator::nodes[4], Port::CLK);
	Simulator::nodes[1]->addConnection(*Simulator::nodes[5], Port::CLK);

	int a = 0, b = 0;
	while(scanf("%d %d", &a, &b) != EOF){
		Signal aa = Signal::LOW, bb = Signal::LOW;
		aa = (a == 0) ? Signal::LOW : Signal::HIGH;
		bb = (b == 0) ? Signal::LOW : Signal::HIGH;

		static_cast<InputNode *>(Simulator::nodes[0].get())->evaluate(Port::O, aa);
		static_cast<InputNode *>(Simulator::nodes[1].get())->evaluate(Port::O, bb);

		//simulate
		Simulator::run();

		// examine outputs
		std::cout << static_cast<int>(Simulator::nodes[6]->getResult()) << " ";
		std::cout << static_cast<int>(Simulator::nodes[7]->getResult()) << " ";
		std::cout << static_cast<int>(Simulator::nodes[8]->getResult()) << " ";
		std::cout << static_cast<int>(Simulator::nodes[9]->getResult()) << std::endl;
	}
}

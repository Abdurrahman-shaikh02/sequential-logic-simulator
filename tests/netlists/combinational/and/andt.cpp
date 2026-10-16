#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "combinational/AND.hpp"
#include <iostream>
#include <memory>
#include "core/Node.hpp"

int main(){
	Simulator::parse("netlist.table");
	Simulator::initialize();

	int a = 0, b = 0;
	while(scanf("%d %d", &a, &b) != EOF){
		Signal aa = Signal::LOW, bb = Signal::LOW, cc = Signal::LOW, dd = Signal::LOW;
		aa = (a == 0) ? Signal::LOW : Signal::HIGH;
		bb = (b == 0) ? Signal::LOW : Signal::HIGH;

		// test inputs
		Simulator::primaryInputs["a"][0]->evaluate(Port::O, aa);
		Simulator::primaryInputs["b"][0]->evaluate(Port::O, bb);

		//simulate
		Simulator::run();

		// examine outputs
		std::cout << static_cast<int>(Simulator::primaryOutputs["y"][0]->getResult()) << std::endl;
	}
}

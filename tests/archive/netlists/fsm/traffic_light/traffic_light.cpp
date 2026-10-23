#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "combinational/AND.hpp"
#include <iostream>
#include <memory>
#include "core/Node.hpp"

int main(){
	Simulator::parse("./traffic_light_fsm.table");
	Simulator::initialize();

	int en = 0, clk = 0, reset = 0;

	std::cout << "en clk reset\n";
	while(scanf("%d %d %d", &en, &clk, &reset) != EOF){
		Signal aa = Signal::LOW, bb = Signal::LOW, cc = Signal::LOW;
		aa = (en == 0) ? Signal::LOW : Signal::HIGH;
		bb = (clk == 0) ? Signal::LOW : Signal::HIGH;
		cc = (reset == 0) ? Signal::LOW : Signal::HIGH;

		// test inputs
		Simulator::primaryInputs["en"][0]->evaluate(Port::O, aa);
		Simulator::primaryInputs["clk"][0]->evaluate(Port::O, bb);
		Simulator::primaryInputs["reset"][0]->evaluate(Port::O, cc);

		//simulate
		Simulator::run();

		// examine outputs
		std::cout << "R Y G -> ";
		std::cout << static_cast<int>(Simulator::primaryOutputs["R"][0]->getResult()) << " "
			<< static_cast<int>(Simulator::primaryOutputs["Y"][0]->getResult()) << " "
			<< static_cast<int>(Simulator::primaryOutputs["G"][0]->getResult()) << std::endl;
		std::cout << "en clk reset\n";
	}
}

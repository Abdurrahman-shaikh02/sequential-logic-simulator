#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include <iostream>

int main() {
    if (Simulator::parse("netlist.table") != 0) return 1;
    Simulator::initialize();

    int d = 0, clk = 0;
    while (scanf("%d %d", &d, &clk) != EOF) {
        Signal dd = (d == 0) ? Signal::LOW : Signal::HIGH;
        Signal cc = (clk == 0) ? Signal::LOW : Signal::HIGH;

        Simulator::primaryInputs["d"][0]->evaluate(Port::O, dd);
        Simulator::primaryInputs["clk"][0]->evaluate(Port::O, cc);

        Simulator::run();

        std::cout << "q -> ";

        std::cout << static_cast<int>(Simulator::primaryOutputs["q"][0]->getResult()) << std::endl;
        std::cout << "d clk\n";
    }
}

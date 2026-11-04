#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include <iostream>

int main() {
    if (Simulator::parse("netlist.table") != 0) return 1;
    Simulator::initialize();

    int t = 0, clk = 0;
    while (scanf("%d %d", &t, &clk) != EOF) {
        Signal tt = (t == 0) ? Signal::LOW : Signal::HIGH;
        Signal cc = (clk == 0) ? Signal::LOW : Signal::HIGH;

        Simulator::primaryInputs["t"][0]->evaluate(Port::O, tt);
        Simulator::primaryInputs["clk"][0]->evaluate(Port::O, cc);

        Simulator::run();
        std::cout << "q -> ";
        std::cout << static_cast<int>(Simulator::primaryOutputs["q"][0]->getResult()) << std::endl;
        std::cout << "t clk\n";
    }
    return 0;
}
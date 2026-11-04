#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include <iostream>

int main() {
    if (Simulator::parse("netlist.table") != 0) return 1;
    Simulator::initialize();

    int j = 0, k = 0, clk = 0;
    while (scanf("%d %d %d", &j, &k, &clk) != EOF) {
        Signal jj = (j == 0) ? Signal::LOW : Signal::HIGH;
        Signal kk = (k == 0) ? Signal::LOW : Signal::HIGH;
        Signal cc = (clk == 0) ? Signal::LOW : Signal::HIGH;

        Simulator::primaryInputs["j"][0]->evaluate(Port::O, jj);
        Simulator::primaryInputs["k"][0]->evaluate(Port::O, kk);
        Simulator::primaryInputs["clk"][0]->evaluate(Port::O, cc);

        Simulator::run();
        std::cout << "q -> ";
        std::cout << static_cast<int>(Simulator::primaryOutputs["q"][0]->getResult()) << std::endl;
        std::cout << "j k clk\n";
    }
    return 0;
}
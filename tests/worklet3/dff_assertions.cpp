
#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "sequential/DFF_P.hpp"
#include <iostream>
#include <memory>

int passed = 0;
int failed = 0;

void check(const char* name, Signal expected, Signal actual) {
    if (expected == actual) {
        std::cout << "[PASS] " << name << '\n';
        ++passed;
    } else {
        std::cout << "[FAIL] " << name
                  << " Expected=" << static_cast<int>(expected)
                  << " Actual=" << static_cast<int>(actual) << '\n';
        ++failed;
    }
}

int main() {
    Simulator::nodes.clear();
    Simulator::combinationalQueue.clear();
    Simulator::sequentialQueue.clear();

    auto data = std::make_unique<InputNode>();
    auto clock = std::make_unique<InputNode>();
    auto flipFlop = std::make_unique<DFF_P>();

    InputNode* dataPtr = data.get();
    InputNode* clockPtr = clock.get();
    DFF_P* ffPtr = flipFlop.get();

    data->addConnection(*flipFlop, Port::D);
    clock->addConnection(*flipFlop, Port::CLK);

    Simulator::nodes.push_back(std::move(data));
    Simulator::nodes.push_back(std::move(clock));
    Simulator::nodes.push_back(std::move(flipFlop));

    check("Initial output is LOW", Signal::LOW, ffPtr->getResult());

    // Set D=HIGH while clock remains LOW.
    dataPtr->evaluate(Port::O, Signal::HIGH);
    Simulator::run();

    check("Output stays LOW before rising edge",
          Signal::LOW, ffPtr->getResult());

    // Rising clock edge captures D=HIGH.
    clockPtr->evaluate(Port::O, Signal::HIGH);
    Simulator::run();

    check("Rising edge captures HIGH",
          Signal::HIGH, ffPtr->getResult());

    // Lower clock, then change D to LOW.
    clockPtr->evaluate(Port::O, Signal::LOW);
    Simulator::run();

    dataPtr->evaluate(Port::O, Signal::LOW);
    Simulator::run();

    check("Output holds HIGH without rising edge",
          Signal::HIGH, ffPtr->getResult());

    // Next rising edge captures D=LOW.
    clockPtr->evaluate(Port::O, Signal::HIGH);
    Simulator::run();

    check("Next rising edge captures LOW",
          Signal::LOW, ffPtr->getResult());

    std::cout << "\nResults: " << passed << " passed, "
              << failed << " failed.\n";


    return failed == 0 ? 0 : 1;

}

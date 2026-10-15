#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "sequential/DFF_P.hpp"

#include <fstream>
#include <iostream>
#include <memory>

int main() {
    Simulator::nodes.clear();
    Simulator::combinationalQueue.clear();
    Simulator::sequentialQueue.clear();

    auto data = std::make_unique<InputNode>();
    auto clock = std::make_unique<InputNode>();
    auto ff = std::make_unique<DFF_P>();

    InputNode* dataPtr = data.get();
    InputNode* clockPtr = clock.get();
    DFF_P* ffPtr = ff.get();

    data->addConnection(*ff, Port::D);
    clock->addConnection(*ff, Port::CLK);

    Simulator::nodes.push_back(std::move(data));
    Simulator::nodes.push_back(std::move(clock));
    Simulator::nodes.push_back(std::move(ff));

    std::ofstream vcd("tests/worklet3/dff_waveform.vcd");
    if (!vcd) {
        std::cerr << "Could not create waveform file.\n";
        return 1;
    }

    vcd << "$timescale 1ns $end\n"
        << "$scope module dff_test $end\n"
        << "$var wire 1 d D $end\n"
        << "$var wire 1 c CLK $end\n"
        << "$var wire 1 q Q $end\n"
        << "$upscope $end\n"
        << "$enddefinitions $end\n";

    const int dataValues[] = {0, 1, 1, 0, 0, 0};
    const int clockValues[] = {0, 0, 1, 1, 0, 1};
    const int expectedValues[] = {0, 0, 1, 1, 1, 0};

    int passed = 0;
    int failed = 0;

    for (int i = 0; i < 6; ++i) {
        Signal d = dataValues[i] ? Signal::HIGH : Signal::LOW;
        Signal clk = clockValues[i] ? Signal::HIGH : Signal::LOW;

        dataPtr->evaluate(Port::O, d);
        clockPtr->evaluate(Port::O, clk);
        Simulator::run();

        Signal actual = ffPtr->getResult();
        Signal expected = expectedValues[i] ? Signal::HIGH : Signal::LOW;

        vcd << "#" << i << "\n"
            << dataValues[i] << "d\n"
            << clockValues[i] << "c\n"
            << static_cast<int>(actual) << "q\n";

        if (actual == expected) {
            std::cout << "[PASS] Time " << i
                      << ": Q=" << static_cast<int>(actual) << '\n';
            ++passed;
        } else {
            std::cout << "[FAIL] Time " << i
                      << ": expected Q=" << expectedValues[i]
                      << ", actual Q=" << static_cast<int>(actual) << '\n';
            ++failed;
        }
    }

    vcd.close();

    std::cout << "\nResults: " << passed << " passed, "
              << failed << " failed.\n";
    std::cout << "Waveform: tests/worklet3/dff_waveform.vcd\n";

    return failed == 0 ? 0 : 1;
}

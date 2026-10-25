#include "testbench/TestSuite.hpp"
#include "testbench/Testbench.hpp"
#include "waveform/WaveformGenerator.hpp"
#include "assertion/AssertionEngine.hpp"
#include "simulation/Simulator.hpp"
#include "parser/Parser.hpp"
#include "synthesizer/Synthesizer.hpp"
#include "synthesizer/DeadStates.hpp"
#include "synthesizer/UnreachableStates.hpp"
#include "combinational/AND.hpp"
#include "combinational/OR.hpp"
#include "combinational/NAND.hpp"
#include "combinational/NOR.hpp"
#include "combinational/NOT.hpp"
#include "combinational/XOR.hpp"
#include "combinational/MUX.hpp"
#include "sequential/DFF_P.hpp"
#include "sequential/TFF_P.hpp"
#include "sequential/JKFF_P.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "operations/SetPortOperation.hpp"
#include "operations/ChangeStateOperation.hpp"

#include <iomanip>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace slms {

void TestSuite::runIntegrationTests() {
    // A. Worklet 1 + 2 + 3 Pipeline on Traffic Light FSM
    {
        Testbench tb;
        bool loaded = tb.loadCircuit("tests/netlists/fsm/traffic_light/traffic_light_fsm.net");
        if (loaded) {
            tb.setClock("clk", 10);
            tb.setMaxTime(60);

            // Initial reset
            tb.addStimulus(0, "reset", 1);
            tb.addStimulus(0, "en", 0);
            tb.addStimulus(5, "reset", 0);

            // Advance through states
            tb.addStimulus(15, "en", 1);

            // Assertions for traffic lights: Green -> Yellow -> Red -> Green
            tb.addAssertion(5, "G", 1, "Reset state is Green");
            tb.addAssertion(5, "Y", 0, "Yellow is off at reset");
            tb.addAssertion(5, "R", 0, "Red is off at reset");

            tb.addAssertion(25, "Y", 1, "Advances to Yellow after clock edge");
            tb.addAssertion(35, "R", 1, "Advances to Red after Yellow");
            tb.addAssertion(45, "G", 1, "Advances to Green after Red");

            bool pass = tb.run();
            record("Integration", "Worklet1_2_3_FSM_Pipeline", pass, "Complete FSM synthesis, parsing, simulation and assertion flow");
        } else {
            record("Integration", "Worklet1_2_3_FSM_Pipeline", false, "Failed to load/synthesize FSM");
        }
    }

    // B. Sequential Shift Register Integration
    {
        Testbench tb;
        bool loaded = tb.loadCircuit("tests/netlists/sequential/15_shiftregister/netlist.table");
        if (loaded) {
            tb.setClock("clk", 10);
            tb.setMaxTime(50);
            // Shift in bits serially on 'serial_in'
            tb.addStimulus(0, "serial_in", 1);
            tb.addStimulus(10, "serial_in", 0);
            tb.addStimulus(20, "serial_in", 1);
            tb.addStimulus(30, "serial_in", 1);

            bool pass = tb.run();
            record("Integration", "Sequential_ShiftRegister", pass, "Multi-stage flip-flop register shifts bits on clock pulses");
        } else {
            record("Integration", "Sequential_ShiftRegister", false, "Shift register netlist not found");
        }
    }

    // C. Combinational AND Netlist Integration
    {
        Testbench tb;
        bool loaded = tb.loadCircuit("tests/netlists/combinational/and/netlist.table");
        if (loaded) {
            tb.setMaxTime(30);
            tb.addStimulus(0, "a", 0);
            tb.addStimulus(0, "b", 0);
            tb.addAssertion(5, "y", 0, "0 AND 0 = 0");

            tb.addStimulus(10, "a", 1);
            tb.addStimulus(10, "b", 0);
            tb.addAssertion(15, "y", 0, "1 AND 0 = 0");

            tb.addStimulus(20, "a", 1);
            tb.addStimulus(20, "b", 1);
            tb.addAssertion(25, "y", 1, "1 AND 1 = 1");

            bool pass = tb.run();
            record("Integration", "Combinational_Netlist_Evaluation", pass, "Parsed netlist evaluates topologically and matches expected");
        } else {
            record("Integration", "Combinational_Netlist_Evaluation", false, "Combinational and netlist not found");
        }
    }
}

} // namespace slms

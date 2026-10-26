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

void TestSuite::runBlackBoxTests() {
    // Equivalence Partitioning & Boundary Value Analysis (BVA)
    // Partition 1: Clock inactive (static LOW), inputs toggling.
    // In sequential circuits, outputs must remain static without clock edges!
    {
        Testbench tb;
        tb.loadCircuit("tests/netlists/sequential/13_dffp/netlist.table");
        tb.setMaxTime(40);

        // Keep clk = 0 throughout
        tb.addStimulus(0, "clk", 0);
        tb.addStimulus(0, "d", 0);
        tb.addAssertion(5, "q", 0, "Initial Q is LOW");

        // Change D to 1 without rising clock edge
        tb.addStimulus(10, "d", 1);
        tb.addAssertion(20, "q", 0, "Q must NOT change to 1 because clock has no edge");

        // Change D back to 0
        tb.addStimulus(25, "d", 0);
        tb.addAssertion(35, "q", 0, "Q remains unchanged across D transitions without clock");

        bool pass = tb.run();
        record("BlackBox", "EquivalencePartition_StaticClock_NoStateChange", pass, "Sequential logic rejects data transitions without valid clock edge");
    }

    // Partition 2: Active clock boundary edge
    {
        Testbench tb;
        tb.loadCircuit("tests/netlists/sequential/13_dffp/netlist.table");
        tb.setClock("clk", 10);
        tb.setMaxTime(30);

        tb.addStimulus(0, "d", 1);
        // Positive clock edge arrives at t=5
        tb.addAssertion(6, "q", 1, "Q latches D=1 immediately after clock rising edge");

        bool pass = tb.run();
        record("BlackBox", "BoundaryValue_ClockEdgeTrigger", pass, "State successfully updates on positive edge boundary");
    }
}

} // namespace slms

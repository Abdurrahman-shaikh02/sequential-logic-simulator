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
#include <map>

namespace slms {

void TestSuite::runRegressionTests() {
    // Compare all four AND rows against an independent reference table.
    {
        Simulator::clear();
        const bool parsed = Simulator::parse("tests/netlists/combinational/and/netlist.table") == 0;
        std::filesystem::create_directories("build");
        const std::string path = "build/regression_truth_table.txt";
        const bool generated = parsed && Simulator::generateTruthTable(path);
        std::ifstream in(path);
        std::string header;
        std::getline(in, header);
        // Column order is a,b | y for this netlist; independently check all four rows.
        std::map<std::pair<int,int>, int> observed;
        int a, b, y;
        std::string separator;
        while (in >> a >> b >> separator >> y) {
            if (separator == "|" && (a==0 || a==1) && (b==0 || b==1))
                observed[{a,b}] = y;
        }
        const bool correct = observed.size() == 4 &&
            observed[{0,0}] == 0 && observed[{0,1}] == 0 &&
            observed[{1,0}] == 0 && observed[{1,1}] == 1;
        record("Regression", "AND_TruthTable_AllRows", generated && correct,
               "Compare all four generated rows to independent AND truth table");
        std::filesystem::remove(path);
        Simulator::clear();
    }
    // Synthesize then simulate FSM and verify a known reference sequence.
    {
        Testbench tb;
        const bool loaded = tb.loadCircuit("docs/fsm_example/traffic_light_fsm.net");
        tb.setClock("clk", 10, 5);
        tb.setMaxTime(38);
        tb.addStimulus(0, "reset", 1);
        tb.addStimulus(1, "en", 1);
        tb.addStimulus(2, "reset", 0);
        // t=5,15,25,35 are enabled rising edges; expect Y,R,G,Y.
        tb.addAssertion(5, "Y", 1, "GREEN -> YELLOW");
        tb.addAssertion(15, "R", 1, "YELLOW -> RED");
        tb.addAssertion(25, "G", 1, "RED -> GREEN");
        tb.addAssertion(35, "Y", 1, "GREEN -> YELLOW again");
        const bool correct = loaded && tb.run();
        record("Regression", "FSM_Synthesis_ReferenceSequence", correct,
               "Verify four FSM state transitions against independent golden sequence");
    }
    // Invalid assertions and out-of-range signal indices must never pass.
    {
        Testbench tb;
        const bool loaded = tb.loadCircuit("tests/netlists/combinational/and/netlist.table");
        tb.setMaxTime(10);
        tb.addAssertion(5, "wrong_signal_name", 0, "Unknown signals must fail");
        record("Regression", "UnknownSignalAssertionFails", loaded && !tb.run(),
               "Invalid output signal cannot silently equal LOW");
    }
    {
        Testbench tb;
        const bool loaded = tb.loadCircuit("tests/netlists/combinational/and/netlist.table");
        tb.setMaxTime(10);
        tb.addAssertion(5, "y", 0, "Out-of-bounds bit must fail", 9);
        record("Regression", "InvalidBitIndexAssertionFails", loaded && !tb.run(),
               "Invalid output bit index cannot silently equal LOW");
    }
    // Check an asymmetric duty cycle's actual transition timestamps.
    {
        Testbench tb;
        const bool loaded = tb.loadCircuit("tests/netlists/combinational/and/netlist.table");
        tb.setClock("a", 10, 3);
        tb.setMaxTime(21);
        tb.addStimulus(0, "b", 1);
        tb.addAssertion(0, "y", 0, "clock low at zero");
        tb.addAssertion(3, "y", 1, "clock high at three");
        tb.addAssertion(10, "y", 0, "clock low at ten");
        tb.addAssertion(13, "y", 1, "clock high at thirteen");
        tb.addAssertion(20, "y", 0, "clock low at twenty");
        record("Regression", "AsymmetricClock_10_3", loaded && tb.run(),
               "Check both low and high clock edges at 0,3,10,13,20");
    }
}

} // namespace slms

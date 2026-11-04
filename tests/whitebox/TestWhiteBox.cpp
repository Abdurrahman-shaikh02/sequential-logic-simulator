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
#include "sequential/DFF_N.hpp"
#include "sequential/TFF_P.hpp"
#include "sequential/TFF_N.hpp"
#include "sequential/JKFF_P.hpp"
#include "sequential/JKFF_N.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "operations/ChangeStateOperation.hpp"

#include <iomanip>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace slms {

void TestSuite::runWhiteBoxTests() {
    // White-box test 1: simultaneous data/control and clock events use the new input.
    // The input is applied directly, while the clock edge is queued; when the
    // queued state transition runs it must sample the latest input value.
    {
        bool allPassed = true;

        Simulator::clear();
        DFF_P dffp;
        dffp.commit(Signal::LOW); // mark initialized, with clock initially LOW
        dffp.evaluate(Port::D, Signal::HIGH);
        dffp.evaluate(Port::CLK, Signal::HIGH);
        Simulator::run();
        allPassed = allPassed && (dffp.getResult() == Signal::HIGH);

        // Also reverse the evaluate() call order: queue the clock edge first,
        // then update D before the simulator drains the sequential queue.
        Simulator::clear();
        DFF_P dffpClockFirst;
        dffpClockFirst.commit(Signal::LOW);
        dffpClockFirst.evaluate(Port::CLK, Signal::HIGH);
        dffpClockFirst.evaluate(Port::D, Signal::HIGH);
        Simulator::run();
        allPassed = allPassed && (dffpClockFirst.getResult() == Signal::HIGH);

        Simulator::clear();
        DFF_N dffn;
        dffn.commit(Signal::LOW);
        dffn.evaluate(Port::CLK, Signal::HIGH); // establish the inactive level before the falling edge
        dffn.evaluate(Port::D, Signal::HIGH);
        dffn.evaluate(Port::CLK, Signal::LOW);
        Simulator::run();
        allPassed = allPassed && (dffn.getResult() == Signal::HIGH);

        Simulator::clear();
        JKFF_P jkffp;
        jkffp.commit(Signal::LOW);
        jkffp.evaluate(Port::J, Signal::HIGH);
        jkffp.evaluate(Port::K, Signal::LOW);
        jkffp.evaluate(Port::CLK, Signal::HIGH);
        Simulator::run();
        allPassed = allPassed && (jkffp.getResult() == Signal::HIGH);

        Simulator::clear();
        JKFF_N jkffn;
        jkffn.commit(Signal::LOW);
        jkffn.evaluate(Port::CLK, Signal::HIGH); // establish the inactive level before the falling edge
        jkffn.evaluate(Port::J, Signal::HIGH);
        jkffn.evaluate(Port::K, Signal::LOW);
        jkffn.evaluate(Port::CLK, Signal::LOW);
        Simulator::run();
        allPassed = allPassed && (jkffn.getResult() == Signal::HIGH);

        Simulator::clear();
        TFF_P tffp;
        tffp.commit(Signal::LOW);
        tffp.evaluate(Port::T, Signal::HIGH);
        tffp.evaluate(Port::CLK, Signal::HIGH);
        Simulator::run();
        allPassed = allPassed && (tffp.getResult() == Signal::HIGH);

        Simulator::clear();
        TFF_N tffn;
        tffn.commit(Signal::LOW);
        tffn.evaluate(Port::CLK, Signal::HIGH); // establish the inactive level before the falling edge
        tffn.evaluate(Port::T, Signal::HIGH);
        tffn.evaluate(Port::CLK, Signal::LOW);
        Simulator::run();
        allPassed = allPassed && (tffn.getResult() == Signal::HIGH);

        Simulator::clear();
        record("WhiteBox", "FlipFlop_SimultaneousInputClock_UsesNewestInput", allPassed,
               "All six flip-flop variants sample the newest input when input and active clock edge arrive together; no hold-time warning/reordering is used");
    }

    // White-box test 2: TopologicalQueue priorities and draining
    {
        Simulator::clear();
        InputNode inNode;
        OutputNode outNode;
        inNode.addConnection(outNode, Port::O);

        inNode.evaluate(Port::O, Signal::HIGH);
        Simulator::run();
        bool ok = (outNode.getResult() == Signal::HIGH);
        Simulator::clear();

        record("WhiteBox", "Simulation_TwoPhase_Topological_Drain", ok, 
               "Combinational events propagate and drain topologically to downstream nodes");
    }
}

} // namespace slms

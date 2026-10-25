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

void TestSuite::runWhiteBoxTests() {
    // White-box test 1: Testing internal OperationQueue::reorder() for hold-time violation
    {
        OperationQueue seqQueue;
        DFF_P testNode;

        // Force a SetPortOperation before ChangeStateOperation for the exact same node
        seqQueue.enqueue(std::make_unique<SetPortOperation>(testNode, Port::D, Signal::HIGH));
        seqQueue.enqueue(std::make_unique<ChangeStateOperation>(testNode, Port::CLK, Signal::HIGH));

        // Call reorder() to verify hazard detection and priority swap
        seqQueue.reorder();

        // The first dequeued operation must now be ChangeStateOperation!
        auto firstOp = seqQueue.dequeue();
        bool swappedCorrectly = (dynamic_cast<ChangeStateOperation*>(firstOp.get()) != nullptr);

        record("WhiteBox", "OperationQueue_HazardDetection_And_Reorder", swappedCorrectly, 
               "reorder() successfully detects hold-time hazard and swaps ChangeState before SetPort");
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

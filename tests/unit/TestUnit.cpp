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

void TestSuite::runUnitTests() {
    // A. Combinational Gates Truth Tables
    {
        AND andGate;
        andGate.evaluate(Port::A, Signal::LOW);
        andGate.evaluate(Port::B, Signal::LOW);
        bool p1 = (andGate.getResult() == Signal::LOW);
        andGate.evaluate(Port::A, Signal::HIGH);
        andGate.evaluate(Port::B, Signal::LOW);
        bool p2 = (andGate.getResult() == Signal::LOW);
        andGate.evaluate(Port::A, Signal::HIGH);
        andGate.evaluate(Port::B, Signal::HIGH);
        bool p3 = (andGate.getResult() == Signal::HIGH);
        record("Unit", "Combinational_AND_TruthTable", p1 && p2 && p3, "AND(0,0)=0, AND(1,0)=0, AND(1,1)=1");
    }

    {
        OR orGate;
        orGate.evaluate(Port::A, Signal::LOW);
        orGate.evaluate(Port::B, Signal::LOW);
        bool p1 = (orGate.getResult() == Signal::LOW);
        orGate.evaluate(Port::A, Signal::HIGH);
        orGate.evaluate(Port::B, Signal::LOW);
        bool p2 = (orGate.getResult() == Signal::HIGH);
        record("Unit", "Combinational_OR_TruthTable", p1 && p2, "OR(0,0)=0, OR(1,0)=1");
    }

    {
        NOT notGate;
        notGate.evaluate(Port::A, Signal::LOW);
        bool p1 = (notGate.getResult() == Signal::HIGH);
        notGate.evaluate(Port::A, Signal::HIGH);
        bool p2 = (notGate.getResult() == Signal::LOW);
        record("Unit", "Combinational_NOT_TruthTable", p1 && p2, "NOT(0)=1, NOT(1)=0");
    }

    {
        XOR xorGate;
        xorGate.evaluate(Port::A, Signal::HIGH);
        xorGate.evaluate(Port::B, Signal::HIGH);
        bool p1 = (xorGate.getResult() == Signal::LOW);
        xorGate.evaluate(Port::A, Signal::HIGH);
        xorGate.evaluate(Port::B, Signal::LOW);
        bool p2 = (xorGate.getResult() == Signal::HIGH);
        record("Unit", "Combinational_XOR_TruthTable", p1 && p2, "XOR(1,1)=0, XOR(1,0)=1");
    }

    {
        MUX muxGate;
        // Select A when S=LOW
        muxGate.evaluate(Port::A, Signal::HIGH);
        muxGate.evaluate(Port::B, Signal::LOW);
        muxGate.evaluate(Port::S, Signal::LOW);
        bool p1 = (muxGate.getResult() == Signal::HIGH);

        // Select B when S=HIGH
        muxGate.evaluate(Port::S, Signal::HIGH);
        bool p2 = (muxGate.getResult() == Signal::LOW);
        record("Unit", "Combinational_MUX_Select", p1 && p2, "MUX selects A when S=0, selects B when S=1");
    }

    // B. Sequential Flip-Flop Nodes
    {
        DFF_P dff;
        // Simulate clock rising edge with D=HIGH
        dff.evaluate(Port::D, Signal::HIGH);
        dff.setPort(Port::D, Signal::HIGH);
        dff.changeState();
        bool p1 = (dff.getResult() == Signal::HIGH);

        // Simulate clock rising edge with D=LOW
        dff.setPort(Port::D, Signal::LOW);
        dff.changeState();
        bool p2 = (dff.getResult() == Signal::LOW);
        record("Unit", "Sequential_DFF_P_ClockStateChange", p1 && p2, "DFF updates state on positive clock edge");
    }

    // C. Synthesizer State Analysis
    {
        std::vector<std::string> dead, unreachable;
        bool okDead = DeadStates::find("docs/fsm_example/dead_unreachable_fsm.net", dead, false);
        bool okUnreach = UnreachableStates::find("docs/fsm_example/dead_unreachable_fsm.net", unreachable, false);
        bool p1 = okDead && !dead.empty();
        bool p2 = okUnreach && !unreachable.empty();
        record("Unit", "Synthesizer_DeadAndUnreachableDetection", p1 && p2, "Accurately detects dead states & unreachable states");
    }

    // D. Parser Negative Test (Handling malformed or missing file)
    {
        std::vector<std::unique_ptr<Node>> testNodes;
        std::unordered_map<std::string, std::vector<Node*>> pi, po;
        int rc = Parser::parse("non_existent_circuit_file.table", testNodes, pi, po);
        record("Unit", "Parser_Negative_MissingFile", rc != 0, "Parser cleanly returns non-zero error on non-existent file");
    }
}

} // namespace slms

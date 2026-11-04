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
#include "sequential/TFF_N.hpp"
#include "sequential/JKFF_N.hpp"
#include "sequential/DFF_N.hpp"
#include "sequential/TFF_P.hpp"
#include "sequential/JKFF_P.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "operations/ChangeStateOperation.hpp"

#include <iomanip>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace slms {

void TestSuite::runUnitTests() {
    // A. Combinational Gates Truth Tables — exercise all four input combinations.
    // Each row is checked independently; the named test passes only when all rows match.
    {
        AND andGate;
        bool allMatch = true;
        for (int a = 0; a <= 1; ++a) {
            for (int b = 0; b <= 1; ++b) {
                andGate.evaluate(Port::A, a ? Signal::HIGH : Signal::LOW);
                andGate.evaluate(Port::B, b ? Signal::HIGH : Signal::LOW);
                const bool expected = (a && b);
                allMatch = (andGate.getResult() == (expected ? Signal::HIGH : Signal::LOW)) && allMatch;
            }
        }
        record("Unit", "Combinational_AND_TruthTable", allMatch,
               "AND(0,0)=0, AND(0,1)=0, AND(1,0)=0, AND(1,1)=1");
    }

    {
        OR orGate;
        bool allMatch = true;
        for (int a = 0; a <= 1; ++a) {
            for (int b = 0; b <= 1; ++b) {
                orGate.evaluate(Port::A, a ? Signal::HIGH : Signal::LOW);
                orGate.evaluate(Port::B, b ? Signal::HIGH : Signal::LOW);
                const bool expected = (a || b);
                allMatch = (orGate.getResult() == (expected ? Signal::HIGH : Signal::LOW)) && allMatch;
            }
        }
        record("Unit", "Combinational_OR_TruthTable", allMatch,
               "OR(0,0)=0, OR(0,1)=1, OR(1,0)=1, OR(1,1)=1");
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
        bool allMatch = true;
        for (int a = 0; a <= 1; ++a) {
            for (int b = 0; b <= 1; ++b) {
                xorGate.evaluate(Port::A, a ? Signal::HIGH : Signal::LOW);
                xorGate.evaluate(Port::B, b ? Signal::HIGH : Signal::LOW);
                const bool expected = (a != b);
                allMatch = (xorGate.getResult() == (expected ? Signal::HIGH : Signal::LOW)) && allMatch;
            }
        }
        record("Unit", "Combinational_XOR_TruthTable", allMatch,
               "XOR(0,0)=0, XOR(0,1)=1, XOR(1,0)=1, XOR(1,1)=0");
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

    {
        JKFF_P ff;
        ff.evaluate(Port::J, Signal::HIGH);
        ff.evaluate(Port::K, Signal::LOW);
        ff.changeState();
        bool set = ff.getResult() == Signal::HIGH;
        ff.evaluate(Port::J, Signal::HIGH);
        ff.evaluate(Port::K, Signal::HIGH);
        ff.changeState();
        bool toggle = ff.getResult() == Signal::LOW;
        ff.evaluate(Port::J, Signal::LOW);
        ff.evaluate(Port::K, Signal::LOW);
        ff.changeState();
        bool hold = ff.getResult() == Signal::LOW;
        record("Unit", "Sequential_JKFF_P_SetToggleHold", set && toggle && hold,
               "JK positive-edge state rules: set, toggle, and hold");
    }

    {
        TFF_P ff;
        ff.evaluate(Port::T, Signal::HIGH);
        ff.changeState();
        bool firstToggle = ff.getResult() == Signal::HIGH;
        ff.changeState();
        bool secondToggle = ff.getResult() == Signal::LOW;
        ff.evaluate(Port::T, Signal::LOW);
        ff.changeState();
        bool hold = ff.getResult() == Signal::LOW;
        record("Unit", "Sequential_TFF_P_ToggleHold", firstToggle && secondToggle && hold,
               "T positive-edge state rules: toggle when T=1, hold when T=0");
    }

    {
        DFF_N ff;
        ff.evaluate(Port::D, Signal::HIGH);
        ff.changeState();
        bool captured = ff.getResult() == Signal::HIGH;
        ff.evaluate(Port::D, Signal::LOW);
        ff.changeState();
        bool capturedLow = ff.getResult() == Signal::LOW;
        record("Unit", "Sequential_DFF_N_StateChange", captured && capturedLow,
               "DFF negative-edge state capture logic");
    }

    {
        JKFF_N ff;
        ff.evaluate(Port::J, Signal::LOW);
        ff.evaluate(Port::K, Signal::HIGH);
        ff.changeState();
        bool reset = ff.getResult() == Signal::LOW;
        ff.evaluate(Port::J, Signal::HIGH);
        ff.evaluate(Port::K, Signal::LOW);
        ff.changeState();
        bool set = ff.getResult() == Signal::HIGH;
        record("Unit", "Sequential_JKFF_N_ResetSet", reset && set,
               "JK negative-edge state rules: reset and set");
    }

    {
        TFF_N ff;
        ff.evaluate(Port::T, Signal::HIGH);
        ff.changeState();
        bool firstToggle = ff.getResult() == Signal::HIGH;
        ff.changeState();
        bool secondToggle = ff.getResult() == Signal::LOW;
        record("Unit", "Sequential_TFF_N_Toggle", firstToggle && secondToggle,
               "T negative-edge state toggles when T=1");
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

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
#include "operations/SetPortOperation.hpp"
#include "operations/ChangeStateOperation.hpp"

#include <iomanip>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace slms {

void TestSuite::runUnitTests() {
        // A. Combinational Gates: Complete Truth Tables

    // AND: Test all 4 input combinations
    {
        AND gate;
        bool passed = true;

        for (int a = 0; a <= 1; ++a) {
            for (int b = 0; b <= 1; ++b) {
                Signal A = a ? Signal::HIGH : Signal::LOW;
                Signal B = b ? Signal::HIGH : Signal::LOW;

                gate.evaluate(Port::A, A);
                gate.evaluate(Port::B, B);

                bool expected = (a && b);
                Signal expectedSignal =
                    expected ? Signal::HIGH : Signal::LOW;

                if (gate.getResult() != expectedSignal) {
                    passed = false;
                }
            }
        }

        record("Unit", "Combinational_AND_TruthTable",
               passed, "All 4 input combinations tested");
    }

    // OR: Test all 4 input combinations
    {
        OR gate;
        bool passed = true;

        for (int a = 0; a <= 1; ++a) {
            for (int b = 0; b <= 1; ++b) {
                Signal A = a ? Signal::HIGH : Signal::LOW;
                Signal B = b ? Signal::HIGH : Signal::LOW;

                gate.evaluate(Port::A, A);
                gate.evaluate(Port::B, B);

                bool expected = (a || b);
                Signal expectedSignal =
                    expected ? Signal::HIGH : Signal::LOW;

                if (gate.getResult() != expectedSignal) {
                    passed = false;
                }
            }
        }

        record("Unit", "Combinational_OR_TruthTable",
               passed, "All 4 input combinations tested");
    }

    // NOT: Test both input values
    {
        NOT gate;
        bool passed = true;

        for (int a = 0; a <= 1; ++a) {
            Signal A = a ? Signal::HIGH : Signal::LOW;
            gate.evaluate(Port::A, A);

            bool expected = !a;
            Signal expectedSignal =
                expected ? Signal::HIGH : Signal::LOW;

            if (gate.getResult() != expectedSignal) {
                passed = false;
            }
        }

        record("Unit", "Combinational_NOT_TruthTable",
               passed, "Both input values tested");
    }

    // XOR: Test all 4 input combinations
    {
        XOR gate;
        bool passed = true;

        for (int a = 0; a <= 1; ++a) {
            for (int b = 0; b <= 1; ++b) {
                Signal A = a ? Signal::HIGH : Signal::LOW;
                Signal B = b ? Signal::HIGH : Signal::LOW;

                gate.evaluate(Port::A, A);
                gate.evaluate(Port::B, B);

                bool expected = (a != b);
                Signal expectedSignal =
                    expected ? Signal::HIGH : Signal::LOW;

                if (gate.getResult() != expectedSignal) {
                    passed = false;
                }
            }
        }

        record("Unit", "Combinational_XOR_TruthTable",
               passed, "All 4 input combinations tested");
    }

    // NAND: Test all 4 input combinations
    {
        NAND gate;
        bool passed = true;

        for (int a = 0; a <= 1; ++a) {
            for (int b = 0; b <= 1; ++b) {
                Signal A = a ? Signal::HIGH : Signal::LOW;
                Signal B = b ? Signal::HIGH : Signal::LOW;

                gate.evaluate(Port::A, A);
                gate.evaluate(Port::B, B);

                bool expected = !(a && b);
                Signal expectedSignal =
                    expected ? Signal::HIGH : Signal::LOW;

                if (gate.getResult() != expectedSignal) {
                    passed = false;
                }
            }
        }

        record("Unit", "Combinational_NAND_TruthTable",
               passed, "All 4 input combinations tested");
    }

    // NOR: Test all 4 input combinations
    {
        NOR gate;
        bool passed = true;

        for (int a = 0; a <= 1; ++a) {
            for (int b = 0; b <= 1; ++b) {
                Signal A = a ? Signal::HIGH : Signal::LOW;
                Signal B = b ? Signal::HIGH : Signal::LOW;

                gate.evaluate(Port::A, A);
                gate.evaluate(Port::B, B);

                bool expected = !(a || b);
                Signal expectedSignal =
                    expected ? Signal::HIGH : Signal::LOW;

                if (gate.getResult() != expectedSignal) {
                    passed = false;
                }
            }
        }

        record("Unit", "Combinational_NOR_TruthTable",
               passed, "All 4 input combinations tested");
    }

    // MUX: Test all 8 combinations of A, B and select S
    {
        MUX gate;
        bool passed = true;

        for (int a = 0; a <= 1; ++a) {
            for (int b = 0; b <= 1; ++b) {
                for (int s = 0; s <= 1; ++s) {
                    Signal A = a ? Signal::HIGH : Signal::LOW;
                    Signal B = b ? Signal::HIGH : Signal::LOW;
                    Signal S = s ? Signal::HIGH : Signal::LOW;

                    gate.evaluate(Port::A, A);
                    gate.evaluate(Port::B, B);
                    gate.evaluate(Port::S, S);

                    bool expected = s ? b : a;
                    Signal expectedSignal =
                        expected ? Signal::HIGH : Signal::LOW;

                    if (gate.getResult() != expectedSignal) {
                        passed = false;
                    }
                }
            }
        }

        record("Unit", "Combinational_MUX_TruthTable",
               passed, "All 8 input combinations tested");
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

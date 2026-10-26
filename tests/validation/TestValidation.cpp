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

void TestSuite::runValidationTests() {
    // Validate simulator behavior against formal mathematical specifications (Golden Model)
    {
        Testbench tb;
        const bool loaded = tb.loadCircuit("tests/netlists/fsm/traffic_light/traffic_light_fsm.net");
        tb.setClock("clk", 10);
        tb.setMaxTime(70);

        // Test hold state: when en=0, the FSM should hold Green indefinitely
        tb.addStimulus(0, "reset", 1);
        tb.addStimulus(5, "reset", 0);
        tb.addStimulus(5, "en", 0);

        tb.addAssertion(15, "G", 1, "Hold Green at t=15 when en=0");
        tb.addAssertion(25, "G", 1, "Hold Green at t=25 when en=0");
        tb.addAssertion(35, "G", 1, "Hold Green at t=35 when en=0");

        bool pass = loaded && tb.run();
        record("Validation", "GoldenModel_FSM_HoldCondition", pass, "FSM remains in stable state when enable is held low");
    }

    {
        // Mutual exclusion property validation: Red and Green are never active together
        Testbench tb;
        const bool loaded = tb.loadCircuit("tests/netlists/fsm/traffic_light/traffic_light_fsm.net");
        tb.setClock("clk", 10);
        tb.setMaxTime(60);

        tb.addStimulus(0, "reset", 1);
        tb.addStimulus(5, "reset", 0);
        tb.addStimulus(10, "en", 1);

        bool safetyMaintained = true;
        tb.addInvariant("TrafficSafety", [&safetyMaintained](const std::unordered_map<std::string, Signal> &st, int64_t) {
            bool r = st.count("R") && st.at("R") == Signal::HIGH;
            bool g = st.count("G") && st.at("G") == Signal::HIGH;
            if (r && g) safetyMaintained = false;
            return !(r && g);
        }, "Red and Green lights must be mutually exclusive");

        const bool ran = loaded && tb.run();
        record("Validation", "SafetyInvariant_MutualExclusion", loaded && ran && safetyMaintained, "Traffic lights never show Red and Green simultaneously");
    }
}

} // namespace slms

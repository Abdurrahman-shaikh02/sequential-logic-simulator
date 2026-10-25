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

void TestSuite::runSystemTests() {
    // A. Positive System Test: Full simulation + Waveform Export (VCD, ASCII, JSON)
    {
        Testbench tb;
        const bool loaded = tb.loadCircuit("tests/netlists/fsm/traffic_light/traffic_light_fsm.net");
        tb.setClock("clk", 10);
        tb.setMaxTime(50);
        tb.addStimulus(0, "reset", 1);
        tb.addStimulus(5, "reset", 0);
        tb.addStimulus(10, "en", 1);

        bool simOk = loaded && tb.run();
        std::filesystem::create_directories("build/system_test");
        bool vcdOk = tb.exportVCD("build/system_test/traffic.vcd");
        bool txtOk = tb.exportWaveform("build/system_test/traffic_wave.txt");
        bool jsonOk = tb.exportJSON("build/system_test/traffic_wave.json");

        record("System", "EndToEnd_Simulation_And_WaveformExport", simOk && vcdOk && txtOk && jsonOk, 
               "System executes complete cycle and generates VCD, ASCII, and JSON waveforms");
    }

    // B. Negative System Test 1: Assertion Failure Detection
    {
        Testbench tb;
        const bool loaded = tb.loadCircuit("tests/netlists/fsm/traffic_light/traffic_light_fsm.net");
        tb.setClock("clk", 10);
        tb.setMaxTime(20);
        tb.addStimulus(0, "reset", 1);
        tb.addStimulus(5, "reset", 0);

        // Intentionally schedule an incorrect assertion: G=0 when reset state is G=1
        tb.addAssertion(5, "G", 0, "Intentional negative test: expecting wrong Green signal");

        bool simResult = loaded && tb.run();
        bool caughtViolation = (loaded && tb.getAssertionEngine().getFailCount() > 0 && !simResult);

        record("System", "Negative_AssertionFailureDetection", caughtViolation, 
               "System accurately catches and logs intentional assertion mismatch without crashing");
    }

    // C. Negative System Test 2: Invalid testbench file loading
    {
        Testbench tb;
        bool loaded = tb.loadTestbenchFile("non_existent_testbench.tb");
        record("System", "Negative_MissingTestbenchFileHandling", !loaded, 
               "System gracefully handles missing testbench file with descriptive error");
    }
}

} // namespace slms

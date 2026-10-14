#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "simulation/Simulator.hpp"
#include "primary/InputNode.hpp"
#include "primary/OutputNode.hpp"
#include "sequential/DFF_P.hpp"
#include "combinational/AND.hpp"
#include "combinational/OR.hpp"
#include "combinational/NOT.hpp"

using namespace std;

static int passed = 0;
static int failed = 0;

template <typename T>
T* addNode() {
    auto node = make_unique<T>();
    T* ptr = node.get();
    Simulator::nodes.push_back(move(node));
    return ptr;
}

void connect(Node* source, Node* target, Port targetPort) {
    source->addConnection(*target, targetPort);
}

Signal bit(int value) {
    return value == 1 ? Signal::HIGH : Signal::LOW;
}

int asInt(Signal value) {
    return value == Signal::HIGH ? 1 : 0;
}

string stateName(int q0, int q1) {
    if (q0 == 0 && q1 == 0) return "RED";
    if (q0 == 1 && q1 == 0) return "GREEN";
    if (q0 == 0 && q1 == 1) return "YELLOW";
    return "INVALID";
}

string nextStateName(const string& current, int enValue) {
    if (enValue == 0) return current;
    if (current == "RED") return "GREEN";
    if (current == "GREEN") return "YELLOW";
    if (current == "YELLOW") return "RED";
    return "INVALID";
}

void check(const string& name, int expected, int actual) {
    if (expected == actual) {
        cout << "[PASS] " << name << '\n';
        ++passed;
    } else {
        cout << "[FAIL] " << name << ": expected " << expected
             << ", got " << actual << '\n';
        ++failed;
    }
}

int main() {
    Simulator::nodes.clear();
    Simulator::combinationalQueue.clear();
    Simulator::sequentialQueue.clear();

    // Primary inputs and outputs.
    InputNode* en = addNode<InputNode>();
    InputNode* clk = addNode<InputNode>();
    OutputNode* outR = addNode<OutputNode>();
    OutputNode* outY = addNode<OutputNode>();
    OutputNode* outG = addNode<OutputNode>();

    // State registers. Both start LOW, so the initial state is RED (00).
    DFF_P* ffQ0 = addNode<DFF_P>();
    DFF_P* ffQ1 = addNode<DFF_P>();

    // Inverted signals.
    NOT* notEn = addNode<NOT>();
    NOT* notQ0 = addNode<NOT>();
    NOT* notQ1 = addNode<NOT>();

    // Next-state terms:
    // D0 = (en AND !Q0 AND !Q1) OR (!en AND Q0 AND !Q1)
    // D1 = (en AND Q0 AND !Q1) OR (!en AND !Q0 AND Q1)
    AND* d0GreenA = addNode<AND>();
    AND* d0YellowA = addNode<AND>();
    OR* d0Or = addNode<OR>();

    AND* d1YellowA = addNode<AND>();
    AND* d1RedA = addNode<AND>();
    OR* d1Or = addNode<OR>();

    // Three-input ANDs are built from pairs of two-input AND gates.
    AND* d0Green = addNode<AND>();
    AND* d0Yellow = addNode<AND>();
    AND* d1Yellow = addNode<AND>();
    AND* d1Red = addNode<AND>();

    // Light outputs.
    AND* redA = addNode<AND>();
    AND* redB = addNode<AND>();
    AND* yellowA = addNode<AND>();
    AND* yellowB = addNode<AND>();
    AND* greenA = addNode<AND>();
    AND* greenB = addNode<AND>();

    // Inverters.
    connect(en, notEn, Port::A);
    connect(ffQ0, notQ0, Port::A);
    connect(ffQ1, notQ1, Port::A);

    // D0 green term = en & !Q0 & !Q1.
    connect(en, d0GreenA, Port::A);
    connect(notQ0, d0GreenA, Port::B);
    connect(d0GreenA, d0Green, Port::A);
    connect(notQ1, d0Green, Port::B);

    // D0 yellow hold term = !en & Q0 & !Q1.
    connect(notEn, d0YellowA, Port::A);
    connect(ffQ0, d0YellowA, Port::B);
    connect(d0YellowA, d0Yellow, Port::A);
    connect(notQ1, d0Yellow, Port::B);

    connect(d0Green, d0Or, Port::A);
    connect(d0Yellow, d0Or, Port::B);
    connect(d0Or, ffQ0, Port::D);

    // D1 yellow-to-red term = en & Q0 & !Q1.
    connect(en, d1YellowA, Port::A);
    connect(ffQ0, d1YellowA, Port::B);
    connect(d1YellowA, d1Yellow, Port::A);
    connect(notQ1, d1Yellow, Port::B);

    // D1 red hold term = !en & !Q0 & Q1.
    connect(notEn, d1RedA, Port::A);
    connect(notQ0, d1RedA, Port::B);
    connect(d1RedA, d1Red, Port::A);
    connect(ffQ1, d1Red, Port::B);

    connect(d1Yellow, d1Or, Port::A);
    connect(d1Red, d1Or, Port::B);
    connect(d1Or, ffQ1, Port::D);

    // Clock drives both flip-flops.
    connect(clk, ffQ0, Port::CLK);
    connect(clk, ffQ1, Port::CLK);

    // R = !Q0 & !Q1 (initial state 00). Output order is R G Y.
    connect(notQ0, redA, Port::A);
    connect(notQ1, redA, Port::B);
    connect(redA, outR, Port::O);

    // G = Q0 & !Q1.
    connect(ffQ0, greenA, Port::A);
    connect(notQ1, greenA, Port::B);
    connect(greenA, outG, Port::O);

    // Y = !Q0 & Q1.
    connect(notQ0, yellowA, Port::A);
    connect(ffQ1, yellowA, Port::B);
    connect(yellowA, outY, Port::O);

    // Seed the circuit with initial values so inversions and outputs settle.
    en->commit(en->getResult());
    clk->commit(clk->getResult());
    ffQ0->commit(ffQ0->getResult());
    ffQ1->commit(ffQ1->getResult());
    Simulator::run();

    cout << "Traffic-Light FSM Interactive Testbench\n";
    cout << "Initial state: RED (R G Y = 1 0 0)\n";
    cout << "Enter binary values only: 0 or 1.\n\n";

    int cycles;
    cout << "Number of clock cycles to test: ";
    if (!(cin >> cycles) || cycles <= 0) {
        cerr << "Invalid cycle count.\n";
        return 1;
    }

    ofstream waveform("tests/worklet3/traffic_light_waveform.txt");
    if (!waveform) {
        cerr << "Cannot create tests/worklet3/traffic_light_waveform.txt\n";
        return 1;
    }
    waveform << "time clk en Q0 Q1 R G Y expectedR expectedG expectedY state_assertion output_assertion\n";
    waveform << "0 0 0 0 0 1 0 0 - - - INITIAL INITIAL\n";

    for (int i = 0; i < cycles; ++i) {
        int enValue, expectedR, expectedG, expectedY;

        int previousQ0 = asInt(ffQ0->getResult());
        int previousQ1 = asInt(ffQ1->getResult());
        string previousState = stateName(previousQ0, previousQ1);

        cout << "\nCycle " << (i + 1) << '\n';
        cout << "Previous state: " << previousState
             << " (Q0=" << previousQ0 << ", Q1=" << previousQ1 << ")\n";
        cout << "Enter en (0=hold, 1=advance): ";
        if (!(cin >> enValue) || (enValue != 0 && enValue != 1)) {
            cerr << "Invalid en value. Enter 0 or 1.\n";
            return 1;
        }

        cout << "Enter expected R G Y (three binary values separated by spaces): ";
        if (!(cin >> expectedR >> expectedG >> expectedY) ||
            (expectedR < 0 || expectedR > 1) ||
            (expectedG < 0 || expectedG > 1) ||
            (expectedY < 0 || expectedY > 1)) {
            cerr << "Invalid expected outputs. Enter R G Y, each 0 or 1.\n";
            return 1;
        }

        string expectedState = nextStateName(previousState, enValue);
        cout << "Input en: " << enValue << (enValue == 0 ? " (hold)\n" : " (advance)\n");
        cout << "Expected next state: " << expectedState << "\n";

        // Drive en while clock is LOW, then allow combinational logic to settle.
        en->evaluate(Port::O, bit(enValue));
        Simulator::run();

        // Rising edge: the flip-flops capture the calculated next state.
        clk->evaluate(Port::O, Signal::HIGH);
        Simulator::run();

        int actualR = asInt(outR->getResult());
        int actualG = asInt(outG->getResult());
        int actualY = asInt(outY->getResult());
        int q0 = asInt(ffQ0->getResult());
        int q1 = asInt(ffQ1->getResult());
        string actualState = stateName(q0, q1);

        cout << "Actual next state: " << actualState
             << " (Q0=" << q0 << ", Q1=" << q1 << ")\n";
        cout << "Expected outputs R G Y: " << expectedR << " " << expectedG << " " << expectedY << "\n";
        cout << "Actual outputs   R G Y: " << actualR << " " << actualG << " " << actualY << "\n";

        bool statePass = expectedState == actualState;
        cout << (statePass ? "[PASS] State transition\n" : "[FAIL] State transition: expected " + expectedState + ", got " + actualState + "\n");
        if (statePass) ++passed; else ++failed;

        string prefix = "Cycle " + to_string(i + 1) + " output ";
        check(prefix + "R", expectedR, actualR);
        check(prefix + "G", expectedG, actualG);
        check(prefix + "Y", expectedY, actualY);
        check("Exactly one traffic light is ON", 1, actualR + actualG + actualY);

        string stateAssertion = statePass ? "PASS" : "FAIL";
        string outputAssertion = (expectedR == actualR &&
                                  expectedG == actualG &&
                                  expectedY == actualY) ? "PASS" : "FAIL";

        waveform << (i * 2 + 1) << " 1 " << enValue << " "
                 << q0 << " " << q1 << " " << actualR << " "
                 << actualG << " " << actualY << " "
                 << expectedR << " " << expectedG << " " << expectedY
                 << " " << stateAssertion << " " << outputAssertion << '\n';

        // Falling edge prepares the circuit for the next cycle.
        clk->evaluate(Port::O, Signal::LOW);
        Simulator::run();

        waveform << (i * 2 + 2) << " 0 " << enValue << " "
                 << q0 << " " << q1 << " " << actualR << " "
                 << actualG << " " << actualY << " "
                 << expectedR << " " << expectedG << " " << expectedY
                 << " " << stateAssertion << " " << outputAssertion << '\n';
    }

    waveform.close();

    cout << "\n========== TEST SUMMARY ==========\n";
    cout << "Passed assertions: " << passed << '\n';
    cout << "Failed assertions: " << failed << '\n';
    cout << "Waveform saved to tests/worklet3/traffic_light_waveform.txt\n";

    return failed == 0 ? 0 : 1;
}

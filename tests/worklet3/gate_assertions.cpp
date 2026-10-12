#include <cassert>
#include <cstdlib>
#include <iostream>

#include "combinational/AND.hpp"
#include "combinational/OR.hpp"
#include "combinational/XOR.hpp"
#include "combinational/NOT.hpp"
#include "combinational/NAND.hpp"
#include "combinational/NOR.hpp"
#include "combinational/ANDNOT.hpp"
#include "combinational/ORNOT.hpp"
#include "combinational/MUX.hpp"

void check(bool condition, const char* gate,
           int a, int b, int expected, int actual) {
    if (condition) {
        std::cout << "[PASS] " << gate
                  << " A=" << a << " B=" << b
                  << " Expected=" << expected
                  << " Actual=" << actual << '\n';
    } else {
        std::cout << "[FAIL] " << gate
                  << " A=" << a << " B=" << b
                  << " Expected=" << expected
                  << " Actual=" << actual << '\n';
        std::exit(1);
    }
}

Signal signal(int value) {
    return value ? Signal::HIGH : Signal::LOW;
}

int value(Signal s) {
    return s == Signal::HIGH ? 1 : 0;
}

template <typename Gate, typename ExpectedFunction>
void testTwoInputGate(const char* name, ExpectedFunction expected) {
    for (int a = 0; a <= 1; ++a) {
        for (int b = 0; b <= 1; ++b) {
            Gate gate;
            gate.evaluate(Port::A, signal(a));
            gate.evaluate(Port::B, signal(b));

            int actual = value(gate.getResult());
            int wanted = expected(a, b);

            check(actual == wanted, name, a, b, wanted, actual);
        }
    }
}

void testAND() {
    testTwoInputGate<AND>("AND",
        [](int a, int b) { return a & b; });
}

void testOR() {
    testTwoInputGate<OR>("OR",
        [](int a, int b) { return a | b; });
}

void testXOR() {
    testTwoInputGate<XOR>("XOR",
        [](int a, int b) { return a ^ b; });
}

void testNAND() {
    testTwoInputGate<NAND>("NAND",
        [](int a, int b) { return !(a & b); });
}

void testNOR() {
    testTwoInputGate<NOR>("NOR",
        [](int a, int b) { return !(a | b); });
}

void testANDNOT() {
    testTwoInputGate<ANDNOT>("ANDNOT",
        [](int a, int b) { return a && !b; });
}

void testORNOT() {
    testTwoInputGate<ORNOT>("ORNOT",
        [](int a, int b) { return a || !b; });
}

void testNOT() {
    for (int a = 0; a <= 1; ++a) {
        NOT gate;
        gate.evaluate(Port::A, signal(a));

        int actual = value(gate.getResult());
        int expected = !a;

        check(actual == expected, "NOT", a, -1, expected, actual);
    }
}

void testMUX() {
    for (int a = 0; a <= 1; ++a) {
        for (int b = 0; b <= 1; ++b) {
            for (int s = 0; s <= 1; ++s) {
                MUX gate;
                gate.evaluate(Port::A, signal(a));
                gate.evaluate(Port::B, signal(b));
                gate.evaluate(Port::S, signal(s));

                int actual = value(gate.getResult());
                int expected = s ? b : a;

                check(actual == expected, "MUX", a, b,
                      expected, actual);
                std::cout << "       Select=" << s << '\n';
            }
        }
    }
}

int main() {
    std::cout << "=== Worklet 3: Gate Assertion Tests ===\n";

    testAND();
    testOR();
    testXOR();
    testNOT();
    testNAND();
    testNOR();
    testANDNOT();
    testORNOT();
    testMUX();

    std::cout << "\nAll gate assertion tests passed.\n";
    return 0;
}

// Hand-built circuits run through the full engine, checked against software models.

#include "support/helpers.hpp"

#include "combinational/AND.hpp"
#include "combinational/NOT.hpp"
#include "combinational/OR.hpp"
#include "combinational/XOR.hpp"
#include "sequential/DFF_P.hpp"

using namespace hdltest;

namespace {

unsigned readBus(const std::vector<OutputNode *> &bits) {
    unsigned v = 0;
    for (size_t i = 0; i < bits.size(); i++) v |= static_cast<unsigned>(Circuit::read(*bits[i])) << i;
    return v;
}

}  // namespace

TEST(circuits, sipo_shift_register) {
    // serial in -> ff0 -> ff1 -> ff2 -> ff3, every stage on the same clock
    Circuit c;
    auto &in = c.input("in");
    auto &clk = c.input("clk");
    std::vector<DFF_P *> ff;
    std::vector<OutputNode *> q;
    for (int i = 0; i < 4; i++) ff.push_back(&c.add<DFF_P>());
    for (int i = 0; i < 4; i++) q.push_back(&c.probe(*ff[i], "q" + std::to_string(i)));
    Circuit::connect(in, *ff[0], Port::D);
    for (int i = 1; i < 4; i++) Circuit::connect(*ff[i - 1], *ff[i], Port::D);
    for (auto *f : ff) Circuit::connect(clk, *f, Port::CLK);
    c.init();

    unsigned model = 0;
    for (int step = 0; step < 200; step++) {
        int bitIn = randomBit();
        Circuit::set(in, bitIn);
        Circuit::pulse(clk);
        model = ((model << 1) | bitIn) & 0xF;
        INFO("step " << step);
        REQUIRE_EQ(readBus(q), model);
    }
}

TEST(circuits, johnson_counter) {
    // D0 = NOT Q3, Di = Q(i-1): cycles through 8 states
    Circuit c;
    auto &clk = c.input("clk");
    std::vector<DFF_P *> ff;
    for (int i = 0; i < 4; i++) ff.push_back(&c.add<DFF_P>());
    auto &inv = c.add<NOT>();
    std::vector<OutputNode *> q;
    for (int i = 0; i < 4; i++) q.push_back(&c.probe(*ff[i], "q" + std::to_string(i)));
    for (auto *f : ff) Circuit::connect(clk, *f, Port::CLK);
    Circuit::connect(*ff[3], inv, Port::A);
    Circuit::connect(inv, *ff[0], Port::D);
    for (int i = 1; i < 4; i++) Circuit::connect(*ff[i - 1], *ff[i], Port::D);
    c.init();

    const unsigned expected[] = {0b0001, 0b0011, 0b0111, 0b1111, 0b1110, 0b1100, 0b1000, 0b0000};
    for (int cycle = 0; cycle < 24; cycle++) {
        Circuit::pulse(clk);
        INFO("cycle " << cycle);
        REQUIRE_EQ(readBus(q), expected[cycle % 8]);
    }
}

TEST(circuits, synchronous_binary_counter) {
    // Di = Qi XOR (Q0 AND ... AND Q(i-1))
    Circuit c;
    auto &clk = c.input("clk");
    std::vector<DFF_P *> ff;
    for (int i = 0; i < 4; i++) ff.push_back(&c.add<DFF_P>());
    for (auto *f : ff) Circuit::connect(clk, *f, Port::CLK);

    auto &n0 = c.add<NOT>();
    Circuit::connect(*ff[0], n0, Port::A);
    Circuit::connect(n0, *ff[0], Port::D);

    Node *carry = ff[0];
    for (int i = 1; i < 4; i++) {
        auto &x = c.add<XOR>();
        Circuit::connect(*ff[i], x, Port::A);
        Circuit::connect(*carry, x, Port::B);
        Circuit::connect(x, *ff[i], Port::D);
        if (i < 3) {
            auto &a = c.add<AND>();
            Circuit::connect(*carry, a, Port::A);
            Circuit::connect(*ff[i], a, Port::B);
            carry = &a;
        }
    }
    std::vector<OutputNode *> q;
    for (int i = 0; i < 4; i++) q.push_back(&c.probe(*ff[i], "q" + std::to_string(i)));
    c.init();

    for (int cycle = 1; cycle <= 40; cycle++) {
        Circuit::pulse(clk);
        INFO("cycle " << cycle);
        REQUIRE_EQ(readBus(q), static_cast<unsigned>(cycle % 16));
    }
}

TEST(circuits, ripple_counter_with_derived_clocks) {
    // stage i toggles (D = NOT Q) and is clocked by NOT Q(i-1): flip-flop
    // outputs drive other flip-flops' clocks through logic
    Circuit c;
    auto &clk = c.input("clk");
    std::vector<DFF_P *> ff;
    std::vector<NOT *> inv;
    for (int i = 0; i < 4; i++) {
        ff.push_back(&c.add<DFF_P>());
        inv.push_back(&c.add<NOT>());
        Circuit::connect(*ff[i], *inv[i], Port::A);
        Circuit::connect(*inv[i], *ff[i], Port::D);
        Circuit::connect(i == 0 ? static_cast<Node &>(clk) : *inv[i - 1], *ff[i], Port::CLK);
    }
    std::vector<OutputNode *> q;
    for (int i = 0; i < 4; i++) q.push_back(&c.probe(*ff[i], "q" + std::to_string(i)));
    c.init();

    // initialize() settles NOT outputs high, so stage clocks start high and the
    // first stage-to-stage edge happens when Q falls
    for (int cycle = 1; cycle <= 40; cycle++) {
        Circuit::pulse(clk);
        INFO("cycle " << cycle);
        REQUIRE_EQ(readBus(q), static_cast<unsigned>(cycle % 16));
    }
}

TEST(circuits, ripple_carry_adder_exhaustive) {
    // 4 full adders: s = a ^ b ^ cin, cout = (a & b) | (cin & (a ^ b))
    Circuit c;
    std::vector<InputNode *> a, b;
    for (int i = 0; i < 4; i++) a.push_back(&c.input("a"));
    for (int i = 0; i < 4; i++) b.push_back(&c.input("b"));
    auto &cin = c.input("cin");

    std::vector<OutputNode *> sum;
    Node *carry = &cin;
    std::vector<Node *> sumNodes;
    for (int i = 0; i < 4; i++) {
        auto &x1 = c.add<XOR>();
        Circuit::connect(*a[i], x1, Port::A);
        Circuit::connect(*b[i], x1, Port::B);
        auto &x2 = c.add<XOR>();
        Circuit::connect(x1, x2, Port::A);
        Circuit::connect(*carry, x2, Port::B);
        auto &g = c.add<AND>();
        Circuit::connect(*a[i], g, Port::A);
        Circuit::connect(*b[i], g, Port::B);
        auto &p = c.add<AND>();
        Circuit::connect(x1, p, Port::A);
        Circuit::connect(*carry, p, Port::B);
        auto &co = c.add<OR>();
        Circuit::connect(g, co, Port::A);
        Circuit::connect(p, co, Port::B);
        sumNodes.push_back(&x2);
        carry = &co;
    }
    for (int i = 0; i < 4; i++) sum.push_back(&c.probe(*sumNodes[i], "s"));
    sum.push_back(&c.probe(*carry, "cout"));
    c.init();

    for (unsigned r = 0; r < 512; r++) {
        unsigned av = r & 0xF, bv = (r >> 4) & 0xF, cv = r >> 8;
        std::vector<std::pair<InputNode *, int>> drive;
        for (int i = 0; i < 4; i++) {
            drive.push_back({a[i], (av >> i) & 1});
            drive.push_back({b[i], (bv >> i) & 1});
        }
        drive.push_back({&cin, static_cast<int>(cv)});
        Circuit::setAll(drive);
        INFO(av << " + " << bv << " + " << cv);
        CHECK_EQ(readBus(sum), av + bv + cv);
    }
}

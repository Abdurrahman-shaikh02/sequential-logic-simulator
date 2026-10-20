// Real Yosys netlists (tests/fixtures/netlists/<name>/{test.v,netlist.table}),
// parsed and simulated, checked against a software model of the Verilog.
//
// Combinational designs are checked exhaustively when they have at most 14
// input bits (walked in Gray-code order, so each step changes a single input
// bit, and then in random order); larger ones get random samples.

#include <functional>
#include <map>

#include "support/helpers.hpp"

using namespace hdltest;

namespace {

using Values = std::map<std::string, uint64_t>;
using Model = std::function<Values(const Values &)>;

std::string netlist(const std::string &name) { return fixture("netlists/" + name + "/netlist.table"); }

std::string describe(const Values &v) {
    std::string s;
    for (auto &[k, x] : v) s += k + "=" + std::to_string(x) + " ";
    return s;
}

void checkOutputs(const Values &in, const Values &expected) {
    for (auto &[name, want] : expected) {
        INFO("inputs " << describe(in) << "output " << name);
        CHECK_EQ(NetlistSim::get(name), want);
    }
}

void checkCombinational(const std::string &name, const std::vector<std::string> &inputs, const Model &model) {
    NetlistSim sim(netlist(name));

    std::vector<int> widths;
    int total = 0;
    for (auto &in : inputs) {
        widths.push_back(NetlistSim::width(in));
        total += widths.back();
    }
    REQUIRE_LE(total, 63);

    auto unpack = [&](uint64_t packed) {
        Values v;
        int shift = 0;
        for (size_t i = 0; i < inputs.size(); i++) {
            v[inputs[i]] = (packed >> shift) & ((1ULL << widths[i]) - 1);
            shift += widths[i];
        }
        return v;
    };

    std::vector<uint64_t> order;
    if (total <= 14) {
        uint64_t rows = 1ULL << total;
        for (uint64_t i = 0; i < rows; i++) order.push_back(i ^ (i >> 1));
        for (uint64_t i = 0; i < rows; i++) order.push_back(randomBits(total));
    } else {
        for (int i = 0; i < 4000; i++) order.push_back(randomBits(total));
    }

    for (uint64_t packed : order) {
        Values in = unpack(packed);
        for (auto &[n, v] : in) NetlistSim::drive(n, v);
        NetlistSim::settle();
        checkOutputs(in, model(in));
        if (hdltest::state().failures.size() > 20) FAIL("too many mismatches, stopping");
    }
}

uint64_t b(const Values &v, const std::string &k) { return v.at(k); }

}  // namespace

// ---------------------------------------------------------------------------
// combinational
// ---------------------------------------------------------------------------

TEST(netlists, and) {
    checkCombinational("and", {"a", "b"}, [](auto &v) { return Values{{"y", b(v, "a") & b(v, "b")}}; });
}

TEST(netlists, boolean_all_gate_kinds) {
    checkCombinational("boolean", {"a", "b"}, [](auto &v) {
        uint64_t a = b(v, "a"), bb = b(v, "b"), m = 0xF;
        return Values{{"y_and", a & bb},          {"y_nand", ~(a & bb) & m}, {"y_or", a | bb},
                      {"y_nor", ~(a | bb) & m},   {"y_xor", a ^ bb},         {"y_not", ~a & m},
                      {"y_andnot", a & ~bb & m},  {"y_ornot", (a | ~bb) & m}, {"y_buf", a}};
    });
}

TEST(netlists, mux2_bus) {
    checkCombinational("mux", {"a", "b", "sel"}, [](auto &v) {
        return Values{{"y", b(v, "sel") ? b(v, "b") : b(v, "a")}};
    });
}

TEST(netlists, mux4) {
    checkCombinational("mux4", {"a", "b", "c", "d", "sel0", "sel1"}, [](auto &v) {
        uint64_t s = b(v, "sel1") * 2 + b(v, "sel0");
        const char *pick[] = {"a", "b", "c", "d"};
        return Values{{"y", b(v, pick[s])}};
    });
}

TEST(netlists, mux8) {
    std::vector<std::string> ins = {"d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "sel0", "sel1", "sel2"};
    checkCombinational("mux8", ins, [](auto &v) {
        uint64_t s = b(v, "sel2") * 4 + b(v, "sel1") * 2 + b(v, "sel0");
        return Values{{"y", b(v, "d" + std::to_string(s))}};
    });
}

TEST(netlists, mux16) {
    std::vector<std::string> ins;
    for (int i = 0; i < 16; i++) ins.push_back("d" + std::to_string(i));
    for (int i = 0; i < 4; i++) ins.push_back("sel" + std::to_string(i));
    checkCombinational("mux16", ins, [](auto &v) {
        uint64_t s = b(v, "sel3") * 8 + b(v, "sel2") * 4 + b(v, "sel1") * 2 + b(v, "sel0");
        return Values{{"y", b(v, "d" + std::to_string(s))}};
    });
}

TEST(netlists, decoder_2_to_4) {
    checkCombinational("decode2-4", {"a", "b"}, [](auto &v) {
        uint64_t idx = b(v, "a") * 2 + b(v, "b");
        return Values{{"y0", idx == 0}, {"y1", idx == 1}, {"y2", idx == 2}, {"y3", idx == 3}};
    });
}

TEST(netlists, encoder_4_to_2) {
    checkCombinational("encoder", {"y0", "y1", "y2", "y3"}, [](auto &v) {
        return Values{{"a", b(v, "y1") | b(v, "y3")}, {"b", b(v, "y2") | b(v, "y3")}};
    });
}

TEST(netlists, comparator) {
    checkCombinational("comparator", {"a", "b"}, [](auto &v) {
        uint64_t a = b(v, "a"), bb = b(v, "b");
        return Values{{"eq", a == bb}, {"gt", a > bb}, {"lt", a < bb}};
    });
}

TEST(netlists, adder) {
    checkCombinational("adder", {"a", "b"}, [](auto &v) { return Values{{"y", (b(v, "a") + b(v, "b")) & 0xF}}; });
}

TEST(netlists, subtractor) {
    checkCombinational("subtractor", {"a", "b"}, [](auto &v) { return Values{{"y", (b(v, "a") - b(v, "b")) & 0xF}}; });
}

TEST(netlists, alu) {
    checkCombinational("alu", {"a", "b", "sel"}, [](auto &v) {
        uint64_t a = b(v, "a"), bb = b(v, "b"), y = 0;
        switch (b(v, "sel")) {
            case 0: y = a + bb; break;
            case 1: y = a - bb; break;
            case 2: y = a & bb; break;
            case 3: y = a ^ bb; break;
        }
        return Values{{"y", y & 0xF}};
    });
}

TEST(netlists, arbitrary_combinational) {
    checkCombinational("arbitrarycombinational", {"a", "b", "c", "d", "sel"}, [](auto &v) {
        uint64_t a = b(v, "a"), bb = b(v, "b"), c = b(v, "c"), d = b(v, "d"), m = 0xF;
        uint64_t x = ((a & bb) ^ ~c) & m;
        uint64_t z = (c | d) & (a ^ d);
        uint64_t sel = b(v, "sel");
        uint64_t selected = sel == 0 ? x : sel == 1 ? z : sel == 2 ? (x ^ z) : (x & z);
        return Values{{"y", (selected ^ (a | d)) & m}};
    });
}

// ---------------------------------------------------------------------------
// sequential
// ---------------------------------------------------------------------------

TEST(netlists, dff) {
    NetlistSim sim(netlist("dffp"));
    Capture err(std::cerr);
    uint64_t q = 0;
    for (int step = 0; step < 300; step++) {
        uint64_t d = randomBit();
        NetlistSim::set("d", d);
        NetlistSim::clock();
        q = d;
        INFO("step " << step);
        REQUIRE_EQ(NetlistSim::get("q"), q);
    }
}

// Switches on once include/sequential/DFF_N.hpp is non-empty (see the Makefile);
// $_DFF_N_ must also be uncommented in Parser.cpp's compTypeTable.
#ifdef HDLSIM_HAVE_DFF_N
TEST(netlists, dff_negedge) {
    NetlistSim sim(netlist("dffn"));
    Capture err(std::cerr);
    uint64_t q = 0;
    for (int step = 0; step < 300; step++) {
        uint64_t d = randomBit();
        NetlistSim::set("d", d);
        NetlistSim::set("clk", 1);
        INFO("step " << step);
        REQUIRE_EQ(NetlistSim::get("q"), q);   // rising edge: nothing
        NetlistSim::set("clk", 0);
        q = d;
        REQUIRE_EQ(NetlistSim::get("q"), q);
    }
}
#endif

TEST(netlists, shift_register) {
    NetlistSim sim(netlist("shiftregister"));
    REQUIRE_EQ(NetlistSim::width("q"), 4);
    uint64_t model = 0;
    for (int step = 0; step < 200; step++) {
        uint64_t in = randomBit();
        NetlistSim::set("serial_in", in);
        NetlistSim::clock();
        model = ((model << 1) | in) & 0xF;
        INFO("step " << step);
        REQUIRE_EQ(NetlistSim::get("q"), model);
    }
}

TEST(netlists, parallel_register) {
    NetlistSim sim(netlist("parallelregister"));
    uint64_t q = 0;
    for (int step = 0; step < 200; step++) {
        uint64_t d = randomBits(4);
        NetlistSim::set("d", d);
        REQUIRE_EQ(NetlistSim::get("q"), q);   // nothing changes before the edge
        NetlistSim::clock();
        q = d;
        INFO("step " << step);
        REQUIRE_EQ(NetlistSim::get("q"), q);
    }
}

TEST(netlists, binary_up_counter) {
    NetlistSim sim(netlist("binaryupcounter"));
    REQUIRE_EQ(NetlistSim::get("q"), 0u);
    for (int cycle = 1; cycle <= 40; cycle++) {
        NetlistSim::clock();
        INFO("cycle " << cycle);
        REQUIRE_EQ(NetlistSim::get("q"), static_cast<uint64_t>(cycle % 16));
    }
}

TEST(netlists, up_down_counter) {
    NetlistSim sim(netlist("updowncounter"));
    uint64_t count = 0;
    for (int step = 0; step < 300; step++) {
        uint64_t up = randomBit();
        NetlistSim::set("up", up);
        NetlistSim::clock();
        count = (up ? count + 1 : count - 1) & 0xF;
        INFO("step " << step << " up=" << up);
        REQUIRE_EQ(NetlistSim::get("q"), count);
    }
}

TEST(netlists, ring_counter_stays_zero_until_reset) {
    // DFFs power up at 0, and rotating 0000 gives 0000: reset is what loads the 1
    NetlistSim sim(netlist("ringcounter"));
    for (int cycle = 0; cycle <= 8; cycle++) {
        INFO("cycle " << cycle);
        REQUIRE_EQ(NetlistSim::get("q"), 0u);
        NetlistSim::clock();
    }
}

TEST(netlists, ring_counter_reset_is_synchronous) {
    NetlistSim sim(netlist("ringcounter"));
    NetlistSim::set("rst", 1);
    CHECK_EQ(NetlistSim::get("q"), 0u);   // nothing happens before the clock edge
    NetlistSim::clock();
    CHECK_EQ(NetlistSim::get("q"), 1u);
    NetlistSim::clock();
    CHECK_EQ(NetlistSim::get("q"), 1u);   // held in 0001 while rst stays high
}

TEST(netlists, ring_counter_rotates_after_reset) {
    // 0001 -> 0010 -> 0100 -> 1000 -> 0001
    NetlistSim sim(netlist("ringcounter"));
    NetlistSim::set("rst", 1);
    NetlistSim::clock();
    NetlistSim::set("rst", 0);
    REQUIRE_EQ(NetlistSim::get("q"), 1u);
    for (int cycle = 1; cycle <= 12; cycle++) {
        NetlistSim::clock();
        INFO("cycle " << cycle);
        REQUIRE_EQ(NetlistSim::get("q"), 1ULL << (cycle % 4));
    }
}

TEST(netlists, ring_counter_random_reset_matches_model) {
    NetlistSim sim(netlist("ringcounter"));
    uint64_t q = 0;
    for (int step = 0; step < 300; step++) {
        uint64_t rst = randomBits(3) == 0;   // reset about one cycle in eight
        NetlistSim::set("rst", rst);
        NetlistSim::clock();
        q = rst ? 1 : ((q << 1) | (q >> 3)) & 0xF;
        INFO("step " << step << " rst=" << rst);
        REQUIRE_EQ(NetlistSim::get("q"), q);
    }
}

TEST(netlists, traffic_light_fsm) {
    // RED(0) -> GREEN(1) -> YELLOW(2) -> RED
    NetlistSim sim(netlist("trafficlight"));
    REQUIRE_EQ(NetlistSim::get("state"), 0u);
    for (int cycle = 1; cycle <= 12; cycle++) {
        NetlistSim::clock();
        INFO("cycle " << cycle);
        REQUIRE_EQ(NetlistSim::get("state"), static_cast<uint64_t>(cycle % 3));
    }
}

TEST(netlists, sequence_detector) {
    // detects "...1 0 1": in S2 (just saw 1,0) with din = 1, combinationally (Mealy)
    NetlistSim sim(netlist("sequencedetector"));
    int state = 0;
    for (int step = 0; step < 500; step++) {
        int din = randomBit();
        NetlistSim::set("din", din);
        INFO("step " << step << " state=" << state << " din=" << din);
        REQUIRE_EQ(NetlistSim::get("detected"), static_cast<uint64_t>(state == 2 && din));
        NetlistSim::clock();
        state = state == 0 ? (din ? 1 : 0) : state == 1 ? (din ? 1 : 2) : (din ? 1 : 0);
    }
}

TEST(netlists, vending_machine) {
    NetlistSim sim(netlist("vendingmachine"));
    int state = 0;   // 0 = S0, 1 = S5
    for (int step = 0; step < 500; step++) {
        int c5 = randomBit(), c10 = randomBit();
        NetlistSim::drive("coin5", c5);
        NetlistSim::drive("coin10", c10);
        NetlistSim::settle();
        bool vend = (state == 0 && c10) || (state == 1 && (c5 || c10));
        INFO("step " << step << " state=" << state << " coin5=" << c5 << " coin10=" << c10);
        REQUIRE_EQ(NetlistSim::get("vend"), static_cast<uint64_t>(vend));
        NetlistSim::clock();
        if (state == 0) state = c10 ? 0 : c5 ? 1 : 0;
        else state = (c5 || c10) ? 0 : 1;
    }
}

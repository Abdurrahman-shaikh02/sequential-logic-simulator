// ============================================================
// TEST 05: 16:1 Multiplexer
// ============================================================
// Purpose:
//   Check how Yosys represents a behavioral 16:1 MUX.
//
// We are NOT trying to force $_MUX16_.
// We want to see whether Yosys naturally produces a tree of
// $_MUX_ cells, as it did for the 4:1 and 8:1 cases.
//
// Selection:
//   0000 -> d0
//   0001 -> d1
//   ...
//   1111 -> d15
//
// All signals are scalar (1-bit).
// ============================================================

module test_mux16 (
    input d0,
    input d1,
    input d2,
    input d3,
    input d4,
    input d5,
    input d6,
    input d7,
    input d8,
    input d9,
    input d10,
    input d11,
    input d12,
    input d13,
    input d14,
    input d15,

    input sel0,
    input sel1,
    input sel2,
    input sel3,

    output y
);

    assign y = sel3
             ? (sel2
                 ? (sel1
                     ? (sel0 ? d15 : d14)
                     : (sel0 ? d13 : d12))
                 : (sel1
                     ? (sel0 ? d11 : d10)
                     : (sel0 ? d9 : d8)))
             : (sel2
                 ? (sel1
                     ? (sel0 ? d7 : d6)
                     : (sel0 ? d5 : d4))
                 : (sel1
                     ? (sel0 ? d3 : d2)
                     : (sel0 ? d1 : d0)));

endmodule

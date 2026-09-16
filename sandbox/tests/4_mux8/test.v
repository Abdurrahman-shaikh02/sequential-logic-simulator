// ============================================================
// TEST 04: 8:1 Multiplexer
// ============================================================
// Purpose:
//   Check how Yosys represents a behavioral 8:1 MUX in the
//   final primitive netlist.
//
// We are NOT specifically trying to force $_MUX8_.
// We want to see what Yosys naturally produces.
//
// Inputs:
//   d0 ... d7 : data inputs
//   sel[2:0]  : select inputs
//
// Selection:
//   000 -> d0
//   001 -> d1
//   010 -> d2
//   011 -> d3
//   100 -> d4
//   101 -> d5
//   110 -> d6
//   111 -> d7
//
// Signals are scalar (1-bit).
// ============================================================

module test_mux8 (
    input d0,
    input d1,
    input d2,
    input d3,
    input d4,
    input d5,
    input d6,
    input d7,

    input sel0,
    input sel1,
    input sel2,

    output y
);

    assign y = sel2
             ? (sel1
                 ? (sel0 ? d7 : d6)
                 : (sel0 ? d5 : d4))
             : (sel1
                 ? (sel0 ? d3 : d2)
                 : (sel0 ? d1 : d0));

endmodule

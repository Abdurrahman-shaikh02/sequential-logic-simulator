// ============================================================
// TEST 03: 4:1 Multiplexer
// ============================================================
// Purpose:
//   Verify that a 4:1 MUX is correctly lowered by Yosys.
//
// Function:
//   sel1 sel0
//     00  -> a
//     01  -> b
//     10  -> c
//     11  -> d
//
// Signals are scalar (1-bit).
// ============================================================

module test_mux4 (
    input a,
    input b,
    input c,
    input d,

    input sel0,
    input sel1,

    output y
);

    assign y = sel1
             ? (sel0 ? d : c)
             : (sel0 ? b : a);

endmodule

// ============================================================
// TEST 02: 4-bit 2:1 Multiplexer
// ============================================================
// Purpose:
//   Test vector-width handling of $_MUX_.
//
//   When sel = 0: y = a
//   When sel = 1: y = b
//
// Expected:
//   4 x $_MUX_
// ============================================================

module test_mux2 (
    input  [3:0] a,
    input  [3:0] b,
    input        sel,

    output [3:0] y
);

    assign y = sel ? b : a;

endmodule

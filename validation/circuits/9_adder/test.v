// ============================================================
// TEST 09: 4-bit Adder
// ============================================================
// Purpose:
//   Test synthesis of the '+' operator into primitive gates.
//
// Inputs:
//   a, b : 4-bit operands
//
// Output:
//   y    : 4-bit sum
//
// NOTE:
//   We intentionally keep only the 4-bit result.
//   The carry-out is discarded.
//
// This gives us useful edge cases:
//
//   0 + 0
//   0 + maximum
//   maximum + 1  -> wraps around
//   maximum + maximum -> wraps around
//
// Expected final netlist:
//   Only cells from our supported primitive set.
// ============================================================

module test_adder (
    input  [3:0] a,
    input  [3:0] b,

    output [3:0] y
);

    assign y = a + b;

endmodule

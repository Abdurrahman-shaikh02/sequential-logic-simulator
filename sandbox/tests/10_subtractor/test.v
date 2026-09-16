// ============================================================
// TEST 10: 4-bit Subtractor
// ============================================================
// Purpose:
//   Test synthesis of the '-' operator into primitive gates.
//
// The result is 4 bits, so subtraction naturally wraps around
// modulo 16.
//
// Interesting cases:
//
//   0 - 0
//   5 - 3
//   3 - 5       -> wraps around
//   0 - 1       -> wraps around
//   15 - 1
//   15 - 15
//
// We deliberately do not expose a borrow output.
// ============================================================

module test_subtractor (
    input  [3:0] a,
    input  [3:0] b,

    output [3:0] y
);

    assign y = a - b;

endmodule

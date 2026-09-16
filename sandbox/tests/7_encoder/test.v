// ============================================================
// TEST 07: 4-to-2 Encoder
// ============================================================
// Purpose:
//   Check how Yosys synthesizes an encoder.
//
// Inputs:
//   y0-y3 : one-hot input
//
// Outputs:
//   a, b  : binary encoded result
//
// Expected encoding:
//
//   y3 y2 y1 y0 | b a
//   ------------+----
//    0  0  0  1 | 0 0
//    0  0  1  0 | 0 1
//    0  1  0  0 | 1 0
//    1  0  0  0 | 1 1
//
// Assumption:
//   Exactly one input is asserted at a time.
//   We are testing the normal encoder, not a priority encoder.
//
// Signals are scalar (1-bit).
// ============================================================

module test_encoder (
    input y0,
    input y1,
    input y2,
    input y3,

    output a,
    output b
);

    // Least-significant output bit.
    // 1 for inputs y1 and y3.
    assign a = y1 | y3;

    // Most-significant output bit.
    // 1 for inputs y2 and y3.
    assign b = y2 | y3;

endmodule

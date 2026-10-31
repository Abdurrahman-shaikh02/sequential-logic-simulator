// ============================================================
// TEST 06: 2-to-4 Decoder
// ============================================================
// Purpose:
//   Check how Yosys synthesizes a decoder.
//
// Inputs:
//   a, b : 2-bit encoded input
//
// Outputs:
//   y0-y3 : one-hot decoded outputs
//
// Mapping:
//
//   a b | y0 y1 y2 y3
//   ----+------------
//   0 0 |  1  0  0  0
//   0 1 |  0  1  0  0
//   1 0 |  0  0  1  0
//   1 1 |  0  0  0  1
//
// We write the decoder explicitly using Boolean expressions.
// This lets us see which primitive gates Yosys generates.
//
// Signals are scalar (1-bit).
// ============================================================

module test_decoder (
    input a,
    input b,

    output y0,
    output y1,
    output y2,
    output y3
);

    // 00
    assign y0 = ~a & ~b;

    // 01
    assign y1 = ~a & b;

    // 10
    assign y2 = a & ~b;

    // 11
    assign y3 = a & b;

endmodule

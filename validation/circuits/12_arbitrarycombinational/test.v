// ============================================================
// TEST 12: Arbitrary combinational circuit
// ============================================================
// Purpose:
//   Stress the combinational synthesis path with a mixture of
//   Boolean operations and conditional selection.
//
// This is intentionally not a named circuit such as an adder,
// decoder, etc. The goal is to test arbitrary RTL expressions.
//
// Inputs:
//   a, b, c, d : 4-bit operands
//   sel        : 2-bit selector
//
// Operations:
//   x = (a & b) ^ (~c)
//   z = (c | d) & (a ^ d)
//
// Then select between x and z and apply another Boolean
// operation.
//
// This should exercise:
//   - AND
//   - OR
//   - XOR
//   - NOT
//   - MUX
//   - multi-level combinational dependencies
//   - 4-bit signals
// ============================================================

module test_arbitrary_comb (
    input  [3:0] a,
    input  [3:0] b,
    input  [3:0] c,
    input  [3:0] d,
    input  [1:0] sel,

    output [3:0] y
);

    wire [3:0] x;
    wire [3:0] z;
    wire [3:0] selected;

    assign x = (a & b) ^ (~c);
    assign z = (c | d) & (a ^ d);

    assign selected = (sel == 2'b00) ? x :
                      (sel == 2'b01) ? z :
                      (sel == 2'b10) ? (x ^ z) :
                                       (x & z);

    assign y = selected ^ (a | d);

endmodule

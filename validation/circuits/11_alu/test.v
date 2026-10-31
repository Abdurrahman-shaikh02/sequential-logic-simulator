// ============================================================
// TEST 11: 4-bit ALU-like combinational circuit
// ============================================================
// sel:
//   00 -> a + b
//   01 -> a - b
//   10 -> a & b
//   11 -> a ^ b
//
// This tests:
//   - 4-bit arithmetic
//   - 4-bit Boolean operations
//   - nested selection / MUX logic
//
// The result is only 4 bits.
// Arithmetic overflow/borrow is intentionally discarded.
// ============================================================

module test_alu (
    input  [3:0] a,
    input  [3:0] b,
    input  [1:0] sel,

    output [3:0] y
);

    always @(*) begin
        case (sel)
            2'b00: y = a + b;
            2'b01: y = a - b;
            2'b10: y = a & b;
            2'b11: y = a ^ b;
        endcase
    end

endmodule

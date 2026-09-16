// ============================================================
// TEST 18: 4-bit up/down counter
// ============================================================
// up = 1 -> count upward
// up = 0 -> count downward
//
// Tests:
//   - sequential state
//   - arithmetic
//   - feedback
//   - conditional selection
//   - interaction between combinational and sequential logic
// ============================================================

module test_up_down_counter (
    input        clk,
    input        up,
    output [3:0] q
);

    reg [3:0] count;

    always @(posedge clk) begin
        if (up)
            count <= count + 4'b0001;
        else
            count <= count - 4'b0001;
    end

    assign q = count;

endmodule

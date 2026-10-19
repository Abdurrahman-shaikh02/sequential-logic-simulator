// ============================================================
// TEST 19: 4-bit ring counter
// ============================================================
// Each clock shifts the '1' to the next position:
//
//   0001 -> 0010 -> 0100 -> 1000 -> 0001 -> ...
//
// Tests:
//   - multiple DFFs
//   - sequential feedback
//   - combinational wiring
// ============================================================

module test_ring_counter (
    input        clk,
    output [3:0] q
);

    reg [3:0] state = 4'b0001;

    always @(posedge clk) begin
        state <= {state[2:0], state[3]};
    end

    assign q = state;

endmodule

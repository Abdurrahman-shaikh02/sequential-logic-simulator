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
    input  wire       clk,
    input  wire       rst,
    output reg  [3:0] q
);

always @(posedge clk) begin
    if (rst)
        q <= 4'b0001;
    else
        q <= {q[2:0], q[3]};
end

endmodule

// ============================================================
// TEST 13: Positive-edge D flip-flop
// ============================================================
// Tests:
//   - state storage
//   - positive clock edge
//   - D -> Q transfer
//   - Q retaining its previous value between clock edges
// ============================================================

module test_dff_pos (
    input  clk,
    input  d,
    output reg q
);

    always @(posedge clk) begin
        q <= d;
    end

endmodule

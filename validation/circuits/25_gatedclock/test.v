// Gated clock: the flop's clock is clk AND a primary input
module test_gated_clock (
    input      clk,
    input      en,
    input      d,
    output reg q
);

    initial q = 1'b0;

    wire gated = clk & en;

    always @(posedge gated) q <= d;

endmodule

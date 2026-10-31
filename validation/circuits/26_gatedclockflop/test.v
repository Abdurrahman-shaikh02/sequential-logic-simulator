// Gated clock: the flop's clock is clk AND another flop's output.
// t changes on the falling edge, so the gate only opens while clk is low.
module test_gated_clock_flop (
    input      clk,
    input      d,
    output reg t,
    output reg q
);

    initial begin
        t = 1'b0;
        q = 1'b0;
    end

    wire gated = clk & t;

    always @(negedge clk)   t <= d;
    always @(posedge gated) q <= d;

endmodule

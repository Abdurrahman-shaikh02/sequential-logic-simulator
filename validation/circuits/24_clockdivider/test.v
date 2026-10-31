// Clock divider: clk/2 clocks a flop that samples a primary input
module test_clock_divider (
    input      clk,
    input      d,
    output reg div,
    output reg q
);

    initial begin
        div = 1'b0;
        q = 1'b0;
    end

    always @(posedge clk) div <= ~div;
    always @(posedge div) q <= d;

endmodule

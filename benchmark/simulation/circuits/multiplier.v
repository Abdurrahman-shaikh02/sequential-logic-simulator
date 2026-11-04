// generate.py sets WIDTH for each size.
module top #(parameter WIDTH = 8) (
    input clk,
    input [WIDTH-1:0] a,
    input [WIDTH-1:0] b,
    output reg [2*WIDTH-1:0] p
);
    reg [WIDTH-1:0] ra = 0, rb = 0;
    initial p = 0;

    always @(posedge clk) begin
        ra <= a;
        rb <= b;
        p <= ra * rb;
    end
endmodule

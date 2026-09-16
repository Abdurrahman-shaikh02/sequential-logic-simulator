module test(
    input clk,
    input enable,
    input d,
    output reg q
);

always @(posedge clk) begin
    if (enable)
        q <= d;
end

endmodule

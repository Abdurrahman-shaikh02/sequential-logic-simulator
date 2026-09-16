module test_dff_neg (
    input clk,
    input d,
    output reg q
);

    always @(negedge clk) begin
        q <= d;
    end

endmodule

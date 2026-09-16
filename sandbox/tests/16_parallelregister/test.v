module test_parallel_register (
    input        clk,
    input  [3:0] d,
    output [3:0] q
);

    reg [3:0] state;

    always @(posedge clk) begin
        state <= d;
    end

    assign q = state;

endmodule

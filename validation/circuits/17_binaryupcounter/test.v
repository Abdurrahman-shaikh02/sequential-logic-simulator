module test_binary_counter (
    input        clk,
    output [3:0] q
);

    reg [3:0] count;

    always @(posedge clk) begin
        count <= count + 4'b0001;
    end

    assign q = count;

endmodule

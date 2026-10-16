module test_shift_register (
    input        clk,
    input        serial_in,
    output [3:0] q
);

    reg [3:0] state;

    always @(posedge clk) begin
        state[0] <= serial_in;
        state[1] <= state[0];
        state[2] <= state[1];
        state[3] <= state[2];
    end

    assign q = state;

endmodule

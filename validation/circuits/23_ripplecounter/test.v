// Ripple counter: each flop is clocked by the previous flop's output
module test_ripple_counter (
    input            clk,
    output reg [3:0] q
);

    initial q = 4'b0000;

    always @(posedge clk)  q[0] <= ~q[0];
    always @(posedge q[0]) q[1] <= ~q[1];
    always @(posedge q[1]) q[2] <= ~q[2];
    always @(posedge q[2]) q[3] <= ~q[3];

endmodule

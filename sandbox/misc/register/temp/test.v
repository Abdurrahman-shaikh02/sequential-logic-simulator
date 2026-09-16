module dff (
    input clk,
    input d,
    output reg q
);
    always @(posedge clk)
        q <= d;
endmodule


module register4 (
    input        clk,
    input  [3:0] d,
    output [3:0] q
);

    dff ff0 (
        .clk(clk),
        .d(d[0]),
        .q(q[0])
    );

    dff ff1 (
        .clk(clk),
        .d(d[1]),
        .q(q[1])
    );

    dff ff2 (
        .clk(clk),
        .d(d[2]),
        .q(q[2])
    );

    dff ff3 (
        .clk(clk),
        .d(d[3]),
        .q(q[3])
    );

endmodule

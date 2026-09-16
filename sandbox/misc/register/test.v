module dff (
    input clk,
    input d,
    output reg q
);
    always @(posedge clk)
        q <= d;
endmodule


module register4 (
    input clk,
    input d0,
    input d1,
    input d2,
    input d3,

    output q0,
    output q1,
    output q2,
    output q3
);

    dff ff0 (
        .clk(clk),
        .d(d0),
        .q(q0)
    );

    dff ff1 (
        .clk(clk),
        .d(d1),
        .q(q1)
    );

    dff ff2 (
        .clk(clk),
        .d(d2),
        .q(q2)
    );

    dff ff3 (
        .clk(clk),
        .d(d3),
        .q(q3)
    );

endmodule

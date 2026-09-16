module dff (
    input clk,
    input d,
    output reg q
);
    always @(posedge clk)
        q <= d;
endmodule


module counter4 (
    input clk,

    output q0,
    output q1,
    output q2,
    output q3
);

    wire d0;
    wire d1;
    wire d2;
    wire d3;

    wire c1;
    wire c2;
    wire c3;

    wire c4;

    // Bit 0: q0 toggles every clock
    assign d0 = ~q0;

    // Bit 1
    assign d1 = q1 ^ q0;

    // Bit 2
    assign c1 = q1 & q0;
    assign d2 = q2 ^ c1;

    // Bit 3
    assign c2 = q2 & q1;
    assign c3 = c2 & q0;
    assign d3 = q3 ^ c3;

    // State registers
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

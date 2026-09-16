module HalfAdder (
    input  A,
    input  B,
    output S,
    output C
);

    assign S = A ^ B;
    assign C = A & B;

endmodule

module FullAdder (
    input A,
    input B,
    input Cin,
    output Sum,
    output Cout
);

    wire S1;
    wire C1;
    wire C2;

    HalfAdder HA1 (
        .A(A),
        .B(B),
        .S(S1),
        .C(C1)
    );

    HalfAdder HA2 (
        .A(S1),
        .B(Cin),
        .S(Sum),
        .C(C2)
    );

    assign Cout = C1 | C2;

endmodule

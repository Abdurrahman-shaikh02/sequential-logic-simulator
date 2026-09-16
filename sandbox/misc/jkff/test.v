module jk_flip_flop (
    input  wire J,
    input  wire K,
    input  wire clk,
    input  wire reset,
    output reg  Q,
    output wire Qbar
);

    wire D;

    // JK characteristic equation:
    // Q(next) = J.Qbar + Kbar.Q
    assign D = (J & ~Q) | (~K & Q);

    // D flip-flop provides the storage
    always @(posedge clk or posedge reset) begin
        if (reset)
            Q <= 1'b0;
        else
            Q <= D;
    end

    assign Qbar = ~Q;

endmodule

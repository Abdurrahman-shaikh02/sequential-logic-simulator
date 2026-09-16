module test_sequence_detector (
    input  clk,
    input  din,
    output detected
);

    reg [1:0] state;

    localparam S0 = 2'b00;
    localparam S1 = 2'b01;
    localparam S2 = 2'b10;

    always @(posedge clk) begin
        case (state)
            S0: begin
                if (din)
                    state <= S1;
                else
                    state <= S0;
            end

            S1: begin
                if (din)
                    state <= S1;
                else
                    state <= S2;
            end

            S2: begin
                if (din)
                    state <= S1;
                else
                    state <= S0;
            end

            default: state <= S0;
        endcase
    end

    assign detected = (state == S2) && din;

endmodule

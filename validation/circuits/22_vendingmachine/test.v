module test_vending_machine (
    input  clk,
    input coin5,
    input coin10,
    output vend
);

    reg [1:0] state;

    localparam S0 = 2'b00;
    localparam S5 = 2'b01;

    // State transition logic
    always @(posedge clk) begin
        case (state)

            // No credit
            S0: begin
                if (coin10)
                    state <= S0;   // Vend immediately, then reset
                else if (coin5)
                    state <= S5;   // Accumulate 5
                else
                    state <= S0;
            end

            // 5 units inserted
            S5: begin
                if (coin5)
                    state <= S0;   // 10 reached -> vend
                else if (coin10)
                    state <= S0;   // 15 -> vend, reset
                else
                    state <= S5;
            end

            default:
                state <= S0;

        endcase
    end

    // Vend whenever:
    //   - currently at S0 and a 10-unit coin arrives
    //   - currently at S5 and another coin arrives
    assign vend =
        (state == S0 && coin10) ||
        (state == S5 && (coin5 || coin10));

endmodule

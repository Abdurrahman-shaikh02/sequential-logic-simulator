module test_traffic_fsm (
    input        clk,
    input        reset,
    input        en,
    output [2:0] state
);

    reg [2:0] current_state;

    localparam RED    = 3'b100;
    localparam GREEN  = 3'b001;
    localparam YELLOW = 3'b010;

    // Initialize state to RED
    initial begin
        current_state = RED;
    end

    always @(posedge clk) begin
        if (reset)
            current_state <= RED;
        else if (en) begin
            case (current_state)
                RED:    current_state <= GREEN;
                GREEN:  current_state <= YELLOW;
                YELLOW: current_state <= RED;
                default: current_state <= RED;
            endcase
        end
    end

    assign state = current_state;

endmodule

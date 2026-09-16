// ============================================================
// TEST 20: Traffic-light FSM
// ============================================================
// States:
//
//   00 = RED
//   01 = GREEN
//   10 = YELLOW
//
// Sequence:
//
//   RED -> GREEN -> YELLOW -> RED -> ...
//
// Tests:
//   - multiple sequential state bits
//   - next-state combinational logic
//   - feedback
//   - FSM-style case statement
// ============================================================

module test_traffic_fsm (
    input        clk,
    output [1:0] state
);

    reg [1:0] current_state;

    localparam RED    = 2'b00;
    localparam GREEN  = 2'b01;
    localparam YELLOW = 2'b10;

    always @(posedge clk) begin
        case (current_state)
            RED:    current_state <= GREEN;
            GREEN:  current_state <= YELLOW;
            YELLOW: current_state <= RED;
            default: current_state <= RED;
        endcase
    end

    assign state = current_state;

endmodule

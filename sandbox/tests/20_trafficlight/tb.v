module tb_test_traffic_fsm;

    reg clk, en, reset;
    wire [2:0] state;

    test_traffic_fsm dut (
        .clk(clk),
        .reset(reset),
        .en(en),
        .state(state)
    );

    initial begin
        clk = 0;
        en = 0;
        reset = 0;

        $display("// outputs: state[2:0]");

        // Initial output
        //$display("%03b", state);

        // inputs: clk en reset
        // reset on the first edge so both simulators start from a known state
        // (the netlist does not carry the initial value; row 1 is power-up and may differ)
        clk = 0; en = 1; reset = 1; #1; $display("%03b", state);
        clk = 1; en = 1; reset = 1; #1; $display("%03b", state);

        clk = 0; en = 1; reset = 0; #1; $display("%03b", state);
        clk = 1; en = 1; reset = 0; #1; $display("%03b", state);

        clk = 0; en = 1; reset = 0; #1; $display("%03b", state);
        clk = 1; en = 1; reset = 0; #1; $display("%03b", state);

        clk = 0; en = 1; reset = 0; #1; $display("%03b", state);
        clk = 1; en = 1; reset = 0; #1; $display("%03b", state);

        clk = 0; en = 1; reset = 0; #1; $display("%03b", state);
        clk = 1; en = 1; reset = 0; #1; $display("%03b", state);

        clk = 0; en = 1; reset = 1; #1; $display("%03b", state);
        clk = 1; en = 1; reset = 1; #1; $display("%03b", state);

        clk = 0; en = 1; reset = 0; #1; $display("%03b", state);

        $finish;
    end

endmodule

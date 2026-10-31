module tb_test_shift_register;

    reg clk;
    reg serial_in;
    wire [3:0] q;

    test_shift_register dut (
        .clk(clk),
        .serial_in(serial_in),
        .q(q)
    );

    initial begin
        clk = 0;
        serial_in = 0;

        $display("// outputs: q[3:0]");

        // inputs: clk serial_in
        clk = 0; serial_in = 1; #1; $display("%04b", q);
        clk = 1; serial_in = 0; #1; $display("%04b", q);

        clk = 0; serial_in = 0; #1; $display("%04b", q);
        clk = 1; serial_in = 0; #1; $display("%04b", q);

        clk = 0; serial_in = 1; #1; $display("%04b", q);
        clk = 1; serial_in = 1; #1; $display("%04b", q);

        clk = 0; serial_in = 1; #1; $display("%04b", q);
        clk = 1; serial_in = 1; #1; $display("%04b", q);

        $finish;
    end

endmodule

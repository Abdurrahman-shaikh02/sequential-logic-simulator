# Validation

Runs each circuit in `circuits/` through Yosys once, then compares the outputs
of our simulator and another simulator on that same netlist.

```
make
python3 validation/icarus/validate.py
```

## Writing circuits

**A bus output must come from a single vector, not from separate wires or regs.**

```verilog
output reg [3:0] q;                   // ok
assign q = count;                     // ok, count is one vector
assign q = {d, c, b, a};              // fails
assign q[0] = a; assign q[1] = b;     // fails the same way
```

If you need separate signals, use separate single-bit output ports.

When a bus is built from separate signals, Yosys writes the port as a list like
`{ \d \c \b \a }`, which the parser does not read. The simulator then stops with:

```
wire { \d \c \b \a } has no driver
```

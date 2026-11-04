# Validation

Runs each circuit in `circuits/` through Yosys once, then compares the outputs
of our simulator and another simulator on that same netlist.

```
make
python3 validation/icarus/validate.py
```

Our simulator is driven through the Worklet 3 interface, `simulator
--interactive`. The script answers its two prompts on stdin (the netlist path,
then an input-vectors file) and reads `output/interactive_results.txt`. It
never types inputs by hand. These files are kept in
`icarus/out/<circuit>/<flow>/`:

| File | Contents |
|---|---|
| `netlist.table`, `netlist.v` | The Yosys netlist, for us and for Icarus |
| `vectors` | Inputs for our simulator: one 0/1 column per input bit, in the order the simulator lists them |
| `tb.v` | The same inputs as a Verilog testbench for Icarus |
| `output/` | Everything `--interactive` writes: results table, waveform, JSON, VCD |
| `sim.out`, `icarus.out` | One line per row, each output bus written most significant bit first. These two are compared |

The port list comes from the simulator: with no vectors file and `q` at the
first step, `--interactive` lists the inputs and stops. The outputs are the
remaining columns of the results table.

These tests are separate from `tests/validation/`, which is part of the C++ test
suite (`./simulator --test validation`).

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

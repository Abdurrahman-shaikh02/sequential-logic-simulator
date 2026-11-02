# SLMS – user-created input files for every supported component

This is an additive Worklet 3 test pack. It does not modify the original project source files. The original project files are preserved; this folder contains separate example netlists and editable input files.

## 1. Combinational circuits: create your own `input.txt`

Covered gates: AND, OR, NOT, XOR, NAND, NOR, MUX, ANDNOT, ORNOT.

Each `combinational/<gate>/` folder contains:
- `netlist.table`: the circuit and its primary-input/output names.
- `input.txt`: example input vectors. You may edit it or create another text file yourself.
- `testbench.tb`: a separate reference test with expected-output assertions.

Input-file format:
- One row per input vector.
- Values are `0` or `1`, separated by spaces, commas, or tabs.
- A comment line may start with `#`.
- Values must follow the primary-input order printed by the simulator. The current CLI lists primary inputs alphabetically; follow that displayed order. The comments in each sample file show the intended order.

Run the file-input workflow from the project root:

```bash
./simulator
```

Choose option `3`, then enter the netlist path and your input-file path when prompted. The simulator runs one vector every 10 time units and writes:
- `output/interactive_results.txt`
- `output/interactive_waveform.txt`
- `output/interactive_waveform.json`
- `output/interactive_simulation.vcd`

For a direct automated reference check, run the corresponding `.tb` file, for example:

```bash
./simulator --testbench examples/component_file_io/combinational/xor/testbench.tb
```

The `.tb` assertion run and the `input.txt` vector run are distinct checks: the first checks expected outputs; the second demonstrates user-supplied file input and exports the interactive output table/waveforms.

## 2. Sequential circuits: the user-created `.tb` is the input file

Covered cells:
- Positive-edge and negative-edge DFF
- Positive-edge and negative-edge JKFF
- Positive-edge and negative-edge TFF
- 4-bit shift register

A sequential circuit needs timed input changes and a clock, so its input file is a clock-aware `.tb` file rather than a plain untimed vector list. Each `sequential/<cell>/testbench.tb` is editable and contains:
- `NETLIST`: the corresponding circuit netlist.
- `CLOCK`: clock name and period.
- `MAX_TIME`: simulation duration.
- `@time signal=value`: user stimulus events.
- `ASSERT`: expected output values at selected times.

Edit/copy the `.tb` file to supply your own input sequence. Keep the `NETLIST`, `CLOCK`, and `MAX_TIME` directives valid, and update assertions to match the behavior you expect. Then run it with:

```bash
./simulator --testbench examples/component_file_io/sequential/dff_positive/testbench.tb
```

Successful sequential verification requires a nonzero assertion count and zero failed assertions. A testbench that merely loads/runs with `Total Evaluated : 0` is not a correctness pass.

## 3. FSMs

Traffic Light and Vending Machine each have a netlist and editable clock-aware `.tb` stimulus file under `fsm/`. Run them with `./simulator --testbench <path-to-testbench.tb>`.

## 4. Preserve each run's outputs

The simulator writes to the common `output/` directory and overwrites the previous run's filenames. Copy/rename the four generated files after each run if you need a separate record for every component.

## 5. Important scope note

This pack adds example netlists and editable input/testbench files only. It does not change Worklet 1/2 implementation sources or the existing W3 simulator code. It demonstrates the existing W3 file-input interfaces; it does not claim that a plain `.txt` vector file is the correct interface for clocked sequential circuits. For sequential logic, the timed `.tb` file is the input file.

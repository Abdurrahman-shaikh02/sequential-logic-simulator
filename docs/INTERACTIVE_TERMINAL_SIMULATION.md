# Interactive Terminal Circuit Simulation

SLMS now supports entering circuit input values directly in the terminal. This feature uses the existing Worklet 1/2 parser and simulation engine together with Worklet 3's waveform exports.

## Build

From the `Mini Project SLMS` project directory:

```bash
make
```

## Launch

Run the dedicated interactive mode:

```bash
./simulator --interactive
```

Or run `./simulator` and select **3. Interactive Terminal Circuit Simulation**.

When prompted, enter the path to a supported circuit file:

- A synthesized netlist table, such as `tests/netlists/combinational/and/netlist.table`
- An FSM description in `.net` format, such as `examples/vending_machine/vending_machine.net`, if the project's FSM synthesizer supports that file

The simulator lists primary inputs it discovers from the loaded circuit. For each simulation step, enter `0` or `1` for each listed input. Press Enter at the step prompt to enter a vector; enter `q` at the step prompt to finish and run the collected vectors. Input steps are spaced 10 simulation time units apart. You can also type `q` while entering an input to finish the collected sequence.

## Results

The simulation report and an ASCII waveform preview are printed in the terminal. Full waveform files are written to:

- `output/interactive_waveform.txt`
- `output/interactive_waveform.json`
- `output/interactive_simulation.vcd`

The `.vcd` file can be opened in a compatible waveform viewer. The JSON waveform is available for GUI or other tooling integration.

## Notes

- Inputs are binary (`0` or `1`).
- The interactive mode accepts a supported synthesized table or supported FSM `.net` description, not arbitrary HDL source such as a `.v` file.
- For assertion-driven pass/fail checks with expected outputs, continue using a `.tb` testbench file with `./simulator --testbench <file.tb>`.

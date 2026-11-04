# SLMS Final Package — Build and Run Guide

This package combines the Worklet 1/Worklet 2 implementation snapshot with the Worklet 3 testbench, assertion, waveform, and GUI layers.

## Build (Ubuntu / Linux)

Requirements: `g++` with C++17 support, GNU `make`, and Python 3 for the GUI.

```bash
make clean
make -j2
```

## Run the simulator

```bash
./simulator
```

Useful direct commands:

```bash
./simulator --help
./simulator --testbench examples/and_gate/and_gate.tb
./simulator --testbench examples/traffic_light/traffic_light.tb
./simulator --testbench examples/vending_machine/vending_machine.tb
```

## Run the Worklet 3 web GUI

From the project root:

```bash
python3 gui/server.py 8080
```

Then open `http://localhost:8080` in a browser.

## Run tests

The main integrated Worklet suite is:

```bash
./tests/test_suite all
```

Run the included example testbenches:

```bash
make examples-test
```

The legacy parser regression suite is separately available with:

```bash
make parser-test
```

See `VERIFICATION_REPORT.md` for the exact results from the build verification, including the known parser-regression failures. Do not interpret a successful compile as proof that every test passed.

## Option 3: interactive input-vector file format

Input-only files remain supported. To enable output assertions, include a header and separate input/output values with `|`.

Example for the negative-edge D flip-flop:

```text
# columns: clk d | q
1 0 | 0
0 1 | 1
1 0 | 1
0 0 | 0
```

Column names must match the circuit's primary inputs and primary outputs, in the sorted order shown by the interactive prompt. Each row is one simulation step, ten time units apart. Output values are checked by the assertion engine.

If the file contains only inputs (no `# columns:` header), the simulation still runs, but its assertion report correctly says `NOT VERIFIED` because no expected outputs were supplied.

A complete example is provided at:
`examples/component_file_io/sequential/dff_negative/input.txt`

## Generated files

Simulation outputs are written under `output/` (VCD, JSON waveform, ASCII waveform, and result table). These are generated at runtime and are intentionally not bundled as stale build artifacts.

# SLMS: Sequential Logic Multi-worklet Simulator

A gate-level logic simulator for netlists produced by [Yosys](https://github.com/YosysHQ/yosys), with an FSM synthesizer, a testbench and assertion engine, waveform export, and a browser GUI.

```
 Verilog ──────────Yosys──────────▶ netlist.table ──┐
                                                    ├──▶ Parser ──▶ Simulator ──▶ Testbench ──▶ report, assertions
 FSM table (.net) ──Synthesizer──▶ netlist.table ──┘                                   │        results table
                                                                                        └──────▶ ASCII, JSON, VCD waveforms
      stimulus: .tb testbench file, input-vectors file, terminal, or the web GUI
```

The project was built in three worklets:

| Worklet | Part | Code |
|---|---|---|
| 1 | Netlist parser and combinational simulation | `parser/`, `core/`, `combinational/`, `primary/`, `operations/`, `simulation/` |
| 2 | Sequential elements and FSM synthesis | `sequential/`, `synthesizer/` |
| 3 | Testbench, assertions, waveforms, CLI and GUI | `testbench/`, `assertion/`, `waveform/`, `main.cpp`, `gui/` |

## Quick start

Needs `g++` with C++17, `make` and Python 3 (for the GUI). Yosys and Icarus Verilog are needed only for validation and benchmarks.

```sh
make                                                      # builds ./simulator
./simulator --test all                                    # 7 test categories, 30 tests
./simulator --testbench examples/traffic_light/traffic_light.tb
./simulator --gui 8080                                    # then open http://localhost:8080
```

`./simulator` with no arguments opens a menu. Every mode is described in [docs/cli.md](docs/cli.md).

## Results

- **Correctness against Icarus Verilog:** 26 of 26 circuits match on every row, in both Yosys flows, with 62,676 rows compared per flow. See [docs/validation.md](docs/validation.md).
- **Speed:** at 900 stimulus rows and a logic depth of 32, run time grows about linearly with circuit size. From 16k cells up it is faster than Icarus or about equal on most cases (up to 2.4 times faster at 256k cells), and it uses 1.2 to 1.7 times less memory. Icarus is faster on small circuits, shallow logic and long stimuli. See [docs/benchmarks.md](docs/benchmarks.md) for the conditions behind every number.
- **Tests:** the 7-category suite passes 30 of 30, and the example testbenches pass every assertion. See [docs/testing.md](docs/testing.md).

## Documentation

| | |
|---|---|
| [Getting started](docs/getting-started.md) | Build, first simulation, from Verilog to a waveform |
| [Command line](docs/cli.md) | The menu and every `--` option |
| [Web GUI](docs/gui.md) | Every panel, and the HTTP API behind it |
| [File formats](docs/formats/) | Netlist, testbench, input vectors, FSM, output files |
| [Architecture](docs/architecture/overview.md) | How the simulator works inside |
| [Parser](docs/parser.md) | How a netlist becomes a graph |
| [Synthesizer](docs/synthesizer/synthesizer.md) | FSM table to netlist, and FSM analysis |
| [Testing](docs/testing.md) | Test suites and how to run them |
| [Validation](docs/validation.md) | Comparison against Icarus Verilog |
| [Benchmarks](docs/benchmarks.md) | Run time and memory against Icarus Verilog |
| [Limitations](docs/limitations.md) | Known issues and unsupported features |

The full index is [docs/README.md](docs/README.md).

## Repository layout

```
main.cpp          command line: menu, --testbench, --interactive, --fsm, --truthtable, --test, --gui
include/, src/    the library, one folder per component:
  core/             Node, Port, Signal, List (the circuit graph)
  combinational/    AND, OR, NAND, NOR, XOR, NOT, ANDNOT, ORNOT, MUX
  sequential/       D, T and JK flip-flops, positive and negative edge
  primary/          primary input and output nodes
  operations/       the work items the simulator queues
  simulation/       Simulator and its queues
  parser/           netlist reader
  synthesizer/      FSM synthesizer, dead and unreachable state analysis
  testbench/        Testbench (stimulus, clock, run loop) and the 7-category TestSuite
  assertion/        AssertionEngine (assertions and invariants)
  waveform/         WaveformGenerator (table, ASCII, JSON, VCD)
gui/              browser front end and its Python HTTP server
tools/            run_file_testbench.py: run a .tb and copy the outputs to a folder
examples/         example testbenches, netlists and input files
tests/            the 7 test categories, parser regression, standalone tests, netlists
validation/       comparison against Icarus Verilog
benchmark/        run time and memory against Icarus Verilog
docs/             documentation (docs/archive/ holds the old notes and reports)
sandbox/          scratch circuits, not part of the build
```

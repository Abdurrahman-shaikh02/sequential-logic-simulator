# hdl_sim

A gate-level logic simulator for netlists produced by [Yosys](https://github.com/YosysHQ/yosys), plus a synthesizer that turns a finite state machine table into such a netlist.

- **Simulator.** Reads a Yosys `write_table` netlist of basic gates and flip-flops and drives it from a stimulus file, one row per time step. The model is zero-delay and event-driven, and gates are evaluated in topological order. It writes the outputs after each step, or a truth table for a combinational circuit.
- **Synthesizer.** Converts an FSM transition table into a netlist of AND, OR and NOT gates and D flip-flops that the simulator runs directly. It can also report dead and unreachable states.
- **Validation and benchmarks.** Scripts that compare the simulator's output with Icarus Verilog on the same netlist, and measure run time and memory against it.

```
 Verilog ──Yosys──▶ netlist.table ─┐
                                   ├──▶ simulator sim ──▶ output trace
 FSM table ──simulator synth──▶ ───┘         ▲
                                       stimulus file
```

## Quick start

Needs a C++17 compiler and `make`. Yosys and Icarus Verilog are needed only for validation and benchmarks.

```sh
make                                     # builds ./simulator
make test                                # runs the test suite

./simulator ports tests/fixtures/netlists/dffp/netlist.table
# // inputs: clk d
# // outputs: q

printf '// inputs: clk d\n0 1\n1 1\n0 0\n1 0\n' > dff.stim
./simulator sim tests/fixtures/netlists/dffp/netlist.table -s dff.stim
# // outputs: q
# 0
# 1
# 1
# 0
```

See [docs/getting-started.md](docs/getting-started.md) for the whole flow, from Verilog to a simulated trace.

## Results

- **Correctness:** our simulator matches Icarus Verilog on every row across 26 circuits, both Yosys flows and 3 random seeds, with 62,676 rows compared per flow. See [docs/validation.md](docs/validation.md).
- **Speed:** on the benchmark circuits, at 900 stimulus rows and a logic depth of 32, run time grows almost linearly with circuit size: the slope on a log-log plot is 1.0 to 1.3. At these settings its run time is 1.0 to 2.5 times faster than Icarus, and it uses 1.3 to 1.7 times less memory. Icarus is faster on shallow logic and on long stimuli. See [docs/benchmarks.md](docs/benchmarks.md) for the conditions behind every number.

> **TODO (synthesizer results):** add a summary of the synthesizer validation and benchmarks once they are done.

## Documentation

| | |
|---|---|
| [Getting started](docs/getting-started.md) | Build, run, first simulation |
| [Command line](docs/cli.md) | Every command and option |
| [File formats](docs/formats/) | Netlist, stimulus, output, FSM |
| [Architecture](docs/architecture/overview.md) | How the simulator works inside |
| [Parser](docs/parser.md) | How a netlist becomes a graph |
| [Synthesizer](docs/synthesizer/synthesizer.md) | FSM table to netlist |
| [Testing](docs/testing.md) | Test suite and how to add tests |
| [Validation](docs/validation.md) | Comparison against Icarus Verilog |
| [Benchmarks](docs/benchmarks.md) | Performance measurements |
| [Limitations](docs/limitations.md) | Known issues and unsupported features |

The full index is in [docs/README.md](docs/README.md).

## Repository layout

```
main.cpp          command line front end (sim, synth, truth, ports)
include/, src/    the library, one folder per component:
  core/             Node, Port, Signal, List (the circuit graph)
  combinational/    AND, OR, NAND, NOR, XOR, NOT, ANDNOT, ORNOT, MUX
  sequential/       D, T and JK flip-flops, positive and negative edge
  primary/          primary input and output nodes
  operations/       the work items the simulator queues
  simulation/       Simulator and its queues
  parser/           netlist reader
  synthesizer/      FSM synthesizer, dead and unreachable state analysis
tests/            test framework, unit and integration tests, fixtures
validation/       comparison against Icarus Verilog
benchmark/        performance measurements against Icarus Verilog
docs/             documentation (docs/archive/ holds old design notes)
sandbox/          scratch circuits, not part of the build
```

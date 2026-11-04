# Documentation

Read from top to bottom the first time. Each page also stands on its own.

## Using the simulator

1. [Getting started](getting-started.md): build, run the tests, the first simulation, and the path from Verilog to a waveform.
2. [Command line](cli.md): the menu and every `--` option, with exit codes.
3. [Web GUI](gui.md): the panels in the browser, and the HTTP API the server provides.
4. File formats:
   - [Netlist](formats/netlist.md): the `.table` file the parser reads, and the cells it supports.
   - [Testbench](formats/testbench.md): the `.tb` language, with clocks, stimuli, assertions and invariants.
   - [Input vectors](formats/vectors.md): the file `--interactive` reads, one row per step, with optional expected outputs.
   - [FSM](formats/fsm.md): the state table the synthesizer reads.
   - [Output files](formats/output.md): the report, the results table, and the ASCII, JSON and VCD waveforms.

## How it works

5. [Architecture overview](architecture/overview.md): the components and how data flows between them.
6. [Nodes and the circuit graph](architecture/nodes.md): gates, flip-flops, primary nodes and their connections.
7. [Simulation algorithm](architecture/simulation.md): the queues, topological commits and clock edges.
8. [Testbench engine](architecture/testbench.md): how a testbench run steps through time, checks assertions and records waveforms.
9. [Design decisions](architecture/decisions.md): why it works this way, and what each choice costs.
10. [Parser](parser.md): how a netlist becomes a graph, with validation and cycle detection.
11. Synthesizer:
    - [Synthesizer](synthesizer/synthesizer.md): usage, output and errors.
    - [Algorithm and correctness](synthesizer/algorithm.md): how the FSM becomes gates, and why the circuit behaves like the table.
    - [FSM analysis](synthesizer/fsm-analysis.md): finding dead and unreachable states.

## Quality

12. [Testing](testing.md): the 7-category suite, the parser regression suite, standalone tests and example checks.
13. [Validation](validation.md): comparison against Icarus Verilog.
14. [Benchmarks](benchmarks.md): run time and memory against Icarus Verilog.
15. [Limitations](limitations.md): known issues and unsupported features.

## Other files in this folder

| File | What it is |
|---|---|
| `fsm.net` | Template for the FSM format. The synthesizer headers point to it |
| `fsm_example/` | Example FSMs used by these pages and by the tests: the traffic light, and an FSM with dead and unreachable states |
| `out_format.txt` | The original sketch of the results table. `WaveformGenerator` points to it |
| `synthesis_algorithm.txt` | The synthesis algorithm in pseudocode. `Synthesizer.cpp` points to it; [synthesizer/algorithm.md](synthesizer/algorithm.md) explains it |
| `archive/` | Old notes and change reports, kept as they were written. See below |

## The archive

`archive/` holds the notes and reports written while the project was being built: design sketches (`class.txt`, `algorithm.txt`, `optimized_evaluation.txt`), per-change reports (`AUDIT_FIXES.md`, `IMPLEMENTATION_REPORT.md`, `FINAL_REPAIR_NOTES.md`, `VERIFICATION_REPORT.md`) and the first guides for each feature. Everything still true in them has been moved into the pages above.

Many of them describe an earlier state of the code and are now wrong. For example, some say the negative-edge, JK and T flip-flops are empty placeholders (they are implemented and tested), quote older test counts, or say that `CLOCK clk 10 3` gives 3 units HIGH (it gives 3 LOW, then 7 HIGH; see [formats/testbench.md](formats/testbench.md#clock)). Where the archive and these pages disagree, these pages are right.

## Unfinished

Sections still waiting for the synthesizer validation and benchmarks are marked `TODO (`:

```sh
grep -rn "TODO (" README.md docs/ --exclude-dir=archive
```

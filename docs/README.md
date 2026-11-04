# Documentation

Read from top to bottom the first time. Each page also stands on its own.

## Using the simulator

1. [Getting started](getting-started.md): build, run the tests, and go from a Verilog file to a simulated trace.
2. [Command line](cli.md): the `sim`, `synth`, `truth` and `ports` commands, their options and exit codes.
3. File formats:
   - [Netlist](formats/netlist.md): the `.table` file the simulator reads, and the cells it supports.
   - [Stimulus](formats/stimulus.md): the input file for `sim`.
   - [Output](formats/output.md): the trace, the truth table and the `--perf` statistics.
   - [FSM](formats/fsm.md): the state table the synthesizer reads.

## How it works

4. [Architecture overview](architecture/overview.md): the components and how data flows between them.
5. [Nodes and the circuit graph](architecture/nodes.md): gates, flip-flops, primary nodes and their connections.
6. [Simulation algorithm](architecture/simulation.md): the queues, topological commits and clock edges.
7. [Design decisions](architecture/decisions.md): why it works this way, and what each choice costs.
8. [Parser](parser.md): how a netlist becomes a graph, with validation and cycle detection.
9. Synthesizer:
   - [Synthesizer](synthesizer/synthesizer.md): usage, output and errors.
   - [Algorithm and correctness](synthesizer/algorithm.md): how the FSM becomes gates, and why the circuit behaves like the table.
   - [FSM analysis](synthesizer/fsm-analysis.md): finding dead and unreachable states.

## Quality

10. [Testing](testing.md): the test framework, how to run tests and how to add them.
11. [Validation](validation.md): comparison against Icarus Verilog.
12. [Benchmarks](benchmarks.md): run time and memory against Icarus Verilog.
13. [Limitations](limitations.md): known issues and unsupported features.

## Other

- `fsm.net` and `fsm_example/`: an FSM template and example FSMs used by the docs and tests.
- `archive/`: old design notes, kept as they were written. Some describe behavior that has since changed (for example `algorithm.txt` still has the sequential queue reordering), so trust the pages above where they disagree.

## Unfinished

Search for `TODO (` to find every section still waiting for the synthesizer validation and benchmarks:

```sh
grep -rn "TODO (" README.md docs/
```

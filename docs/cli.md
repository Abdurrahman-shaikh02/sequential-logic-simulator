# Command line

```
./simulator                              Launch interactive menu
./simulator --testbench <file.tb>        Run simulation from testbench file
./simulator --interactive                Interactively enter circuit inputs
./simulator --fsm <file.net>             Synthesize FSM and analyze states
./simulator --truthtable <netlist> <out> Generate combinational truth table
./simulator --test [category]            Run software tests (all, unit, etc.)
./simulator --gui [port]                 Launch web GUI server
./simulator --help                       Display this help menu
```

Only the first argument is read as an option. Run every command from the project root: paths inside `.tb` files, the examples and the GUI are relative to it. The code is in `main.cpp`.

## `--testbench <file.tb>`

Loads and runs a testbench file (format: [formats/testbench.md](formats/testbench.md)).

1. Loads the circuit named by `CIRCUIT`, `NETLIST` or `FSM`. An FSM table is synthesized first.
2. Runs the simulation to `MAX_TIME`, checking every `ASSERT` at its time and every `INVARIANT` at every recorded time.
3. Prints the ASCII waveform and the assertion report.
4. Writes `output/results.txt`, `output/waveform.txt`, `output/waveform.json` and `output/simulation.vcd` ([formats/output.md](formats/output.md)).

| Exit code | Meaning |
|---|---|
| 0 | Loaded, ran, and every assertion and invariant passed. A testbench with no assertions also exits 0 |
| 1 | The file or circuit could not be loaded, a signal name or bit index was invalid, or an assertion or invariant failed |

## `--interactive`

Asks two questions on standard input:

1. **Circuit path:** a `.table` netlist, or a `.net` file. The file's contents decide what a `.net` file is: an FSM table is synthesized, and a six-column netlist is parsed directly.
2. **Input vectors file:** the path to a vectors file ([formats/vectors.md](formats/vectors.md)), or Enter to type the values.

Between the two questions it lists the primary inputs, one per bit, sorted by name and then by bit. That is the column order a vectors file must use.

When typing by hand, press Enter at each `Step N` prompt and then give `0` or `1` for each input. Type `q` at a step prompt to finish and run. Steps are 10 time units apart.

After the run it prints the report and the first 30 lines of the ASCII waveform, and writes `output/interactive_results.txt`, `interactive_waveform.txt`, `interactive_waveform.json` and `interactive_simulation.vcd`.

| Exit code | Meaning |
|---|---|
| 0 | The run finished and no expected-output check failed |
| 1 | No path, the circuit could not be loaded, a vectors-file error, no rows, or an expected output did not match |

Scripts can answer both prompts from a pipe, which is how `validation/` and `benchmark/` drive it:

```sh
printf 'circuit.table\nvectors.txt\n' | ./simulator --interactive
```

With no vectors file and `q` at the first step, it lists the inputs and exits with code 1 without simulating.

## `--fsm <file.net>`

Analyzes an FSM table and synthesizes it:

1. Prints its dead states ([synthesizer/fsm-analysis.md](synthesizer/fsm-analysis.md)).
2. Prints its reachable and unreachable states.
3. Synthesizes it and writes the netlist next to the input, with the extension changed to `.table` ([synthesizer/synthesizer.md](synthesizer/synthesizer.md)).

Always exits 0. Look for `[-] FSM Synthesis failed.` and the error lines before it.

## `--truthtable <netlist> <out>`

Writes the truth table of a combinational netlist to `<out>` and prints its first lines. The header lists the input bits, `|`, then the output bits, and each row follows in the same order.

It fails, with a message, for circuits that contain a flip-flop, have no inputs, or have more than 20 input bits. Always exits 0.

## `--test [category]`

Runs the C++ test suite ([testing.md](testing.md)). `category` is one of `unit`, `integration`, `validation`, `blackbox`, `whitebox`, `regression`, `system`, or `all` (the default; any other word also runs all). Exits 1 if any test failed.

## `--gui [port]`

Runs `python3 gui/server.py <port>` (default 8080) and waits for it. Open `http://localhost:<port>`. Stop it with Ctrl+C. See [gui.md](gui.md). The same server can be started with `python3 gui/server.py 8080` or `make gui`.

## The menu

`./simulator` with no arguments shows:

```
  1. Run Traffic Light FSM Testbench (Full Demonstration)
  2. Run Custom Testbench File (.tb)
  3. Interactive Terminal Circuit Simulation (enter inputs manually)
  4. Synthesize FSM and Analyze Dead/Unreachable States
  5. Generate Combinational Truth Table
  6. Run Complete Software Testing Suite (All 7 Test Types)
  7. Launch Graphical User Interface (Web GUI)
  8. Exit
```

Option 1 runs `examples/traffic_light/traffic_light.tb`. The others ask for their paths and do the same as the option of the same name above. Paths typed at the menu cannot contain spaces.

## `tools/run_file_testbench.py`

Runs a `.tb` file against a netlist of your choice and copies the four output files to a folder:

```sh
python3 tools/run_file_testbench.py --netlist tests/netlists/combinational/and/netlist.table \
    --testbench examples/and_gate/and_gate.tb --out-dir results/and
```

It replaces the testbench's `NETLIST` or `FSM` line with the given netlist (or adds one), runs `./simulator --testbench`, and copies `simulation.vcd`, `waveform.txt`, `waveform.json` and `results.txt` to `--out-dir` (default `file_run_output/`). It exits 1 if the simulator fails.

## Output files are overwritten

Every run writes to the same names in `output/`. Copy them elsewhere to keep a run's results. The GUI server runs one simulation at a time for the same reason.

# Benchmarks

How run time and memory grow with circuit size, activity, logic depth and stimulus length, measured against Icarus Verilog. How to run the scripts is in `benchmark/README.md`. This page covers what was measured and what it shows.

## Simulator

### Setup

| | |
|---|---|
| Data | `benchmark/simulation/hdl_sim/results.csv`, `benchmark/simulation/icarus/results.csv` (run 2026-11-04 on branch `bv3`, simulator code from commit `1d9689d`) |
| Plots | `benchmark/simulation/plots/*.png` |
| Machine | 12th Gen Intel Core i5-12450HX, every run pinned to CPU core 0 |
| Builds | Our simulator from `make` (`-O2`), Icarus Verilog 12.0, Yosys 0.68+112 |
| Interface | `./simulator --interactive`, given the netlist path and an input-vectors file on stdin (see below) |
| What is timed | Wall time of the whole process, including reading, parsing and writing output files, as the median of 3 runs (2 from 100,000 cells up) |
| Memory | Peak RSS of the run. For Icarus this is `vvp`, without the `iverilog` compile step |

### How our simulator is run

The Worklet 3 interface has two ways to give a simulation its inputs: a `.tb` testbench file (`--testbench`) and an input-vectors file (`--interactive`). A `.tb` file cannot set one bit of an input bus, and every circuit here has a 64-bit or wider input bus, so the benchmark uses `--interactive`.

For each case, `hdl_sim/run.py` converts the stimulus into a vectors file with one 0/1 column per input bit. That conversion is not timed. It then runs `./simulator --interactive` and answers its two prompts from a file. Rows are applied 10 time units apart, one simulation step per row.

Every run does everything the interface does, and none of it can be turned off:

- keeps a snapshot of every primary input and output bit at every step, in memory;
- writes `output/interactive_results.txt`, `interactive_waveform.txt`, `interactive_waveform.json` and `interactive_simulation.vcd`;
- prints a report and an ASCII waveform to stdout, which the benchmark discards.

So `run_s` and `rss_kb` include this work. The output hash is taken from the results table.

### Circuits

| Family | What | Sizes |
|---|---|---|
| `random` | Layers of random gates using every gate type, written directly as a netlist | 1k, 4k, 16k, 64k, 256k cells |
| `mixed` | The same, with every 9th layer made of flip-flops (about 9%) | 1k to 256k cells |
| `multiplier` | Registered array multipliers from `circuits/multiplier.v`, through Yosys `abc.ys` | 8, 16, 32, 64 bits (375 to 25,273 cells) |

Unless stated otherwise, depth is 32 layers and the stimulus is 900 rows. Each input bit flips with probability 0.5 (high activity) or 0.02 (low activity) per row. Fixed seeds make every generated file the same on each run.

### Correctness

Both simulators produced identical outputs on all 32 cases, compared by a SHA-256 hash of each output. `plot.py` exits with an error if any case differs.

### Results at default settings (depth 32, 900 rows, high activity)

| Circuit | Cells | Ours (s) | Icarus run (s) | Icarus compile (s) | Run only | Memory, ours vs Icarus |
|---|---|---|---|---|---|---|
| random | 16,000 | 1.25 | 2.35 | 0.26 | ours 1.9× faster | 19 MB vs 22 MB |
| random | 256,000 | 38.3 | 91.1 | 6.1 | ours 2.4× faster | 191 MB vs 247 MB |
| mixed | 16,000 | 0.60 | 0.47 | 0.78 | Icarus 1.3× faster | 19 MB vs 27 MB |
| mixed | 256,000 | 15.9 | 16.6 | 367.7 | about equal | 191 MB vs 314 MB |
| multiplier | 25,273 (64-bit) | 1.32 | 2.41 | 0.76 | ours 1.8× faster | 31 MB vs 36 MB |

Across every case at depth 32 and 900 rows that takes at least 0.1 s, at either activity:

- **Run time:** from 3.0× slower than `vvp` (`mixed`, 1k cells, low activity: 0.113 s vs 0.037 s) to 2.4× faster (`random`, 256k cells). Icarus is faster on most circuits below 16k cells. From 16k cells up ours is faster or about equal, except `mixed` at 16k and the 64-bit multiplier at low activity (0.624 s vs 0.555 s). Including Icarus's compile step widens the gap on `mixed`, where `iverilog` takes 368 s at 256k cells.
- **Memory:** below 16k cells ours uses up to 1.8 times more than Icarus, because of the per-row cost below. From 16k cells up it uses 1.2 to 1.7 times less.

### Cost of the interface

The interface's own work grows with the number of primary input and output bits times the number of rows, whatever the circuit size. It builds a string-keyed snapshot of every such bit at every step, keeps all of them, writes the four output files from them and prints the waveform. On small circuits this, not simulating the gates, sets the run time:

| Circuit (900 rows, low activity) | Cells | Input and output bits | Ours | Icarus run |
|---|---|---|---|---|
| `multiplier`, 8-bit | 375 | 33 | 0.026 s, 7.6 MB | 0.019 s, 8.4 MB |
| `random`, 1k | 2,048 | 128 | 0.119 s, 17.1 MB | 0.057 s, 9.4 MB |

So every `random` and `mixed` run here uses at least 17 MB.

### Scaling

`plots/slopes.csv` gives the slope of log(time) against log(cells). A slope of 1 means linear, and 2 means quadratic.

| | random | mixed | multiplier |
|---|---|---|---|
| Ours | 0.98 to 1.09 | 0.89 to 0.99 | 0.75 to 0.88 |
| Icarus `vvp` | 1.27 to 1.28 | 1.16 to 1.19 | 0.81 to 1.08 |
| Icarus `iverilog` (compile) | 1.04 to 1.06 | 1.87 to 1.89 | 0.80 to 0.86 |

Our slopes are below 1 partly because of the interface's cost per row, which is the same at every circuit size here: it makes the small circuits slower than the gates alone would, which flattens the line. Between 64k and 256k cells, where that cost no longer matters, our time grows 6.1 to 6.4 times on `random` and 5.0 to 5.4 times on `mixed` for 4 times the cells.

### Where Icarus is faster

The lead depends on the conditions, so quote them with any comparison:

| Sweep (16k cells, high activity) | Ours (s) | Icarus run (s) | Memory, ours vs Icarus | Result |
|---|---|---|---|---|
| `random`, depth **8** | 1.66 | 1.35 | 19 MB vs 24 MB | Icarus 1.2× faster |
| `random`, depth 32 | 1.25 | 2.35 | 19 MB vs 22 MB | Ours 1.9× faster |
| `random`, depth **128** | 0.95 | 5.61 | 19 MB vs 22 MB | Ours 5.9× faster |
| `mixed`, 900 rows | 0.60 | 0.47 | 19 MB vs 27 MB | Icarus 1.3× faster |
| `mixed`, **9,000** rows | 6.27 | 3.37 | 131 MB vs 32 MB | Icarus 1.9× faster |
| `mixed`, **90,000** rows | 77.4 | 32.2 | 1,280 MB vs 84 MB | Icarus 2.4× faster |

- **Depth.** Icarus's cost rises with logic depth, while ours does not. Shallow, wide logic favors Icarus, and deep logic favors us strongly.
- **Stimulus length.** Our run time grows linearly with rows, but each row costs more than in Icarus, so Icarus gains as the stimulus grows. Memory is the bigger problem: the testbench keeps a snapshot of every input and output bit at every step until the run ends, so memory grows with rows × signals. At 90,000 rows on a circuit with 129 input and output bits, that is 1.28 GB. Icarus grows from 27 MB to 84 MB over the same range.

The plots `depth.png` and `stimulus_length.png` show these sweeps.

### Reproducing

```sh
make
python3 benchmark/simulation/generate.py
python3 benchmark/simulation/hdl_sim/run.py
python3 benchmark/simulation/icarus/run.py
python3 benchmark/simulation/plot.py
```

Run nothing else heavy at the same time, and do not compare timings from different sessions. `benchmark/README.md` has the details.

## Synthesizer

> **TODO (synthesizer benchmarks):** to be written by the synthesizer benchmarking work. Suggested contents:
>
> - **Setup:** machine, commit, what is timed (for example `./simulator --fsm <file.net>`, which analyzes and synthesizes an FSM), and how many runs.
> - **Inputs:** FSM sizes swept (states, inputs, outputs, rows), and how they were generated.
> - **Results:** synthesis time and memory against FSM size, and the size of the netlist produced (gates, flip-flops, depth). Compare with Yosys on the same FSM if possible.
> - **Scaling:** the slope of time against size, in the same form as the simulator's.
> - **Where the time goes**, if profiled.
> - **Reproducing:** the commands, and where the scripts and results live (for example `benchmark/synthesizer/`).

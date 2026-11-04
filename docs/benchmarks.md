# Benchmarks

How run time and memory grow with circuit size, activity, logic depth and stimulus length, measured against Icarus Verilog. How to run the scripts is in `benchmark/README.md`. This page covers what was measured and what it shows.

## Simulator

### Setup

| | |
|---|---|
| Data | `benchmark/simulation/hdl_sim/results.csv`, `benchmark/simulation/icarus/results.csv` (committed in `1978bde`, 2026-11-01) |
| Plots | `benchmark/simulation/plots/*.png` |
| Machine | 12th Gen Intel Core i5-12450HX, every run pinned to CPU core 0 |
| Builds | Our simulator from `make` (`-O2`), Icarus Verilog 12.0 |
| What is timed | Wall time of the whole process, including reading and parsing, as the median of 3 runs (2 from 100,000 cells up) |
| Memory | Peak RSS of the run. For Icarus this is `vvp`, without the `iverilog` compile step |

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

| Circuit | Cells | Ours (s) | Icarus run (s) | Icarus compile (s) | Speed-up, run only | Memory, ours vs Icarus |
|---|---|---|---|---|---|---|
| random | 16,000 | 1.11 | 2.19 | 0.26 | 2.0× | 16 MB vs 23 MB |
| random | 256,000 | 36.8 | 88.2 | 6.2 | 2.4× | 196 MB vs 253 MB |
| mixed | 16,000 | 0.46 | 0.47 | 0.77 | 1.0× | 16 MB vs 28 MB |
| mixed | 256,000 | 16.1 | 16.7 | 365.7 | 1.0× | 196 MB vs 322 MB |
| multiplier | 25,273 (64-bit) | 0.98 | 2.42 | 0.77 | 2.5× | 26 MB vs 37 MB |

Across every case at depth 32 and 900 rows that takes at least 0.1 s, at either activity:

- **Run time:** ours is 1.0 to 2.5 times faster than `vvp`. Including Icarus's compile step makes the gap much wider on `mixed`, where `iverilog` takes 366 s at 256k cells.
- **Memory:** ours uses 1.3 to 1.7 times less.

### Scaling

`plots/slopes.csv` gives the slope of log(time) against log(cells). A slope of 1 means linear, and 2 means quadratic.

| | random | mixed | multiplier |
|---|---|---|---|
| Ours | 1.21 to 1.26 | 1.21 to 1.22 | 0.96 to 1.03 |
| Icarus `vvp` | 1.27 | 1.14 to 1.19 | 0.80 to 1.08 |
| Icarus `iverilog` (compile) | 1.06 | 1.87 to 1.88 | 0.80 to 0.85 |

Our run time grows slightly faster than linearly on the large generated circuits.

### Where Icarus is faster

The lead depends on the conditions, so quote them with any comparison:

| Sweep (16k cells, high activity) | Ours (s) | Icarus run (s) | Result |
|---|---|---|---|
| `random`, depth **8** | 1.48 | 1.33 | Icarus about 10% faster |
| `random`, depth 32 | 1.11 | 2.19 | Ours 2.0× faster |
| `random`, depth **128** | 0.84 | 5.65 | Ours 6.8× faster |
| `mixed`, **9,000** rows | 3.88 | 3.35 | Icarus about 15% faster |
| `mixed`, **90,000** rows | 38.2 | 32.2 | Icarus about 15% faster |

- **Depth.** Icarus's cost rises with logic depth, while ours does not. Shallow, wide logic favors Icarus, and deep logic favors us strongly.
- **Stimulus length.** Our cost per row is higher on `mixed`, so Icarus gains as the stimulus grows. At 900 rows our lower start-up cost hides this. Our memory also grows with stimulus length, from 16 MB at 900 rows to 23 MB at 90,000, because the whole stimulus and trace are kept in memory. Icarus's memory grows more, from 28 MB to 86 MB.

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
> - **Setup:** machine, commit, what is timed (`simulator synth -p` reports `synth_ms`), and how many runs.
> - **Inputs:** FSM sizes swept (states, inputs, outputs, rows), and how they were generated.
> - **Results:** synthesis time and memory against FSM size, and the size of the netlist produced (gates, flip-flops, depth). Compare with Yosys on the same FSM if possible.
> - **Scaling:** the slope of time against size, in the same form as the simulator's.
> - **Where the time goes**, if profiled.
> - **Reproducing:** the commands, and where the scripts and results live (for example `benchmark/synthesizer/`).

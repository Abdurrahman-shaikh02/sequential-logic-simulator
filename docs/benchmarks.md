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

How run time, memory and the size of the netlist grow with the FSM. How to run the scripts is in `benchmark/README.md`. Correctness is checked separately (see [validation.md](validation.md#synthesizer)).

### Setup

| | |
|---|---|
| Data | `benchmark/synthesizer/hdl_synth/results.csv` (run on 2026-11-04 on commit `ffa0a5f`; the benchmark scripts were not yet committed) |
| Machine | Intel Core Ultra 5 125H with 14 GB of memory, every run pinned to CPU core 0. This is not the machine used for the simulator results above, so do not compare the two |
| Build | `make` (`-O2`) |
| What is timed | Wall time of the whole process, including reading the FSM and writing the netlist to `/dev/null`, as the median of 3 runs (2 from 100,000 rows up) |
| Memory | Peak RSS of the run, with a limit of 6 GB. No case reached it |
| Netlist size | Gates, flip-flops and depth (the longest path in gates) from an extra untimed run. A second untimed run confirms the file is identical |

Times under about 0.1 s are noisy. Yosys is not compared yet.

### Inputs

Rows are tied to the other sizes, because a full table has `rows = states × 2^inputs`, so each family changes one thing:

| Family | Changes | Held still |
|---|---|---|
| `states` | 16 to 65,536 states | 2 inputs, 4 outputs, full table |
| `inputs_full` | 2 to 14 inputs, so rows grow too | 16 states, 4 outputs, full table |
| `inputs_sparse` | 8 to 256 inputs | 4,096 rows, 256 states, 4 outputs |
| `outputs` | 1 to 1,024 outputs | 256 states, 4 inputs, full table |
| `output_ones` | Fraction of output bits that are 1 | 256 states, 4 inputs, 16 outputs |
| `fill` | Fraction of rows present | 256 states, 6 inputs, 4 outputs |
| `structure` | Random, ring, counter or binary tree | 1,024 states, 2 inputs, 4 outputs |
| `row_order` | Sorted or shuffled rows | 256 states, 4 inputs, 4 outputs |
| `unreachable` | Fraction of states that cannot be entered, with their rows kept or removed | 1,024 states, 2 inputs, 4 outputs |

### Results by size

| FSM | Rows | Time (s) | Memory (MB) | Gates | Flip-flops | Depth |
|---|---|---|---|---|---|---|
| 256 states, 2 inputs | 1,024 | 0.016 | 11 | 7,930 | 8 | 525 |
| 4,096 states, 2 inputs | 16,384 | 0.53 | 160 | 160,831 | 12 | 8,330 |
| 16,384 states, 2 inputs | 65,536 | 3.42 | 750 | 710,241 | 14 | 32,970 |
| 65,536 states, 2 inputs | 262,144 | 19.8 | 3,535 | 3,103,349 | 16 | 131,504 |
| 16 states, 14 inputs | 262,144 | 7.3 | 1,644 | 1,571,225 | 4 | 131,472 |

### Scaling

Slope of log(value) against log(size), where 1 means linear and 2 means quadratic. Time slopes use only runs of at least 0.1 s.

| Family, against | Time | Memory | Gates | Depth |
|---|---|---|---|---|
| `states`, rows | 1.31 (3 points) | 0.84 | 1.08 | 0.97 |
| `inputs_full`, rows | 1.09 (3 points) | 0.74 | 1.00 | 0.97 |
| `inputs_sparse`, inputs | 1.04 | 0.85 | 0.91 | 0.03 |
| `outputs`, outputs | 0.84 (5 points) | 0.65 | 0.65 | 0.01 |

The memory slopes below 1 include small runs, where the program's fixed start-up memory dominates. Between the two largest `states` cases, 4 times the rows took 5.8 times the time and 4.7 times the memory.

### What the results show

- **Memory is the limit.** The largest `states` case uses about 13.8 KB per row, about 8 times the size of the netlist it writes. An early trial with 1,048,576 rows reached 11 GB of 14 GB before the system killed it.
- **Rows cost more as states grow.** The same 262,144 rows take 19.8 s with 65,536 states and 7.3 s with 16 states. With more state bits, each row feeds more next-state lines, giving more OR gates.
- **Depth grows linearly with rows.** It is about rows ÷ 2 when half the bits are 1, because the OR gates are chained. It does not depend on the number of inputs or outputs.
- **Each extra input or output has a steady cost.** Time is close to linear in the number of inputs at fixed rows. With 4,096 rows, 1,024 outputs take 10.1 s.
- **More 1s cost more.** From 0 to 75% ones, time goes from 0.065 s to 0.248 s and gates from 24,443 to 73,479. At 100% gates fall to 28,492 because all the output columns are identical and share one chain, but time stays at 0.235 s. Depth rises to 4,107, because that chain includes every row.
- **Regular FSMs get no benefit.** A counter (36,868 gates) costs about the same as a random table (36,119), because nothing is minimized.
- **Row order hardly matters:** 32,777 gates sorted and 32,584 shuffled.
- **Unreachable states are expensive.** When 75% of the states are unreachable, removing them reduces gates from 35,196 to 8,090 and flip-flops from 10 to 8. The synthesizer does not remove them.

### Reproducing

```sh
make
python3 benchmark/synthesizer/generate.py
python3 benchmark/synthesizer/hdl_synth/run.py
```

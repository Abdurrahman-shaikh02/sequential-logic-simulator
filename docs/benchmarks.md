# Benchmarks

How run time and memory grow with circuit size, activity, logic depth and stimulus length, measured against Icarus Verilog. How to run the scripts is in `benchmark/README.md`. This page covers what was measured and what it shows.

## Simulator

### Setup

| | |
|---|---|
| Data | `benchmark/simulation/hdl_sim/results.csv`, `benchmark/simulation/icarus/results.csv` (run 2026-11-04 on branch `bv3`, simulator code from commit `1d9689d`; the simulator code on `main` at `684fb4a` is the same apart from removing an unused enum value) |
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

How run time, memory and the size of the netlist grow with the FSM. How to run the scripts is in `benchmark/README.md`. Correctness is checked separately (see [validation.md](validation.md#synthesizer)).

The tables and findings below describe the synthesizer **before** the memory fix, kept for comparison. [Effect of the memory fix](#effect-of-the-memory-fix) gives the current numbers.

### Setup

| | |
|---|---|
| Data | `benchmark/synthesizer/results/before_memory_fix_bv3/results.csv` (run on 2026-11-04 on `bv3`, commit `01c3349`, with `Synthesizer.cpp` from before the memory fix) |
| Plots | `benchmark/synthesizer/results/before_memory_fix_bv3/plots/*.png` |
| Data and plots after the fix | `benchmark/synthesizer/results/current/` |
| Machine | Intel Core Ultra 5 125H with 14 GB of memory, every run pinned to CPU core 0. This is not the machine used for the simulator results above, so do not compare the two |
| Build | `make` (`-O2`) |
| What is timed | Wall time of `simulator --fsm <file> --synth`, including reading the FSM and writing the netlist to `/dev/null`, as the median of 3 runs (2 from 100,000 rows up) |
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
| 256 states, 2 inputs | 1,024 | 0.058 | 10 | 7,930 | 8 | 525 |
| 4,096 states, 2 inputs | 16,384 | 0.99 | 160 | 160,831 | 12 | 8,330 |
| 16,384 states, 2 inputs | 65,536 | 5.51 | 750 | 710,241 | 14 | 32,970 |
| 65,536 states, 2 inputs | 262,144 | 41.3 | 3,535 | 3,103,349 | 16 | 131,504 |
| 16 states, 14 inputs | 262,144 | 11.3 | 1,643 | 1,571,225 | 4 | 131,472 |

### Scaling

Slope of log(value) against log(size), where 1 means linear and 2 means quadratic. Time slopes use only runs of at least 0.1 s.

| Family, against | Time | Memory | Gates | Depth |
|---|---|---|---|---|
| `states`, rows | 1.28 (4 points) | 0.84 | 1.08 | 0.97 |
| `inputs_full`, rows | 1.23 (3 points) | 0.74 | 1.00 | 0.97 |
| `inputs_sparse`, inputs | 1.14 | 0.85 | 0.91 | 0.03 |
| `outputs`, outputs | 0.69 | 0.65 | 0.65 | 0.01 |

The memory slopes below 1 include small runs, where the program's fixed start-up memory dominates. Between the two largest `states` cases, 4 times the rows took 7.5 times the time and 4.7 times the memory.

`plots/slopes.csv` holds these values. Peak memory is plotted in `rows.png`.

### What the results show

- **Memory was the limit.** The largest `states` case used about 13.8 KB per row, about 8 times the size of the netlist it writes. An early trial with 1,048,576 rows reached 11 GB of 14 GB before the system killed it. The memory fix addresses this.
- **Rows cost more as states grow.** The same 262,144 rows take 41.3 s with 65,536 states and 11.3 s with 16 states. With more state bits, each row feeds more next-state lines, giving more OR gates.
- **Depth grows linearly with rows.** It is about rows ÷ 2 when half the bits are 1, because the OR gates are chained. It does not depend on the number of inputs or outputs.
- **Each extra input or output has a steady cost.** Time is close to linear in the number of inputs at fixed rows. With 4,096 rows, 1,024 outputs take 17.7 s.
- **More 1s cost more.** From 0 to 75% ones, time goes from 0.062 s to 0.220 s and gates from 24,443 to 73,479. At 100% gates fall to 28,492 because all the output columns are identical and share one chain, but time stays at 0.173 s. Depth rises to 4,107, because that chain includes every row.
- **Regular FSMs get no benefit.** A counter (36,868 gates) costs about the same as a random table (36,119), because nothing is minimized.
- **Row order hardly matters:** 32,777 gates sorted and 32,584 shuffled.
- **Unreachable states are expensive.** When 75% of the states are unreachable, removing them reduces gates from 35,196 to 8,090 and flip-flops from 10 to 8. The synthesizer does not remove them. In `unreachable.png`, the 0% case still shrinks slightly, because a random table leaves 25 of its states unreachable by chance. Its times are mostly under 0.1 s, so read them as a trend only.

### Effect of the memory fix

The synthesizer used to copy each row's full list of signal names for every next-state and output line it fed, and to look up gates by long names. Each line now lists row numbers, each row's AND chain is built once, and wires are numbers.

| FSM | Rows | Time before (s) | Time after (s) | Faster | Memory before (MB) | Memory after (MB) | Smaller |
|---|---|---|---|---|---|---|---|
| 4,096 states, 2 inputs | 16,384 | 0.987 | 0.135 | 7.3× | 160 | 58 | 2.8× |
| 65,536 states, 2 inputs | 262,144 | 41.3 | 4.16 | 9.9× | 3,535 | 1,053 | 3.4× |
| 16 states, 14 inputs | 262,144 | 11.3 | 2.38 | 4.7× | 1,643 | 456 | 3.6× |
| 256 states, 256 inputs | 4,096 | 8.99 | 1.01 | 8.9× | 823 | 306 | 2.7× |
| 256 states, 1,024 outputs | 4,096 | 17.7 | 2.06 | 8.6× | 2,129 | 620 | 3.4× |

Both versions were built against `bv3` and measured through the same command, one run after the other in the same session. Only `Synthesizer.cpp` differs.

- **Memory:** the 13 FSMs that used over 100 MB now use 2.6 to 3.6 times less. This holds across every session. The one FSM that grew, the 16-state FSM, grew by 12 KB.
- **Time:** the median speed-up is 6.6× across the 13 FSMs where both times are at least 0.1 s, with a range of 1.8× to 9.9×. The one FSM that got slower is the smallest of `inputs_full`, from 0.003 s to 0.020 s, which is below the noise level.
- **Time varies between runs, and memory does not.** On the 65,536-state FSM, earlier sessions on the same machine took 19.8 s and 33.0 s before the fix, and 3.1 s to 6.6 s after it, with almost the same memory. Treat the speed-up as a rough figure, and compare times only within one session.
- **Larger FSMs:** an FSM with 1,048,576 rows, which was killed at 11 GB before the fix, now finishes in 29 s using 2.7 GB. This was a separate single run, not part of `results.csv`.
- **The netlists did not change.** The old and new code gave byte-identical netlists, error messages and exit codes on 2,294 inputs, including all 51 benchmark FSMs and malformed files. The netlist hashes in the two benchmark runs also match on all 51.
- **Scaling is still above linear in time.** Between the two largest `states` cases, 4 times the rows now take 4.4 times the time and 4.0 times the memory. Depth and gates are unchanged, because the netlists are the same.
- **What remains:** the largest case still holds its 443 MB netlist in memory, because nothing may be written until the whole FSM has been checked.

The graph `results/current/plots/compare_before_memory_fix_bv3.png` shows the sweeps before and after. Compare `results/current/plots/slopes.csv` with care, because the faster runs leave only 2 to 5 points above the 0.1 s noise level for the time slopes. In this run, the `inputs_full` and `inputs_sparse` time slopes come out below 1, which is more likely noise than a real change.

### Reproducing

```sh
make
python3 benchmark/synthesizer/generate.py
python3 benchmark/synthesizer/run.py
python3 benchmark/synthesizer/plot.py --compare before_memory_fix_bv3
```

These write to `benchmark/synthesizer/results/current/`, leaving the saved results untouched.

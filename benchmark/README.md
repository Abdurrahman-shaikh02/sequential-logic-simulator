# Benchmarks

Shows how run time and memory grow with larger inputs. `measure.py` holds the
shared timing, so every program is measured the same way.

## Simulation

Compares our simulator and Icarus Verilog on the same circuits.

```
make
python3 benchmark/simulation/generate.py          # add --quick for small sizes
python3 benchmark/simulation/hdl_sim/run.py
python3 benchmark/simulation/icarus/run.py
python3 benchmark/simulation/plot.py              # needs matplotlib
```

Run nothing else heavy at the same time, because other processes disturb the
timings. Each command runs pinned to CPU core 0.

### How our simulator is run

Through the Worklet 3 interface, `simulator --interactive`. stdin gives the
netlist path and an input-vectors file. `hdl_sim/run.py` converts each `.st`
stimulus into a vectors file, one 0/1 column per input bit, before timing. Every
timed run writes everything the interface writes: a results table, an ASCII
waveform, JSON and VCD in `output/`, plus a report and waveform on stdout. The
interface cannot turn these off, so they are part of `run_s`. The output hash
comes from the results table of the last timed run.

The testbench checks every stimulus event at every time step, so run time grows
with rows times (rows × input bits). On long stimuli this, and not the circuit,
sets the run time.

| Output | Contents |
|---|---|
| `simulation/hdl_sim/results.csv`, `simulation/icarus/results.csv` | One row per case, same columns |
| `simulation/plots/*.png` | Graphs of time and memory |
| `simulation/plots/slopes.csv` | Slope of time against size: 1 is linear, 2 is quadratic |

`plot.py` exits 1 if the simulators produced different outputs on any case.

### Circuits

| Family | Source | Contents |
|---|---|---|
| `random` | `generate.py` | Layers of random gates, using every gate type |
| `mixed` | `generate.py` | The same, with about 9% flip-flops |
| `multiplier` | `circuits/multiplier.v`, through Yosys | Registered array multipliers, 8 to 64 bits |

`random` and `mixed` skip Yosys, because it does not finish random logic from
about 16k cells up, so their `yosys_s` is empty. `generated/` is not committed,
and fixed seeds rebuild the same files.

### What is measured

- **`run_s`:** median wall time of the whole process, including loading and parsing.
- **`compile_s`:** `iverilog` time. Our simulator has no compile step, so it is 0.
- **`yosys_s`:** time to synthesize that multiplier, the same in both CSVs.
- **`rss_kb`:** peak memory of the run. For Icarus this excludes `iverilog`.
- **`output_sha256`:** hash of the outputs. For Icarus it is taken in an extra run that
  is not timed. For ours it comes from the last timed run, which writes its results anyway.

Each case runs 3 times, or 2 times from 100,000 cells up. Times under about
0.1 s are noisy.

## Synthesizer

Measures how the synthesizer's run time, memory and netlist size change with the
FSM it is given. Each family of FSMs changes one thing, such as states, inputs or
outputs, with other params kept constant.

Full results and what they show are in `docs/benchmarks.md`. This section covers
how the scripts work. Correctness is checked separately by
`validation/synthesizer/`, so these scripts only measure.

### Running it

You need a built simulator, `python3`, `taskset`, `prlimit` and `/usr/bin/time`.
`plot.py` also needs `matplotlib` and `numpy`.

```
make
python3 benchmark/synthesizer/generate.py
python3 benchmark/synthesizer/run.py
python3 benchmark/synthesizer/plot.py --compare before_memory_fix_bv3
```

Run nothing else heavy at the same time, because other processes disturb the
timings. Each timed run is pinned to CPU core 0.

### Layout

```
benchmark/synthesizer/
├── generate.py              writes fsms/
├── run.py                   writes results/current/results.csv
├── plot.py                  writes <results folder>/plots/
├── fsms/                    one .net file per FSM, plus manifest.csv (not committed)
└── results/
    ├── current/             the latest run
    └── before_memory_fix_bv3/   saved run on bv3, from before the memory fix
```

### `generate.py`: writes the test FSMs

Deletes `fsms/`, then writes each FSM as a `.net` file named
`<family>_<value>.net`, or `<family>_<value>_<variant>.net`. It also writes
`manifest.csv`, with one row per FSM: its name, family, path and sizes. `run.py`
reads that list.

Each FSM's random numbers are seeded from its family and value, so every run
writes identical files. That is why `fsms/` is not committed.

| Option | Effect |
| --- | --- |
| `--quick` | Stops each size sweep early, to try the scripts in seconds |

A full table has `rows = states x 2^inputs`, so rows change whenever states or
inputs do. These families separate the effects:

| Family | Changes | Held constant |
| --- | --- | --- |
| `states` | 16 to 65,536 states, so rows grow too | 2 inputs, 4 outputs, every row present |
| `inputs_full` | 2 to 14 inputs, so rows grow too | 16 states, 4 outputs, every row present |
| `inputs_sparse` | 8 to 256 inputs | 256 states, 4 outputs, 4,096 rows |
| `outputs` | 1 to 1,024 outputs | 256 states, 4 inputs, every row present |
| `output_ones` | Share of output bits that are 1, from 0 to 100% | 256 states, 4 inputs, 16 outputs |
| `fill` | Share of rows present, from 10 to 100% | 256 states, 6 inputs, 4 outputs |
| `structure` | `random`, `ring`, `counter` or `two_way` table | 1,024 states, 2 inputs, 4 outputs |
| `row_order` | `sorted` or `shuffled` rows | 256 states, 4 inputs, 4 outputs |
| `unreachable` | Share of states that cannot be entered, 0 to 75%, with the variants `kept` and `removed` | 1,024 states, 2 inputs, 4 outputs before removal |

The two `unreachable` variants of a share start from the same table. `removed`
deletes every state the reset state cannot reach, including any left unreachable
by chance, so the 0% case also shrinks slightly.

### `run.py`: measures the synthesizer

Reads `fsms/manifest.csv`, and for each FSM:

1. **Timed runs.** Runs `simulator --fsm <file> --synth` 3 times, or 2 times from 100,000 rows
   up, sending the netlist to `/dev/null`. It records the median time and the
   highest peak memory.
2. **Untimed runs.** Runs it twice more, saving each netlist. It stops with an
   error if the two files differ, because synthesis should give the same result
   each time.
3. **Netlist size.** Counts the gates and flip-flops in the saved netlist and
   finds its longest path.

It writes one row at a time to `results/current/results.csv`, replacing that file,
so a stopped run keeps the rows it has finished.

| Option | Effect |
| --- | --- |
| `--sim FILE` | Measures another simulator binary. The default is `./simulator` |
| `--max-memory-gb N` | Memory limit of each run. The default is 6 |

A case over the memory limit fails alone. Its error goes to the terminal, and its
time and memory stay empty in `results.csv`, so `plot.py` skips it.

Columns in `results.csv`:

| Column | Meaning |
| --- | --- |
| `name`, `family`, `value`, `variant` | Which FSM this is. `value` is what its family changes |
| `states`, `inputs`, `outputs`, `rows` | Size of the FSM |
| `output_ones` | Number of output bits set to 1 in the table |
| `file_kb` | Size of the FSM file |
| `run_s` | Median wall time of the whole process: reading the FSM, building the netlist and writing it |
| `rss_kb` | Peak memory of the whole process |
| `gates`, `and_gates`, `or_gates`, `not_gates` | Gates in the netlist |
| `flip_flops` | Flip-flops in the netlist |
| `depth` | Longest path through the netlist, in gates |
| `netlist_kb` | Size of the netlist file |
| `output_sha256` | Hash of the netlist file |

Times under about 0.1 s are noisy. The netlist's header includes the FSM's path,
so `output_sha256` also changes if `fsms/` moves. To check that a code change kept
the netlists the same, build both versions and compare their output on the same
files.

### `plot.py`: draws the graphs

Reads `results/<folder>/results.csv` and writes to `results/<folder>/plots/`. The
folder defaults to `current`.

| Command | Effect |
| --- | --- |
| `plot.py` | Graphs for `results/current/` |
| `plot.py before_memory_fix_bv3` | Redraws the graphs of a saved run |
| `plot.py --compare before_memory_fix_bv3` | Graphs for `results/current/`, and also compares it with a saved run |

`--compare` exits 1 if the gates, flip-flops or depth differ from the other run on
any FSM, because then the two runs did not do the same work.

| Output | Shows |
| --- | --- |
| `rows.png` | Time, memory and longest path against rows, adding states or adding inputs |
| `inputs.png` | Time, memory and gates against inputs, at 4,096 rows |
| `outputs.png` | Time, memory and gates against outputs, at 4,096 rows |
| `output_ones.png` | Time, gates and longest path against the share of output bits that are 1 |
| `fill.png` | Time, memory and gates against the rows present |
| `unreachable.png` | Gates and time with unreachable states kept or removed |
| `structure_and_order.png` | Gates by kind of FSM and by row order |
| `slopes.csv` | Slope of each measure against size: 1 is linear, 2 is quadratic. Time slopes use only runs of at least 0.1 s, and `time_points` says how many |
| `compare_<folder>.png` | Time and memory before and after, from `--compare` |

### Saving a run to compare later

`run.py` always replaces `results/current/`, so copy a run you want to keep:

```
cp -r benchmark/synthesizer/results/current benchmark/synthesizer/results/<name>
python3 benchmark/synthesizer/plot.py --compare <name>     # after a later run
```

`results/before_memory_fix_bv3/` is a saved run like this, so do not overwrite it.
Compare times only from runs on the same machine.

`generate.py --quick` followed by `run.py` replaces `results/current/results.csv`
with the small sizes only. Rerun both scripts without `--quick`, or restore the
file with `git restore`, before plotting or comparing.

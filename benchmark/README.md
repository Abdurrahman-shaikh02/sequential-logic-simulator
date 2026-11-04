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
- **`output_sha256`:** hash of the outputs, taken in an extra run that is not timed.

Each case runs 3 times, or 2 times from 100,000 cells up. Times under about
0.1 s are noisy.

## Synthesizer

Measures our synthesizer on generated FSMs. Each family changes one thing, such
as states, inputs or outputs, and holds the others still.

```
make
python3 benchmark/synthesizer/generate.py         # add --quick for small sizes
python3 benchmark/synthesizer/hdl_synth/run.py    # about 5 minutes
python3 benchmark/synthesizer/plot.py             # needs matplotlib
```

| Output | Contents |
|---|---|
| `synthesizer/hdl_synth/results.csv` | One row per FSM |
| `synthesizer/hdl_synth/plots/` | Graphs and `slopes.csv`, from `plot.py` |
| `synthesizer/before_memory_fix/` | Saved results, graphs and slopes from before the memory fix |

Results are in `docs/benchmarks.md`.

`before_memory_fix/` is there for comparison, so do not overwrite it. `run.py`
only writes to `hdl_synth/`, and `plot.py before_memory_fix` redraws the saved
graphs from the saved results. Fixed seeds generate the same FSMs each time.
Compare times only from runs on the same machine.

### What is measured

- **`run_s`, `rss_kb`:** median wall time and peak memory of the whole process.
- **`gates`, `and_gates`, `or_gates`, `not_gates`, `flip_flops`:** size of the netlist.
- **`depth`:** the longest path through the netlist, in gates.
- **`netlist_kb`:** size of the netlist file.
- **`output_sha256`:** hash of the netlist. The script stops if two runs differ.

Each run has a memory limit of 6 GB, changed with `--max-memory-gb`. A case over
the limit fails alone and leaves its time and memory empty.

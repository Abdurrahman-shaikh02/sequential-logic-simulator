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

Not built yet.

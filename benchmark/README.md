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

Not built yet.

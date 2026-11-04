# File formats

| Page | File | Read by |
|---|---|---|
| [netlist.md](netlist.md) | `.table` gate-level netlist (Yosys `write_table`) | The parser |
| [testbench.md](testbench.md) | `.tb` testbench: circuit, clock, stimuli, assertions, invariants | `--testbench`, the GUI |
| [vectors.md](vectors.md) | Input vectors, one row per step, optional expected outputs | `--interactive` |
| [fsm.md](fsm.md) | `.net` FSM transition table | The synthesizer, `--fsm`, `FSM` in a testbench |
| [output.md](output.md) | Report, results table, ASCII, JSON and VCD waveforms, truth table | Written by every run |

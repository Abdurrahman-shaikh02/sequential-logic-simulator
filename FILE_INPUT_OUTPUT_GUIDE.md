# Generic file-based input and output (CLI + GUI)

## CLI
From project root, build: `make simulator`.

```bash
python3 tools/run_file_testbench.py --netlist tests/netlists/combinational/and/netlist.table --testbench examples/and_gate/and_gate.tb --out-dir results/and
```

Pass any supported `.table`/`.tbl` or FSM `.net` file and a matching `.tb` containing stimuli and expected-output assertions. Three files are written to the output folder: `simulation.vcd`, `waveform.txt`, `waveform.json`.

## GUI
Run `python3 gui/server.py 8080`, visit http://localhost:8080. Expand **Advanced: Run a custom FSM testbench**, browse for a `.tb` file and optionally a `.table` / `.net` file. Choose **Run this testbench**. The result and waveform appear, and **Download VCD/TXT/JSON** saves traces.

A `.tb` file alone continues to work for circuits already included in the project. A user-selected external netlist replaces the `NETLIST`/`FSM` source directive in that testbench for that run. Other .tb instructions (clock, timed stimuli and assertions) remain unchanged.

For meaningful verification, provide expected-output `ASSERT` statements; a run with zero assertions does not verify logical correctness. Only supported netlist syntax is accepted, and external subfiles referenced by a netlist are not uploaded automatically.

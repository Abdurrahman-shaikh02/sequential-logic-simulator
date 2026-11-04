# GUI file-based testbench input

Open the GUI at http://localhost:8080, expand **Advanced: Run a custom FSM testbench and download waveforms**, and choose **Browse / Upload .tb file**. Select a local `.tb` testbench (max 1 MB). Its text loads into the existing editable testbench field. Click **Run this testbench** to send the text via the existing `/api/simulate` endpoint to the Python server, which runs the actual C++ simulator. Results, PASS/FAIL and waveform display work as before.

Example files: `examples/and_gate/and_gate.tb`, `examples/traffic_light/traffic_light.tb`, `examples/vending_machine/vending_machine.tb`.

**Important:** The browser reads the selected local file; it does not upload it to a permanent server location. All circuit/netlist paths referenced inside the `.tb` file must already exist within the project on the server and be valid for the simulator. You can edit the loaded text before running. A maximum 1 MB file size is enforced in the browser.

To run from the terminal: `./simulator --testbench examples/and_gate/and_gate.tb`.


## CLI input-vector file mode

The interactive circuit option also accepts an optional input-vector file. After selecting a circuit, enter the path to a text file. Each non-comment row contains one 0/1 value for each primary input, in the order printed by the simulator. Values may be separated by spaces, commas, or tabs; `#` starts a comment. For example:

```text
# A B
0 0
0 1
1 0
1 1
```

Rows are applied at 10-time-unit intervals. This simple vector mode is intended for circuits that do not require user-configured clock timing. For clocked sequential circuits and FSMs, use a `.tb` file with `CLOCK`, `MAX_TIME`, timed `@` stimuli, and optional assertions.

## Tabular simulation results

Every `.tb` simulation now also writes `output/results.txt`. Interactive input-vector simulation writes `output/interactive_results.txt`. These files contain timestamped signal values in table form, in addition to the VCD, ASCII waveform, and GUI JSON files. The standalone `tools/run_file_testbench.py` helper copies all four result artifacts into its selected output directory.

When an uploaded netlist is paired with a `.tb` file, the server replaces an existing `CIRCUIT`, `NETLIST`, or `FSM` directive with the uploaded netlist path. If no circuit directive is present, it inserts the appropriate directive.

## Maintenance scope for this update

This update is limited to Worklet 3 integration: `main.cpp` (W3 CLI), `gui/server.py` (W3 upload bridge), and `tools/run_file_testbench.py` (W3 file runner), plus this guide. No Worklet 1 or Worklet 2 implementation, parser, gate, flip-flop, FSM synthesis, or netlist source files were changed.

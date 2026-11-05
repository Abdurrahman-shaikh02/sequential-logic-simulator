# Demo 1: from an FSM table to a verified waveform

**Shows:** the three worklets as one pipeline. A state table goes into the synthesizer (Worklet 2), becomes a gate netlist that the parser and simulator run (Worklet 1), and is driven, checked and exported by the testbench (Worklet 3).

**Covers:** REQ-03 (FSM synthesis), REQ-04 (dead and unreachable states), REQ-05 (testbench and assertions), REQ-06 (exports). **Time:** about 2 minutes.

This folder is a complete package: it has its own copy of the simulator, and everything the commands write stays inside it. Move it anywhere.

## Before the review

```sh
cd demo1
bash check.sh        # one PASS/FAIL line per step; leaves the folder as it was
```

Open a terminal at least 100 columns wide, `cd` into this folder, and clear the screen. Tab completes the file names.

## The commands

Type these in order, from inside this folder.

### 1. Analyze an FSM from its table

```sh
cat dead_unreachable.net
./simulator --fsm dead_unreachable.net
```

**On screen:** the table, then `dead states: T V H` and `unreachable: U V H`, then the synthesized netlist's file name.

**Say:** "Before building anything, we check the FSM from its table. T, V and H are traps: once entered, they can never be left. U, V and H can never be reached from the reset state A. These are design mistakes, found without simulating."

### 2. Synthesize the traffic light into gates

```sh
cat traffic_light.net
./simulator --fsm traffic_light.net --synth -o traffic_light.table
head -12 traffic_light.table
```

**On screen:** a 6-row table, no output from the second command (it succeeded), then the start of the netlist: the state encoding `Sg=00 Sy=01 Sr=10`, the inputs, and the first gates.

**Say:** "This 6-row table is the whole design. The synthesizer encodes the 3 states in 2 flip-flops, and builds the next-state and output logic from AND, OR and NOT gates. The result is 20 gates and 2 flip-flops, in the same netlist format Yosys writes."

### 3. Check the parser accepts it

```sh
./simulator --check traffic_light.table && echo OK
```

**On screen:** `OK`.

**Say:** "The same parser that reads Yosys netlists accepts the synthesized one."

### 4. Simulate it and check the assertions

```sh
cat traffic_light.tb
./simulator --testbench traffic_light.tb
echo $?
```

**On screen:** the testbench, then the ASCII waveform (Green, Yellow, Red, Green), then `Total Evaluated : 31`, `Passed : 31 (100%)`, `[ PASSED ]`, and `0`.

**Say:** "The testbench names the FSM file directly, so it is synthesized on load. It resets the light, enables it, and checks each change, one per rising clock edge. The invariant checks at every time step that red and green are never on together. 31 checks, all passed, exit code 0."

Worth pointing at in the waveform:
- Each light changes just after a rising edge of `clk` (15, 25, 35), once `en` is 1.
- `G` stays on through the edge at 5, because `en` is still 0.

### 5. Show the exported files

```sh
ls output/
cat output/results.txt
```

**On screen:** `results.txt`, `simulation.vcd`, `waveform.json`, `waveform.txt`, then the results table: one row per time step.

**Say:** "The same run writes a results table, an ASCII waveform, JSON for the GUI, and a standard VCD file for any waveform viewer."

Optional, if GTKWave is installed (`sudo apt install gtkwave`):

```sh
gtkwave output/simulation.vcd
```

Click the module in the top-left panel, select the signals below it, click **Append**, then press **Ctrl+Shift+F** to fit.

### 6. Optional: the same circuit from an input-vectors file

```sh
cat inputs.txt
printf 'traffic_light.table\ninputs.txt\n' | ./simulator --interactive
```

Or run `./simulator --interactive` and type `traffic_light.table`, then `inputs.txt`.

**On screen:** the 10 rows (inputs, then the expected lights after `|`), then `Total Evaluated : 30`, `Passed : 30 (100%)`, `[ PASSED ]`.

**Say:** "Instead of a timed testbench, the same circuit can be driven one step at a time from a plain file. The clock is just a column, and the expected outputs after the bar are checked on every row: 10 rows, 3 lights, 30 checks."

## Questions the panel may ask

- **Is the reset synchronous?** Yes. Each flip-flop's D input is ANDed with NOT reset, so reset takes effect on the next rising edge.
- **Is the logic minimized?** No. Each row becomes one AND term, so the circuit is correct but not the smallest.
- **How do you know the synthesizer is right in general, not just for this FSM?** The synthesizer validation checks 2,139 FSMs against their own tables, and catches all 12 deliberately injected bugs (slide 13, and `docs/validation.md`).

## If something fails

| Problem | Fix |
|---|---|
| `No such file or directory: ./simulator` | Copy it back in: from the project root, `make && cp simulator demoandppt/demo1/` |
| `Permission denied` on `./simulator` | `chmod +x simulator` (some copy tools drop the permission) |
| The waveform wraps | Widen the terminal, or zoom out |
| A file was edited by mistake | The `.net` files are copies of `docs/fsm_example/` in the project, and `traffic_light.tb` of `examples/traffic_light/` (with its `FSM` line pointing here) |

## Files

| File | What it is |
|---|---|
| `simulator` | A copy of the simulator, built from `main` |
| `dead_unreachable.net` | An FSM with traps and unreachable states |
| `traffic_light.net` | The traffic-light FSM |
| `traffic_light.tb` | The traffic-light testbench: reset, enable, 5 assertions and 1 invariant |
| `inputs.txt` | 10 input rows for the traffic light, with the expected lights |
| `check.sh` | The pre-review check above |

The commands write `dead_unreachable.table`, `traffic_light.table` and `output/` into this folder. All are safe to delete, and `check.sh` deletes them.

## Updating the simulator copy

The copy does not change when the project's code does. After rebuilding, copy it in again from the project root:

```sh
make && cp simulator demoandppt/demo1/
```

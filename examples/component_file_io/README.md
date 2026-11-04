# Component examples

One folder per supported component, each with a netlist and two ways to drive it: an input-vectors file for `--interactive`, and a testbench file with assertions for `--testbench`. The GUI's Circuit Workbench loads these same testbenches.

| Folder | Components |
|---|---|
| `combinational/` | `and`, `or`, `not`, `xor`, `nand`, `nor`, `mux`, `andnot`, `ornot` |
| `sequential/` | `dff_positive`, `dff_negative`, `jkff_positive`, `jkff_negative`, `tff_positive`, `tff_negative`, `shift_register` (4-bit serial-in, parallel-out) |
| `fsm/` | `traffic_light`, `vending_machine` |

`component_manifest.csv` lists every component with its netlist and its main input file.

## Files in each folder

| File | What it is |
|---|---|
| `netlist.table` | The circuit, in the netlist format ([docs/formats/netlist.md](../../docs/formats/netlist.md)) |
| `input.txt` | Input vectors, one row per step ([docs/formats/vectors.md](../../docs/formats/vectors.md)). All the sequential and FSM files, and `and`, also give the expected outputs after a `\|`, so the run checks them |
| `testbench.tb` | Timed stimuli, a clock and assertions ([docs/formats/testbench.md](../../docs/formats/testbench.md)) |
| `netlist.net` | FSM folders only: the FSM table the netlist was synthesized from ([docs/formats/fsm.md](../../docs/formats/fsm.md)) |

## Running a component

From the project root:

```sh
# input vectors: answer the two prompts with the netlist and the input file
printf 'examples/component_file_io/sequential/dff_positive/netlist.table\nexamples/component_file_io/sequential/dff_positive/input.txt\n' \
    | ./simulator --interactive

# testbench with assertions
./simulator --testbench examples/component_file_io/sequential/dff_positive/testbench.tb
```

`./simulator --interactive` can also be run without the pipe, to type the two paths. Menu option 3 does the same.

In an input file, steps are 10 time units apart, and a sequential circuit's clock is an ordinary input column, so each clock edge takes two rows (`0` then `1`). The columns follow the input order the simulator prints: sorted by name, then by bit. The `# columns:` header names them.

A run passes when it exits with 0 and reports `[ PASSED ]`. If a file has no expected outputs, the report shows `Total Evaluated : 0` and still says `[ PASSED ]`, which only means the run finished.

For the FSM folders, give `--interactive` the `netlist.table`. Giving it `netlist.net` synthesizes the table again and overwrites `netlist.table` next to it.

The FSM testbenches load the FSM tables themselves: `tests/netlists/fsm/traffic_light/traffic_light_fsm.net` (the same table as `fsm/traffic_light/netlist.net`) and `examples/vending_machine/vending_machine.net`.

## Checking every component

```sh
bash examples/component_file_io/run_all_checks.sh
```

Runs every `testbench.tb` and requires at least one assertion and no failures. It also runs each combinational `input.txt` through `--interactive`. It does not run the sequential and FSM `input.txt` files. Logs and each run's output files are saved under `output/component_file_io/`.

## Outputs

Every run overwrites the files in `output/`: `results.txt`, `waveform.txt`, `waveform.json` and `simulation.vcd` for a testbench, with an `interactive_` prefix for `--interactive` ([docs/formats/output.md](../../docs/formats/output.md)). Copy them to keep a run.

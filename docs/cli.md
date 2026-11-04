# Command line

```
simulator <command> <file> [options]
```

| Command | File | Does |
|---|---|---|
| `sim` | netlist | Simulates the netlist with a stimulus file and writes the output trace |
| `synth` | FSM | Synthesizes an FSM table into a netlist |
| `truth` | netlist | Writes the truth table of a combinational netlist |
| `ports` | netlist | Prints the primary inputs and outputs as stimulus and trace headers |

`simulator -h` shows the general help, and `simulator <command> -h` the help for one command.

## `sim`

```sh
simulator sim <netlist> -s <stimulus> [-o <trace>] [--no-dump] [--steps N]
```

| Option | Meaning |
|---|---|
| `-s`, `--stimulus FILE` | Stimulus file. Required. Format: [formats/stimulus.md](formats/stimulus.md) |
| `-o`, `--out FILE` | Write the trace here instead of to standard output |
| `--no-dump` | Do not write a trace. Useful when timing with `-p` |
| `--steps N` | Stop after `N` stimulus rows |

For each stimulus row, `sim` drives every input to the row's value, runs the simulator until the circuit settles, and samples every output. The trace format is in [formats/output.md](formats/output.md).

`--no-dump` and `-o` cannot be used together.

## `synth`

```sh
simulator synth <fsm> [-o <netlist>]
```

Writes the netlist to standard output, or to the `-o` file. The FSM format is in [formats/fsm.md](formats/fsm.md), and how the synthesizer works is in [synthesizer/synthesizer.md](synthesizer/synthesizer.md).

The library function `Synthesizer::synthesize(path)` instead writes the netlist next to the input file. The command line does not do this.

## `truth`

```sh
simulator truth <netlist> [-o <table>]
```

Runs all 2^n input combinations and writes one row for each. This fails with exit code 3 if the circuit has a flip-flop, has no inputs, or has more than 20 input bits. The table format is in [formats/output.md](formats/output.md#truth-table).

## `ports`

```sh
simulator ports <netlist>
```

```
// inputs: clk d[3:0] rst
// outputs: q[3:0]
```

Ports are sorted by name, and buses are shown as `name[N:0]`. Copy the first line into your stimulus file as its header.

## Common options

| Option | Meaning |
|---|---|
| `-p`, `--perf` | Measure the run and print statistics to standard error |
| `--repeat N` | Run `N` times and report the median time of each phase. Needs `-p` |
| `--warmup N` | Run `N` untimed times first. Needs `-p` |
| `--stats FORMAT` | Statistics format: `text` (default), `json` or `csv` |
| `-q`, `--quiet` | Hide warnings from the parser and synthesizer. Errors are still shown |
| `-v`, `--verbose` | Print progress to standard error |
| `-h`, `--help` | Show help |

`-q` and `-v` cannot be used together. `-s`, `--no-dump` and `--steps` work only with `sim`, and `-o` works with every command except `ports`.

With `-p`, every run, warm-ups included, does the whole job from scratch: parse, read the stimulus, initialize and simulate. The output of the last run is the one written. The statistics are described in [formats/output.md](formats/output.md#statistics).

The statistics header says `UNOPTIMIZED BUILD` when the binary was built without optimization, for example with `make debug`. Do not use such numbers for comparisons.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | Usage error: unknown command or option, or a missing or invalid argument |
| 2 | The netlist, FSM or stimulus file could not be read or is invalid |
| 3 | Runtime error: the output file could not be opened or written, or `truth` was given a sequential or too large circuit |

Errors and warnings go to standard error. Parse errors from the stimulus reader show `file:line: message`.

## Examples

```sh
# Trace to a file
simulator sim design.table -s design.stim -o design.trace

# Time 5 runs after 1 warm-up, without writing a trace, as JSON
simulator sim design.table -s design.stim --no-dump -p --warmup 1 --repeat 5 --stats json

# Synthesize, then simulate
simulator synth fsm.net -o fsm.table
simulator sim fsm.table -s fsm.stim
```

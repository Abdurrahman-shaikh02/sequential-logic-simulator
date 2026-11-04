# Output formats

## Trace

Written by `simulator sim`.

```
// outputs: carry sum[3:0]
0 0000
0 0101
1 0010
```

- The header lists every primary output, sorted by name, with buses written as `name[N:0]`.
- There is one row per stimulus row, holding the outputs after that row settled.
- Buses are written MSB first, the same as the stimulus.

The header has the same form as a stimulus header. `validation/icarus/validate.py` makes Icarus print the same format, so the two outputs can be compared with a plain text diff.

## Truth table

Written by `simulator truth`.

```
a[1] a[0] b | y[1] y[0]
0 0 0 | 0 0
0 0 1 | 0 0
0 1 0 | 0 1
...
```

- Inputs come before the `|` and outputs after it. Each is sorted by port name and expanded to single bits, MSB first.
- There are 2^n rows, counting up in binary with the first column as the most significant bit.

## Statistics

Written to standard error by any command run with `-p`. The `text` format is meant for reading:

```
$ simulator sim tests/fixtures/netlists/dffp/netlist.table -s dff.stim --no-dump -p
== sim tests/fixtures/netlists/dffp/netlist.table  (1 run)
  parse ms      0.214
  init ms       0.002
  simulate ms   0.003
  io_read ms    0.018
  nodes         4  (DFF_P 1, InputNode 2, OutputNode 1)
  input bits    2
  output bits   1
  flip-flops    1
  steps         5
  comb ops      12
  commits       4
  seq ops       4
  ops/step      4.000
  ops/sec       7039774.727
  peak rss KB   3992
```

`--stats json` gives one JSON object, and `--stats csv` a header line followed by one data line. Both have the same fields:

| Field | Meaning |
|---|---|
| `command`, `design` | What was run |
| `runs` | Timed runs, not counting warm-ups |
| `optimized` | Whether the binary was built with optimization |
| `parse_ms` | Reading and checking the netlist and building the graph |
| `synth_ms` | Synthesis (`synth` only) |
| `init_ms` | Setting every input to 0 and settling the circuit |
| `simulate_ms` | Applying every stimulus row. For `truth`, all rows, including formatting them |
| `io_read_ms` | Reading the stimulus file |
| `io_write_ms` | Formatting and writing the trace |
| `nodes` | Nodes in the graph: gates, flip-flops, primary inputs and outputs, and constant drivers |
| `input_bits`, `output_bits` | Width of all primary inputs and outputs |
| `flip_flops` | Number of sequential nodes |
| `steps` | Stimulus rows simulated |
| `comb_ops` | Operations run from the combinational queue |
| `commits` | Nodes committed from the topological queue |
| `seq_ops` | Operations run from the sequential queue |
| `ops_per_sec` | (`comb_ops` + `commits` + `seq_ops`) ÷ simulate time |
| `peak_rss_kb` | Peak memory of the whole process |
| `nodes_by_type` | Node count by C++ class (JSON only) |

A time is the median over the timed runs. A field that does not apply to the command is empty in CSV and `null` in JSON. The work counters (`comb_ops`, `commits`, `seq_ops`) are the same on every run. [architecture/simulation.md](../architecture/simulation.md) explains what they count.

# Input-vectors format

The file `./simulator --interactive` reads after the circuit path: one row per simulation step, one 0/1 value per input bit, and optionally the expected outputs. It is the only way to drive single bits of an input bus, so validation and benchmarks use it. The reader is `runInteractiveSimulation` in `main.cpp`.

## Inputs only

```
# clk rst
0 1
1 1
0 0
1 0
```

- One row per step. Row `k` (from 0) is applied at time `10 × k`.
- Each row has exactly one value per input **bit**, in the order `--interactive` prints the inputs: sorted by name, then by bit index. A bus `a[3:0]` contributes `a[0] a[1] a[2] a[3]`, in that order.
- Values are `0` or `1`, separated by spaces, tabs or commas.
- `#` starts a comment. Blank lines and comment lines are skipped.
- Every row sets every input, but only inputs whose value changed cause any work.

Such a run has nothing to check. Before running, the simulator prints `Input-only file: no expected outputs supplied; assertion report will be NOT VERIFIED`. The report itself then shows `Total Evaluated : 0` and `[ PASSED ]`, and the exit code is 0. Nothing has been verified.

## With expected outputs

Add a `# columns:` header naming the input and output columns, and put the expected outputs after a `|` on each row:

```
# columns: clk d | q
1 0 | 0
0 1 | 1
1 1 | 1
0 0 | 0
```

(`examples/component_file_io/sequential/dff_negative/input.txt`, a negative-edge D flip-flop.)

- The header must list **exactly** the circuit's input bits and output bits, each side in the sorted order the simulator uses, with bus bits written `name[i]`. Otherwise the run stops with `Columns header does not match this circuit` and prints the expected names.
- Once the header has been seen, every row must have the `|` and one value per output bit. Without a header, a row with `|` is an error.
- Each expected value becomes an assertion at that row's time, checked after the circuit has settled. A mismatch fails the run, with exit code 1.
- Only one header is allowed.

## Clocks and data

There is no `CLOCK` here: a clock is an ordinary input column, and each row is one instant. A clock cycle therefore takes at least two rows.

When a row changes a data input and gives a flip-flop its active clock edge, the flip-flop samples the **new** data. To get results that match other simulators, change data only on rows where the clock does not make an active edge. The validation scripts use three rows per cycle for this: `(clk=0, new data)`, `(clk=1, same data)`, `(clk=0, same data)`. See [validation.md](../validation.md#inputs).

## Errors

The run stops with exit code 1 and a message naming the file and line when:

- the file cannot be opened, or has no rows
- a value is not 0 or 1
- a row has the wrong number of input values, or of expected outputs
- the header is malformed, repeated, or does not match the circuit

## Output

After the run, `output/interactive_results.txt` has one row per input row, with every input and output bit. See [output.md](output.md). The four `interactive_*` files are overwritten by every run.

## Typing values by hand

Press Enter at the vectors-file prompt instead of giving a path. The simulator then asks, step by step, for each input bit in the same order. Type `q` at a step prompt to finish and run. Expected outputs cannot be given this way.

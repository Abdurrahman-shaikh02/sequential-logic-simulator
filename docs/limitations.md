# Limitations

Known issues and unsupported features, grouped by component. Each one says what happens, and how to work around it where possible. Everything here was checked against the code on `main` at commit `b92a64b`.

## Simulation model

| Limitation | Effect | Workaround |
|---|---|---|
| Two values only, with no `X` or `Z` | Uninitialized state reads 0, and tri-state buses cannot be modeled | End the Yosys script with `setundef -zero` and `setundef -init -zero`, as the flows in `validation/flows/` do |
| Zero delay | No timing, races or glitches are visible | None. This is by design (see [decisions](architecture/decisions.md#zero-delay-two-values)) |
| No hold-time check | When data and the active clock edge change at the same time, the flip-flop samples the new, settled data with no warning. Icarus or real hardware may give a different result | Change data between active edges |
| No asynchronous set or reset, and no enable pins | Yosys cells such as `$_DFF_PP0_`, `$_DFFE_*`, `$_SDFF_*` and `$_DLATCH_*` are rejected as unknown types | Map them to plain flip-flops and logic with `dfflegalize` / `dffunmap`, and use synchronous resets |
| No latches or combinational loops | The netlist is rejected with `cycle detected in circuit` | None |
| Flip-flops power up at 0 | No other initial value can be given | Use a reset |
| One circuit per process | `Simulator` state is global | Run separate processes, as the GUI does |

## Netlist format and parser

| Limitation | Effect | Workaround |
|---|---|---|
| `1'x` is read as 0 | Silent | `setundef -zero` in the Yosys script |
| A primary output tied directly to a constant (`assign y = 2'b01;`) | Constants are recognized only on cell inputs, so the netlist is rejected with "wire 2'01 has no driver", or "wire 1'0 has no driver" for a single bit | None yet. Avoid constant outputs in the Verilog |
| A bus port built from separate signals | Yosys writes it as `{ \d \c \b \a }`, which is rejected as a wire with no driver | Drive the port from a single vector (see `validation/README.md`) |
| One module only | Rejected with "multiple top-level modules" | `flatten` before `write_table` |
| A flip-flop clocked by logic that depends on its own output | Rejected as a cycle | None |
| Bus ports given without a range | Each one scans every wire to find its bits, which is slow with many such ports on a huge netlist | None needed so far |

## Testbench files (`.tb`)

| Limitation | Effect | Workaround |
|---|---|---|
| A stimulus cannot set one bit of a bus | `a=5` sets **every** bit of `a` to 1, and `a[0]=1` fails with `Unknown primary input` | Use an input-vectors file ([formats/vectors.md](formats/vectors.md)) |
| `ASSERT` on a bus name without an index | Checks bit 0 only | Assert each bit: `q[0]=1 q[1]=0` |
| A value or time that is not a number (`a=x`, `@ten`) | The simulator stops with an uncaught `std::invalid_argument`, without a line number | Check the file by eye |
| Unknown directives | Ignored silently, so a typo such as `ASSRT` loses a check without any message | Check the report's `Total Evaluated` count |
| `#` starts a comment even inside quotes | A message such as `"coin #2"` is cut at `#` | Avoid `#` in messages |
| Paths cannot contain spaces | `CIRCUIT my circuit.table` reads `my` | Rename the file |
| The third `CLOCK` argument | Called `highDuration` in the code and "high duration" in old notes, it is the time the clock stays **LOW** at the start of each period | Read it as `low_time` (see [formats/testbench.md](formats/testbench.md#clock)) |
| A testbench with no assertions passes | Exit code 0 and `[ PASSED ]` with `Total Evaluated : 0` | Always add assertions. The GUI shows `NOT VERIFIED` for this case |

## Testbench engine and output

| Limitation | Effect |
|---|---|
| Every snapshot is kept until the run ends | Memory grows with event times × primary bits: 1.28 GB at 90,000 rows on a circuit with 129 input and output bits ([benchmarks](benchmarks.md#where-icarus-is-faster)) |
| Snapshots are string-keyed maps | On small circuits with wide buses, building them costs more than simulating the gates ([benchmarks](benchmarks.md#cost-of-the-interface)) |
| Assertions are found by scanning the whole list at every event time | Cost grows with assertions × event times |
| Every run overwrites the files in `output/` | Copy them to keep a run, or use `tools/run_file_testbench.py --out-dir` |
| The ASCII waveform gives every event time the same width | Its horizontal scale is not linear in time |
| The VCD timescale is fixed at `1ns` | One time unit is always written as 1 ns |

## Command line

| Limitation | Effect |
|---|---|
| `--interactive` steps are fixed at 10 time units | No other spacing, and the clock is an ordinary column |
| `--truthtable` always exits 0 | Scripts must look for `[-]` in the output to detect a failure |
| `--truthtable` handles at most 20 input bits and only combinational circuits | Fails with a message otherwise |
| Paths typed at the menu cannot contain spaces | The menu reads one word |
| `tools/run_file_testbench.py` replaces only a `NETLIST` or `FSM` line | If the testbench uses `CIRCUIT`, the given netlist is added at the top and then replaced by the `CIRCUIT` line's circuit |

## Web GUI

| Limitation | Effect |
|---|---|
| No authentication, and the server listens on all interfaces | Anyone on the network can run simulations. Use it only on a trusted network |
| One simulation at a time | Requests wait for each other, because every run writes `output/` |
| The live traffic light replays its whole history on every tick | Each tick costs more than the last. A session is capped at 200 ticks |
| Uploaded netlists cannot refer to other files | Only the one file is uploaded |
| No browser end-to-end test | The API was exercised with HTTP requests, the page by hand |

## Synthesizer

| Limitation | Effect |
|---|---|
| No logic minimization | Netlists are correct sums of products, larger than needed. A regular FSM such as a counter costs as many gates as a random table of the same size ([benchmarks](benchmarks.md#what-the-results-show)) |
| Unreachable states are kept | Their rows are still turned into gates. With 75% of the states unreachable, removing them by hand cuts the gates from 35,196 to 8,090 ([benchmarks](benchmarks.md#what-the-results-show)). `--fsm <file> --unreachable` lists them |
| OR gates are chained | Logic depth grows linearly with the rows, about rows ÷ 2 when half the bits are 1. This has no cost in our zero-delay simulator, but would be slow as real hardware |
| The whole netlist is held in memory | Nothing is written until the whole FSM has been checked, so the largest benchmark case, 262,144 rows, peaks at about 1 GB |
| Binary state encoding only | No one-hot or Gray encoding |
| Synchronous reset only | Reset takes effect on the next rising clock edge |
| Outputs are not gated by reset | While `reset` is high, the outputs show the current state's outputs |
| Only `$_DFF_P_` flip-flops | No choice of clock edge |
| Plain `--fsm` writes the netlist next to the input | `x.net` produces `x.table` in the same folder, overwriting it. Use `--fsm x.net --synth -o <file>` to choose the file |

The synthesizer validation covers the netlists, not the FSM analysis: `DeadStates` and `UnreachableStates` are checked only by the unit tests ([validation.md](validation.md#what-this-does-not-cover-1)).

## Tests

| Issue | Effect |
|---|---|
| `tests/unit/combinational/or` and `ornot` have four-column `input.txt` files for two-input programs | Their output does not match `expected.txt` |
| `tests/unit/synthesizer/traffict` returns right after synthesis | It no longer checks the netlist's behavior |
| `tests/integration/{ringcounter,simple_dffp_not,sipo_register}` | Not built by any Makefile target |
| No coverage measurement | Coverage is not reported |

## Code

- `List` (a node's connection list) has no destructor, so its entries are not freed when `Simulator::clear()` drops the nodes. Each process loads only a few circuits, so this has not mattered in practice.
- `include/operations/OperationType.hpp` is not used by any code.
- Gates print `port does not exist on ...` to standard error if called with a wrong port. The parser prevents this for netlists, so it only shows up when a circuit is built by hand.

# Limitations

Known issues and unsupported features, grouped by component. Each one says what happens, and how to work around it where possible.

## Simulation model

| Limitation | Effect | Workaround |
|---|---|---|
| Two values only, with no `X` or `Z` | Uninitialized state reads 0, and tri-state buses cannot be modeled | End the Yosys script with `setundef -zero` and `setundef -init -zero`, as the flows in `validation/flows/` do |
| Zero delay | No timing, races or glitches are visible | None. This is by design (see [decisions](architecture/decisions.md#zero-delay-two-values)) |
| No hold-time check | When data and the active clock edge change on the same stimulus row, the flip-flop samples the new, settled data with no warning. Icarus or real hardware may give a different result | Change data only while the clock is inactive (see [stimulus format](formats/stimulus.md#clock-edges-and-data)) |
| No asynchronous set or reset, and no enable pins | Yosys cells such as `$_DFF_PP0_`, `$_DFFE_*`, `$_SDFF_*` and `$_DLATCH_*` are rejected as unknown types | Map them to plain flip-flops and logic with `dfflegalize` / `dffunmap`, and use synchronous resets |
| No latches or combinational loops | The netlist is rejected with `cycle detected in circuit` | None |
| Flip-flops power up at 0 | No other initial value can be given | Use a reset |
| One circuit per process | `Simulator` state is global | Run separate processes |

## Netlist format and parser

| Limitation | Effect | Workaround |
|---|---|---|
| `1'x` is read as 0 | Silent | `setundef -zero` in the Yosys script |
| A primary output tied directly to a constant (`assign y = 2'b01;`) | Constants are recognized only on cell inputs, so the netlist is rejected with "wire 2'01 has no driver", or "wire 1'0 has no driver" for a single bit or after `splitnets -ports` | None yet. Avoid constant outputs in the Verilog |
| A bus port built from separate signals | Yosys writes it as `{ \d \c \b \a }`, which is rejected as a wire with no driver | Drive the port from a single vector (see `validation/README.md`) |
| One module only | Rejected with "multiple top-level modules" | `flatten` before `write_table` |
| A flip-flop clocked by logic that depends on its own output | Rejected as a cycle | None |
| Bus ports given without a range | Each one scans every wire to find its bits, which is slow with many such ports on a huge netlist | None needed so far |

## Command line

| Limitation | Effect |
|---|---|
| `truth` handles at most 20 input bits and only combinational circuits | Fails with exit code 3 otherwise |
| The whole stimulus and trace are kept in memory | Memory grows with stimulus length (see [benchmarks](benchmarks.md#where-icarus-is-faster)) |
| No waveform output (VCD) | Only the text trace |
| Stimulus values are binary only | No `x` or `z` in stimulus rows |

## Synthesizer

| Limitation | Effect |
|---|---|
| No logic minimization | Netlists are correct sums of products, larger than needed |
| Binary state encoding only | No one-hot or Gray encoding |
| Synchronous reset only | Reset takes effect on the next rising clock edge |
| Outputs are not gated by reset | While `reset` is high, the outputs show the current state's outputs |
| Only `$_DFF_P_` flip-flops | No choice of clock edge |
| FSM analysis is not on the command line | `DeadStates` and `UnreachableStates` are callable only from C++, and are tested only by an archived test (see [fsm-analysis.md](synthesizer/fsm-analysis.md#testing)) |

> **TODO (synthesizer validation):** add anything the synthesizer validation and benchmarks find.

## Code

- `include/operations/OperationType.hpp` is unused, and still lists the removed `SET_PORT` operation.
- Gates print `port does not exist on ...` to standard error if called with a wrong port. The parser prevents this for netlists, so it only shows up when a circuit is built by hand.

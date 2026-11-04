# Testbench format (`.tb`)

A testbench names a circuit and says what happens to its inputs over time, and what its outputs must be. Run one with `./simulator --testbench file.tb`, or paste one into the GUI's Advanced panel. The parser is `Testbench::loadTestbenchFile` in `src/testbench/Testbench.cpp`.

```
# traffic light: Green -> Yellow -> Red while enabled
FSM tests/netlists/fsm/traffic_light/traffic_light_fsm.net
CLOCK clk 10
MAX_TIME 60

@0 reset=1 en=0
@6 reset=0 en=1

ASSERT @7  G=1 Y=0 R=0 "starts in Green"
ASSERT @16 G=0 Y=1 R=0 "first rising edge: Yellow"
ASSERT @26 G=0 Y=0 R=1 "second rising edge: Red"
INVARIANT safe !(R == 1 && G == 1) "red and green never together"
```

## Directives

One directive per line. `#` starts a comment anywhere on a line, including inside quotes. Blank lines are ignored.

| Directive | Meaning |
|---|---|
| `CIRCUIT path` | Loads a circuit. `NETLIST` and `FSM` are the same directive. The file's contents decide its type: an FSM table is synthesized first, and anything else is parsed as a netlist ([netlist.md](netlist.md#netlist-format)). The path is relative to where the simulator runs and cannot contain spaces |
| `CLOCK name period [low_time]` | Drives the primary input `name` as a periodic clock. See [Clock](#clock) |
| `MAX_TIME t` | The last time simulated |
| `@t sig=v sig=v ...` | At time `t`, sets each input. `v` is a number, and any value other than 0 means 1 |
| `ASSERT @t sig=v ... "message"` | At time `t`, after the circuit has settled, checks that each signal has value `v`. The message is optional. `ASSERT t ...` without `@` also works |
| `INVARIANT name expression "description"` | Checks `expression` at every recorded time. See [Invariants](#invariants) |

Directives are read in order. Put the circuit line first: the clock and the waveform need the circuit's signal list.

### Signal names

- Stimulus (`@t`) names must be primary inputs. A bus name sets **every** bit of the bus to the same value, so `a=5` sets every bit of `a` to 1. A single bit cannot be set: `a[0]=1` fails with `Unknown primary input: a[0]`. To drive bus bits one at a time, use an [input-vectors file](vectors.md).
- `ASSERT` names may be primary outputs or inputs, and may pick one bit of a bus: `q[2]=1`. A bus name without an index checks bit 0.
- An unknown name, or a bit index past the end of a bus, fails the run with an error. It never reads as 0.

## Time

Time is a whole number of abstract units. It orders events and does not model gate delay: every gate settles instantly. The VCD labels one unit as `1ns`.

The simulator does not visit every unit. It visits only the **event times**: 0, every stimulus time, every assertion time, and with a clock, every clock edge and `MAX_TIME`. Times after `MAX_TIME` are dropped.

At each event time `t`, in this order:

1. If there is a clock, it is set to its level at `t`.
2. The stimuli for `t` are applied, in file order.
3. The circuit settles: logic, then flip-flops whose clock reached its active edge, then the logic after them ([architecture/simulation.md](../architecture/simulation.md)).
4. Every primary input and output is recorded for the waveform.
5. The assertions for `t` are checked, then every invariant.

So an assertion at `t` sees the circuit **after** everything that happens at `t`, including a clock edge. To check the result of a rising edge at 15, assert at 15 or later. To check the value before it, assert at 14 or earlier.

### `MAX_TIME`

The default is 100. A stimulus or assertion later than the current `MAX_TIME` raises it to that time plus 10. A `MAX_TIME` line sets it outright, so a `MAX_TIME` placed after the stimuli wins over them.

## Clock

```
CLOCK clk 10        rising edges at 5, 15, 25, ...   (5 low, 5 high)
CLOCK clk 10 3      rising edges at 3, 13, 23, ...   (3 low, 7 high)
```

Every period starts LOW. The clock rises after `low_time` units, and falls again at the start of the next period:

| Time in the period | Clock |
|---|---|
| `0` to `low_time - 1` | 0 |
| `low_time` to `period - 1` | 1 |

| Argument | Default and rules |
|---|---|
| `period` | 10 if not positive |
| `low_time` | `period / 2` (at least 1) when left out, not positive, or not less than `period` |

The third argument is called `highDuration` in the code, and the old notes describe `CLOCK clk 10 3` as "3 units HIGH". What the code does is the table above: 3 units LOW, then 7 HIGH. `examples/asymmetric_clock/asymmetric_clock.tb` and the regression test `AsymmetricClock_10_3` show it.

The clock must be a primary input of the circuit. A stimulus can also set it, but the clock directive sets it again at every event time.

### Clocks and data

When a data input and the active clock edge change at the same time, the flip-flop samples the **new** data, with no warning. Apply inputs between edges, as the examples do (inputs at 6, 16, 26, edges at 5, 15, 25), when the result must match other simulators or real hardware. [architecture/decisions.md](../architecture/decisions.md#data-inputs-set-immediately-no-hold-time-check) explains why.

## Invariants

```
INVARIANT no_red_green !(R == 1 && G == 1) "red and green never together"
INVARIANT one_coin !(coin5 && coin10) "only one coin input at a time"
```

The expression is checked at every event time, against the recorded values of every primary input and output. Each check counts as one assertion in the report.

| Syntax | Meaning |
|---|---|
| `sig` | The signal is 1. Bus bits are written `q[0]` |
| `sig == 0`, `sig == 1`, `sig != 0`, `sig != 1` | Comparison with a constant |
| `!e` | NOT, binds tightest |
| `e && e` | AND |
| `e \|\| e` | OR, binds loosest |
| `( e )` | Grouping |

A malformed expression or an unknown signal name makes the invariant fail, with the reason on standard error.

## Results

The run passes when every assertion and invariant check passed. The report counts them:

```
 Total Evaluated : 4
 Passed          : 4 (100%)
 Failed          : 0
 Overall Status  : [ PASSED ]
```

A testbench with no `ASSERT` and no `INVARIANT` also passes, with `Total Evaluated : 0`, but it has verified nothing. The GUI reports that case as `NOT VERIFIED`. The output files are described in [output.md](output.md).

## Errors

| Problem | What happens |
|---|---|
| File or circuit cannot be opened, synthesis or parsing fails | `Failed to load testbench file`, exit 1 |
| Unknown signal or bit index in a stimulus or assertion | `Simulation failed: ...`, exit 1 |
| A value or time that is not a number (`a=x`, `@ten`) | The simulator stops with an uncaught `std::invalid_argument` |
| An unknown directive (`FOO bar`) | Ignored silently |

## Examples

| File | Shows |
|---|---|
| `examples/and_gate/and_gate.tb` | Every row of a combinational truth table |
| `examples/dff/dff.tb`, `examples/shift_register/shift_register.tb` | Flip-flops and a register with a clock |
| `examples/traffic_light/traffic_light.tb` | An FSM synthesized on load, 31 assertions, an invariant |
| `examples/vending_machine/vending_machine.tb` | A second FSM, 50 assertions |
| `examples/asymmetric_clock/asymmetric_clock.tb` | `CLOCK clk 10 3` |
| `examples/component_file_io/*/*/testbench.tb` | One per supported cell, the shift register and both FSMs (the GUI's Circuit Workbench uses these) |

# Generic Testbench Guide

The `.tb` testbench language can exercise a combinational netlist table or an FSM `.net` file. Paths are resolved relative to the directory from which `simulator` is launched.

## Commands

```text
CIRCUIT path/to/netlist.table
# or
FSM path/to/fsm.net
CLOCK clk 10
MAX_TIME 40
@0 input_a=0 input_b=1
@10 input_a=1
ASSERT @12 output_y=1 "output should be high"
INVARIANT no_conflicting_outputs !(out_a == 1 && out_b == 1) "outputs must not overlap"
```

- `CIRCUIT`, `NETLIST`, and `FSM` load a circuit. `.net` FSM files are synthesized before parsing.
- `CLOCK name period [high_duration]` creates a periodic clock. If omitted, the high duration is half the period.
- `MAX_TIME` sets the simulation end time.
- `@time signal=value ...` schedules scalar input assignments.
- `ASSERT @time signal=value ... "message"` checks signal values at the given time.
- `INVARIANT name expression "description"` checks a Boolean expression at every recorded simulation timestamp.

## Invariant expression grammar

Supported operators, from highest to lowest precedence:

1. `!` (logical NOT)
2. `==` and `!=` against `0` or `1`
3. `&&` (logical AND)
4. `||` (logical OR)
5. Parentheses for grouping

Bare scalar signal names are treated as Boolean values. Example:

```text
INVARIANT safe_outputs !(red == 1 && green == 1) "red and green must not be high together"
INVARIANT valid_coin_input !(coin5 && coin10) "only one coin input may be active"
```

Unknown signal names or malformed expressions fail the invariant rather than silently passing. This is a two-state simulator: values are LOW/HIGH only. Bit-vector expression support and four-state `X/Z` semantics are not implemented.

## Examples

From the project root:

```bash
make -j4
./simulator --testbench examples/traffic_light/traffic_light.tb
./simulator --testbench examples/vending_machine/vending_machine.tb
./simulator --test all
```

The testbench command returns exit code `0` only when loading and simulation succeed and all assertions/invariants pass; a failed testbench returns non-zero. Waveforms are written under `output/`.

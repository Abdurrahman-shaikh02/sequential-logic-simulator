# Stimulus format

The input file for `simulator sim`.

```
// inputs: clk rst d[3:0]
0 1 0000
1 1 0000
0 0 1010     // a trailing comment
1 0 1010
```

## Header

The first comment that starts with `inputs:` is the header. It must come before the first data row.

- It names **every** primary input of the netlist exactly once, in any order.
- A single-bit input is written as `name`, and a bus as `name[N:0]`, where N + 1 is its width.
- `simulator ports <netlist>` prints a header you can copy.

## Rows

Each row is one time step. It has one field per header entry, in header order, and each field is a binary string as wide as its input. Buses are written MSB first.

Lines that start with `//` are comments, as is anything after `//` on a data row. Blank lines are ignored.

## What one row does

1. Every input is set to its value in the row. Inputs whose value did not change cause no work.
2. The simulator runs until nothing changes. All flip-flops that see a clock edge in this row sample their inputs, and the logic after them settles.
3. Every output is sampled, giving one row of the trace.

Time has no unit and there are no delays: a row is one instant, and the circuit settles completely within it.

## Clock edges and data

A row that changes the clock and a data input together has no defined order in real hardware. It is a hold-time violation. The simulator accepts such rows and handles them by one fixed rule.

### What the simulator does

**A flip-flop that sees its active edge in a row samples the value its data inputs have after the row has settled.** In practice that is the **new** data. In order:

1. Every input in the row is set.
2. All combinational logic settles, including any gates between the inputs and the flip-flop's data pins.
3. Only then does every flip-flop with an active edge in this row compute its next state.

The details:

- It does not matter whether the data comes straight from an input or through gates: the flip-flop always samples the settled value.
- The rule is the same for rising-edge and falling-edge flip-flops, and for D, T and JK flip-flops.
- Flip-flops whose edges arrive in the same wave see each other's **old** outputs, so a shift register or counter on one clock is never affected.
- A flip-flop on a derived or gated clock samples its data after the logic that produced its clock edge has settled. If that logic also changes the flip-flop's data, as when one flip-flop's output drives both another flip-flop's clock and its data, the flip-flop samples the new value.
- **No warning is printed.** The same stimulus always gives the same trace.

Why it works this way is in [design decisions](../architecture/decisions.md#data-inputs-set-immediately-no-hold-time-check).

### Other simulators

Verilog defines no order for events in the same time step, so an event-driven simulator may let a flip-flop see either the old or the new data. Icarus Verilog does not follow one rule here. Which value a flip-flop sees can depend on whether there are gates between the input and the flip-flop. Do not expect a trace with such rows to match Icarus, or real hardware.

### Avoiding it

To get results that every simulator agrees on, change data inputs only while the clock stays at its inactive level. Three rows per cycle works for any edge polarity:

```
// inputs: clk d
0 1      // new data, clock low
1 1      // rising edge, data steady
0 1      // falling edge, data steady
0 0      // next cycle: new data, clock low
...
```

The same rows over two cycles, with `↑` and `↓` marking the edges:

| Row | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| clk | 0 | 1 ↑ | 0 ↓ | 0 | 1 ↑ | 0 ↓ |
| d | 1 | 1 | 1 | **0** (changes, clk low) | 0 | 0 |

The pattern to avoid, a hold-time violation:

```
// inputs: clk d
0 0
1 1      // d changes on the same row as the rising edge: the flip-flop samples d = 1
```

`validation/icarus/validate.py` generates stimuli in this pattern and rejects any it generates that break it.

## Initial state

Before the first row, every input is 0 and every flip-flop holds 0. The circuit has settled in that state. A clock that is 1 in the first row is a rising edge.

## Errors

The command exits with code 2 and shows `file:line: message` when:

- there is no header, or a second one
- a header entry is malformed, names an input the netlist does not have, or names one twice
- a header entry's width does not match the netlist (a bus written without `[N:0]` counts)
- the header leaves out a primary input
- a row has the wrong number of fields, a field of the wrong width, or a character other than `0` or `1`

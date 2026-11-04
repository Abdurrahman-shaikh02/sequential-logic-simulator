# FSM format

The input of the synthesizer (`simulator synth`) and of the FSM analysis functions. Files usually end in `.net`. A template is in `docs/fsm.net`, and examples are in `docs/fsm_example/`.

```
inputs:  en
outputs: R Y G
reset:   Sg

# current | en | next | R Y G
Sg | 0 | Sg | 0 0 1      # hold green
Sg | 1 | Sy | 0 0 1      # advance to yellow
Sy | 0 | Sy | 0 1 0
Sy | 1 | Sr | 0 1 0
Sr | 0 | Sr | 1 0 0
Sr | 1 | Sg | 1 0 0
```

## Header lines

| Line | Meaning |
|---|---|
| `inputs: a b ...` | Input signal names, in the order their bits appear in each row. May be empty. |
| `outputs: x y ...` | Output signal names, in the order their bits appear in each row. May be empty. |
| `reset: S` | The state the machine starts in and returns to on reset. Exactly one state. Required. |

Each header line may appear only once.

## Transition rows

```
current | input bits | next | output bits
```

- One row per transition, with four fields separated by `|`.
- A bit field has one `0` or `1` per input or output. They may be spaced (`0 0 1`) or packed (`001`).
- Outputs belong to the transition, so they may depend on the inputs (a Mealy machine). For a Moore machine, give every row of a state the same output bits.
- Two rows may not have the same current state and input bits.
- A `(state, inputs)` combination with no row is allowed. The synthesizer sends it to the reset state with all outputs 0. The FSM analysis ignores it (see [fsm-analysis.md](../synthesizer/fsm-analysis.md)).

## Names

- Signal and state names must be identifiers: `[A-Za-z_][A-Za-z0-9_]*`.
- `clk` and `reset` are reserved, because the synthesized netlist adds inputs with those names.
- A signal name may not be used twice across `inputs:` and `outputs:`.

`#` starts a comment, and blank lines are ignored.

## Errors

Errors are written to standard error as `file:line: message`, and `simulator synth` exits with code 2. The full list is in [synthesizer.md](../synthesizer/synthesizer.md#errors).

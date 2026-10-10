# Synthesizer

Converts a finite state machine, described as a transition table, into a gate-level netlist that the parser loads and the simulator runs.

```
traffic_light_fsm.net  ──Synthesizer──▶  traffic_light_fsm.table  ──Parser──▶  Simulator
     (FSM table)                              (netlist)
```

## Usage

```cpp
#include "synthesizer/Synthesizer.hpp"

std::string netlist = Synthesizer::synthesize("docs/fsm_example/traffic_light_fsm.net");
// netlist == "docs/fsm_example/traffic_light_fsm.table", or "" on error
```

`Simulator::synthesize(path)` calls the same function. The netlist is written next to the input file, with the same name and a `.table` extension. Errors are reported on `std::cerr` as `file:line: message`.

## Input: FSM format

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

- `inputs:` and `outputs:` list signal names in column order. Either may be empty.
- `reset:` names the state the machine starts in and returns to on reset. It is required.
- Each row is `current | input bits | next | output bits`. Bits may be written as `0 0 1` or `001`.
- Outputs depend on the current state and the inputs (Mealy). For a Moore machine, repeat the same output bits on every row of a state.
- A `(state, inputs)` combination with no row goes to the reset state with all outputs 0.
- Names must be identifiers (`[A-Za-z_][A-Za-z0-9_]*`), and `clk` and `reset` are reserved. `#` starts a comment.

## Output: netlist

The output follows the parser's netlist format: six columns `module cell type port direction wire`, with `#` comments.

```
# state encoding (Q1..Q0): Sg=00 Sy=01 Sr=10  (reset: Sg)
traffic_light_fsm  en      -       -    pi   \en
traffic_light_fsm  clk     -       -    pi   \clk
traffic_light_fsm  reset   -       -    pi   \reset
traffic_light_fsm  $not$0  $not    A    in   \reset
traffic_light_fsm  $not$0  $not    Y    out  $not$0_Y
...
traffic_light_fsm  $dff_p$q0  $dff_p  D    in   $and$9_Y
traffic_light_fsm  $dff_p$q0  $dff_p  CLK  in   \clk
traffic_light_fsm  $dff_p$q0  $dff_p  Q    out  $dff_p$q0_Q
...
traffic_light_fsm  G       -       -    po   $or$19_Y
```

- Primary inputs are the FSM's inputs plus `clk` and `reset`. The primary outputs are the FSM's outputs.
- Cells used: `$and`, `$or`, `$not` (ports `A B Y`) and `$dff_p` (ports `D CLK Q`).
- Generated names contain `$` (`$and$4`, `$and$4_Y`), so they can never clash with FSM signal names.
- The module name is the input file's name without its extension.

## How it works

The algorithm is in `docs/synthesis_algorithm.txt`.

1. **Encode the states in binary.** The reset state is all zeros, and the other states are numbered in the order they first appear. This needs `n = max(1, ceil(log2(#states)))` positive-edge D flip-flops, `Q0..Qn-1`.
2. **Turn each row into a minterm:** an AND over every state bit and every input, e.g. `¬Q0·¬Q1·en` for `Sg | 1`.
3. **Build each D input and each output line** as the OR of the minterms of the rows that set it to 1.
4. **Emit the gates.** Gates have two inputs, so longer ANDs and ORs are built as chains. NOT gates and identical gates are created once and shared.

No logic minimization is done: the result is a correct sum of products, not the smallest circuit.

## Reset

Reset is synchronous for now. Every flip-flop's D input is `next-state logic AND NOT reset`, so a rising clock edge while `reset = 1` moves the machine to the reset state. Flip-flops also power up at 0, which is the reset state. This may change to an asynchronous reset once the simulator has a resettable flip-flop.

## Errors

Synthesis stops without writing a file when:

- a row has the wrong number of fields or bits, or a bit that isn't `0`/`1`
- two rows have the same current state and input bits
- a signal name is repeated, reserved, or not an identifier
- the `reset:` line is missing
- an input, or the state itself, never affects anything. The parser rejects wires with no reader, so the synthesizer reports this case instead of emitting a netlist that won't load.

When the next state or an output is never 1, it is driven by the constant `reset AND NOT reset`.

## Testing

From the repo root:

```sh
make tests/unit/synthesizer/traffict
./tests/unit/synthesizer/traffict < tests/unit/synthesizer/input.txt | diff - tests/unit/synthesizer/expected.txt
```

The test synthesizes the traffic-light example, evaluates the generated netlist cycle by cycle, and prints `R Y G` for each input line `reset en`. No output from `diff` means the test passed.

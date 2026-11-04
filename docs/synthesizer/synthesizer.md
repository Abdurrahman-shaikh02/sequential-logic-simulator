# Synthesizer

Converts a finite state machine, described as a transition table, into a gate-level netlist that the parser loads and the simulator runs.

```
traffic_light_fsm.net  ──Synthesizer──▶  traffic_light_fsm.table  ──Parser──▶  Simulator
     (FSM table)                              (netlist)
```

## Usage

From the command line:

```sh
simulator synth docs/fsm_example/traffic_light_fsm.net -o traffic_light_fsm.table
```

From C++:

```cpp
#include "synthesizer/Synthesizer.hpp"

// writes the netlist next to the input file
std::string netlist = Synthesizer::synthesize("docs/fsm_example/traffic_light_fsm.net");
// netlist == "docs/fsm_example/traffic_light_fsm.table", or "" on error

// or writes it to a stream
bool ok = Synthesizer::synthesize("docs/fsm_example/traffic_light_fsm.net", std::cout);
```

`Simulator::synthesize(path)` calls the first form. It refuses to write the netlist if that would overwrite the input file. Errors are reported on `std::cerr` as `file:line: message`.

## Input: FSM format

The input format is described in [formats/fsm.md](../formats/fsm.md). Two points matter for synthesis:

- A `(state, inputs)` combination with no row goes to the reset state with all outputs 0.
- `clk` and `reset` are reserved names, because the netlist adds inputs with those names.

## Output: netlist

The output uses the Yosys `write_table` format the parser reads: six columns `module cell type port direction wire`, with `#` comments.

```
# state encoding (Q1..Q0): Sg=00 Sy=01 Sr=10  (reset: Sg)
traffic_light_fsm  en      -       -    pi   \en
traffic_light_fsm  clk     -       -    pi   \clk
traffic_light_fsm  reset   -       -    pi   \reset
traffic_light_fsm  $not$0  $_NOT_  A    in   \reset
traffic_light_fsm  $not$0  $_NOT_  Y    out  $not$0_Y
...
traffic_light_fsm  $dff$q0  $_DFF_P_  D  in   $and$9_Y
traffic_light_fsm  $dff$q0  $_DFF_P_  C  in   \clk
traffic_light_fsm  $dff$q0  $_DFF_P_  Q  out  $dff$q0_Q
...
traffic_light_fsm  G       -       -    po   $or$19_Y
```

- Primary inputs are the FSM's inputs plus `clk` and `reset`. The primary outputs are the FSM's outputs.
- Cells used: `$_AND_`, `$_OR_` (ports `A B Y`), `$_NOT_` (ports `A Y`) and `$_DFF_P_` (ports `D C Q`).
- Generated cell and wire names contain `$` (`$and$4`, `$and$4_Y`, `$dff$q0_Q`), so they can never clash with FSM signal names.
- The module name is the input file's name without its extension.

## How it works

The algorithm, and an argument that the circuit behaves like the table, are in [algorithm.md](algorithm.md). The original notes are in `docs/archive/synthesis_algorithm.txt` and `docs/archive/proof.txt`.

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
- an input, or the state itself, never affects anything (the FSM has no logic that reads it)

When the next state or an output is never 1, it is driven by the constant `reset AND NOT reset`.

## Testing

The synthesizer is covered by the `synthesizer` and `fsm_flow` tests:

```sh
make test TESTS=synthesizer     # format, errors, structure of the netlist
make test TESTS=fsm_flow        # synthesized netlists simulated against their tables
```

The `fsm_flow` tests synthesize FSMs, including randomly generated ones, then simulate the netlists and check every step against the table. That covers the next state, the outputs, missing transitions and reset. See [testing.md](../testing.md).

> **TODO (synthesizer validation):** link the synthesizer validation in [validation.md](../validation.md#synthesizer) once it is done.

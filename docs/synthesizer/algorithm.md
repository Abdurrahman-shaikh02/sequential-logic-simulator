# Synthesis algorithm

How `Synthesizer::synthesize` turns an FSM table into gates, and why the resulting circuit behaves like the table. The code is in `src/synthesizer/Synthesizer.cpp`. The original notes are `docs/synthesis_algorithm.txt` (pseudocode) and `docs/archive/proof.txt` (the correctness statement).

## Steps

### 1. Read and check the table

The FSM file is parsed (see [formats/fsm.md](../formats/fsm.md)), and all names are checked. The netlist will add `clk` and `reset` inputs, so the FSM's inputs and outputs may not use those names, and no name may appear twice.

### 2. Encode the states

The reset state gets code 0. Every other state gets the next code in the order it first appears in the table, as either a current or a next state. With `s` states the machine needs

```
n = max(1, ceil(log2(s)))
```

positive-edge D flip-flops. Flip-flop `i` holds bit `i` of the code, and its output wire is `$dff$q<i>_Q`.

For the traffic light, `Sg = 00`, `Sy = 01`, `Sr = 10`. The code `11` is never used.

### 3. One minterm per row

Each row `current | inputs | next | outputs` becomes an AND over every state bit and every input:

```
row  Sy | 1 | Sr | 0 1 0      code(Sy) = 01

minterm  =  Q0 · ¬Q1 · en
```

A literal is the wire itself when the row needs a 1 there, and its inverse when the row needs a 0.

### 4. Sum of products for each D input and each output

- `Dterms[i]` is the list of minterms whose **next** state has bit `i` set.
- `Oterms[j]` is the list of minterms whose output `j` is 1.

Each list becomes an OR of its minterms. Every D input is then ANDed with `¬reset`:

```
D_i   = (OR of Dterms[i]) · ¬reset
out_j =  OR of Oterms[j]
```

A list with no minterms is tied to the constant 0, built as `reset · ¬reset`. The netlist format has a constant, but the synthesizer builds 0 from gates instead.

### 5. Write the gates

The netlist uses only `$_NOT_`, `$_AND_`, `$_OR_` and `$_DFF_P_`. Gates have two inputs, so an AND or OR over k terms becomes a chain of k − 1 gates. Two rules keep the netlist smaller:

- Each wire is inverted at most once, and the NOT gate is shared.
- A gate with the same type and the same two inputs as an existing one is not built again. Rows that share a prefix of literals therefore share the start of their AND chains.

Generated names contain `$` (`$and$4`, `$and$4_Y`), which an FSM name cannot, so they never clash.

### 6. Check for unread wires

If an input or a state bit is never read by any gate, the FSM is rejected with "`<wire>` does not affect the next state or any output". The parser would otherwise warn about a wire with no consumers, and such an FSM usually has a mistake in its table.

No logic minimization is done. The result is correct but not the smallest circuit.

## Why the circuit behaves like the table

### The model

An FSM is `F = (S, A, O, s₀, δ)`. `δ(s, a) = (s', o)` gives the next state and the outputs for state `s` and input bits `a`. A missing row means `δ(s, a) = (s₀, 0…0)`. This is the convention the synthesizer uses.

`e(s)` is the code given to state `s`, with `e(s₀) = 0`. Different states get different codes.

### Claim

From power-up, for any sequence of input rows with a clock edge between them, the circuit's outputs equal the FSM's outputs at every step, and its flip-flops always hold `e(s)` for the FSM's current state `s`.

### Argument, by induction on clock edges

**Start.** Every flip-flop powers up at 0, which is `e(s₀)`. A clock edge with `reset = 1` also loads 0, because every D input is ANDed with `¬reset`.

**One step.** Assume the flip-flops hold `Q = e(s)` and the inputs are `a`.

- The minterm of row `r` is 1 exactly when `Q = e(current_r)` and `a = inputs_r`. Codes are unique, and no two rows have the same current state and inputs. So **at most one minterm is 1**: the minterm of the row for `(s, a)`, if that row exists.
- If the row exists with next state `s'` and outputs `o`, then `OR(Dterms[i])` is bit `i` of `e(s')`, and `OR(Oterms[j])` is `o_j`. These are exactly the bits that row adds to each list.
- If the row does not exist, every minterm is 0, so every D input and output is 0. That is `e(s₀)` with zero outputs, which matches the missing-row convention.
- With `reset = 0` the AND with `¬reset` changes nothing. At the clock edge the flip-flops load `e(s')`, and the claim holds for the next step.

Unused codes (`11` for the traffic light) are never reached: the flip-flops start at a valid code and only ever load valid codes or 0.

### What "outputs" means here

The outputs come from gates fed by the current state and the current inputs, as in a Mealy machine. They are not ANDed with `¬reset`, so while `reset` is high they show the outputs of the current state. In a simulator trace, a row with a rising clock edge shows the outputs **after** the edge, from the new state and that row's inputs.

## Testing

The regression test `FSM_Synthesis_ReferenceSequence` and the example testbenches check the claim on the traffic light and the vending machine: they synthesize the table, simulate it, and assert the expected state outputs at each clock edge. See [synthesizer.md](synthesizer.md#testing).

> **TODO (synthesizer validation):** check the claim on many FSMs against code independent of the synthesizer, for example random tables simulated both through the synthesized netlist and by a direct interpreter of the table. See [validation.md](../validation.md#synthesizer).

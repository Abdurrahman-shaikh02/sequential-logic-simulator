# Design decisions

Why the simulator works the way it does, and what each choice costs.

## Zero delay, two values

**Choice.** Gates have no delay, and a signal is only `LOW` or `HIGH`.

**Why.** The input is a synthesized netlist with no timing data. The question the simulator answers is "what does this logic compute, cycle by cycle". Without delays, every event time is one instant, and the circuit can be settled by an ordering argument instead of an event wheel.

**Cost.** There are no timing violations, races or glitches to observe, because the simulator removes them by design. There is no `X`, so an uninitialized register reads 0 rather than unknown. The validation flows set every initial value to 0 so that Icarus starts in the same state (see [validation.md](../validation.md)).

## Evaluate and commit as separate steps

**Choice.** A gate updates its output in `evaluate`, but the value reaches the gate's fanout only when the gate is **committed**, in topological order.

**Why.** Committing in topological order means each node passes on one final value per wave. If a gate called `commit` from `evaluate`, it would pass on every intermediate value. In logic where paths split and join again, each intermediate value then spreads further, and the work grows with every level. The plan for this change is in `docs/archive/optimized_evaluation.txt`.

**Cost.** A `std::set` insert and remove per changed node, which is O(log k) for k queued nodes.

## Topological priority from the parser

**Choice.** The parser runs Kahn's algorithm once. Flip-flop data inputs are left out of the graph, and clock inputs are kept. A node's priority is its index in the result.

**Why.** Removing data inputs breaks every feedback loop that goes through a flip-flop, so any legal synchronous circuit has an order. Keeping clock inputs puts a flip-flop after the logic that drives its clock, so derived clocks also work. A cycle that is left is a real combinational loop, and the parser rejects it.

**Cost.** Combinational loops, and latches built from gates, cannot be simulated.

## Flip-flop edges in their own queue

**Choice.** An active clock edge queues a `ChangeStateOperation` in the sequential queue. That queue runs only after all combinational work is done, and the Q updates are queued behind every edge.

**Why.** This gives the semantics of `always @(posedge clk) q <= d`: every flip-flop clocked in the same wave reads its inputs before any of them changes. See [simulation.md](simulation.md#clock-edges).

## Data inputs set immediately (no hold-time check)

**Choice.** A flip-flop stores a new data value as soon as it arrives. Since commit `bbfc166` there is no `SetPortOperation` and no reordering of the sequential queue.

**Before.** A data change was queued in the sequential queue as a `SetPortOperation`. Before the queue ran, `reorder()` searched it for a data change and a clock edge on the same flip-flop. When it found one, it printed `[WARNING] Hold-time violation detected.` and moved the edge first, so the flip-flop sampled the **old** data.

**Why it changed.** `reorder()` compared every pair of operations in the queue, which is O(n²) in the number of pending flip-flop operations, on every pass. On large circuits with many flip-flops it dominated the run time.

**The behavior we chose.** When a data input and the active clock edge change at the same event time, the flip-flop samples the data after the circuit has settled, which is the **new** data. It does so whether the data comes straight from an input or through gates, and it gives no warning.

**Why this rule.** In real hardware the result of a hold-time violation is undefined. In Verilog simulators it is a race, and Icarus does not follow one rule there, so there is no reference behavior to copy. Our rule falls out of the simulation order at no extra cost: the sequential queue runs after the combinational logic settles. It is also deterministic, since the same stimulus always gives the same trace.

**Cost.**
- Violations are not reported. A stimulus with one gives a trace that real hardware or another simulator may not reproduce.
- Stimuli meant to be compared with other simulators must keep data steady around active edges. `validation/icarus/validate.py` generates them that way and rejects any that do not.

## No set, reset or enable on flip-flops

**Choice.** Only plain D, T and JK flip-flops.

**Why.** Yosys can lower these features to logic around a plain flip-flop (`dfflegalize`, `dffunmap`). The synthesizer uses a synchronous reset built from gates.

**Cost.** There is no asynchronous reset. A design that needs one must be changed to use a synchronous reset before it can be simulated.

## Global simulator state

**Choice.** `Simulator` has only static members, and there is one circuit per process.

**Why.** It keeps node code simple, because any node can reach the queues without a pointer to the simulator.

**Cost.** Two circuits cannot be simulated in one process at the same time. The testbench calls `Simulator::clear()` before loading a circuit. The GUI server runs one `./simulator` process per request, one at a time.

## The testbench drives the engine from outside

**Choice.** Time, clocks, stimuli, checks and output files live in `Testbench`, not in the engine. The engine only knows "settle now".

**Why.** The engine stays small and is tested on its own. The same engine is reused by `--testbench`, `--interactive`, `--truthtable`, the test suite and the GUI. Assertions read real node values after each settle, so a check cannot pass without the circuit actually producing the value.

**Cost.** The testbench keeps a string-keyed snapshot of every primary bit at every event time. For wide circuits and long runs that, not the engine, sets the run time and memory (see [benchmarks.md](../benchmarks.md#cost-of-the-interface)).

## The GUI runs the real simulator

**Choice.** The GUI server writes a `.tb` file for each request and runs `./simulator --testbench` on it, instead of reimplementing any logic in Python or JavaScript.

**Why.** Everything the GUI shows comes from the same code that the tests and validation check. The live traffic light replays the whole input history for each tick, so no simulator process has to be kept alive between requests.

**Cost.** Each request starts a new process, and the live view's cost per tick grows with the number of ticks (capped at 200).

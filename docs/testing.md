# Testing

Three layers check the code. Each answers a different question:

| Layer | Where | Question | Needs |
|---|---|---|---|
| Unit and integration tests | `tests/` | Does each part, and the whole library, do what we intend? | `make` only |
| Validation | `validation/` | Does our simulator agree with an established simulator on the same netlist? | Yosys, Icarus |
| Archived tests | `tests/archive/` | The original standalone test programs, kept as they were | `make` only |

This page covers the first and last. Validation has its own page: [validation.md](validation.md).

## Running

```sh
make test                         # all tests, debug build (-O0)
make test TESTS=dff               # names containing "dff"
make test TESTS='parser.err_*'    # globs work
make test ARGS=-v                 # show every test and its output
make test ARGS=-x                 # stop at the first failure
make test-asan                    # AddressSanitizer + UBSan + checked STL iterators
make test MODE=release            # optimized build
make test-list                    # list test names
```

A run ends with a summary such as `==== 234 tests in 1.05s: 234 passed`. Runner options (`make test ARGS=--help`):

| Option | Meaning |
|---|---|
| `-l`, `--list` | List matching tests |
| `-v`, `--verbose` | Show every test and its captured output |
| `-x`, `--fail-fast` | Stop after the first failure |
| `--no-fork` | Run in one process, for `gdb`. A crash ends the run |
| `--timeout=S` | Per-test timeout in seconds. Default 30, 0 means none |
| `--junit=FILE` | Also write a JUnit XML report |

## The framework (`tests/framework/`)

`hdltest` is a small framework with no dependencies:

```cpp
#include "support/helpers.hpp"
using namespace hdltest;

TEST(gates, and_truth_table) {
    // ...
    CHECK_EQ(got, Signal::HIGH);
}

TEST_XFAIL(parser, multibit_constant, "2'01 is not supported yet") { ... }
```

- Assertions: `CHECK`, `CHECK_EQ/NE/LT/LE/GT/GE`, `CHECK_CONTAINS`, `CHECK_THROWS`, `CHECK_NOTHROW` and `FAIL`. Each `CHECK_*` has a `REQUIRE_*` twin that stops the test.
- `INFO(a << b)` adds context, such as the current row, to every failure while it is in scope.
- **Every test runs in its own forked process.** The simulator's state is global (see [decisions](architecture/decisions.md#global-simulator-state)), so forking keeps tests independent. It also means a crash or a hang becomes one failed test instead of ending the run.

## Helpers (`tests/support/helpers.hpp`)

| Helper | Use |
|---|---|
| `Circuit` | Builds a circuit by hand, without a netlist. Add nodes in topological order, then call `Circuit::connect`, `Circuit::init()` and `Circuit::read` |
| `NetlistSim` | Parses a netlist into the simulator, then `drive`/`set`/`get` ports by name, `settle()`, and `clock()` (pulses `clk` 1 then 0) |
| `ProbeNode`, `ProbeSeqNode` | Nodes that record every call the simulator makes to them |
| `fixture("netlists/adder/netlist.table")` | Absolute path to a file in `tests/fixtures/` |
| `TempDir`, `readFile`, `Capture` | Temporary files, reading files, capturing `std::cerr` |
| `rng()`, `randomBit()`, `randomBits(n)` | Random values with a fixed seed, so runs repeat |

## What is tested

| Suite | File | Covers |
|---|---|---|
| `node` | `unit/core/test_node.cpp` | Connections, commit, results |
| `gates` | `unit/combinational/test_gates.cpp` | Truth table of every gate type |
| `dff_p`, `dff_n`, `tff_*`, `jkff_*` | `unit/sequential/` | Edges, initialization, sampling, against reference models |
| `input_node`, `output_node`, `primary_nodes` | `unit/primary/test_primary_nodes.cpp` | Primary nodes |
| `operations` | `unit/operations/test_operations.cpp` | Each operation calls the right method |
| `operation_queue`, `topo_queue` | `unit/simulation/` | Queue order, duplicates |
| `simulator`, `truth_table` | `unit/simulation/test_simulator.cpp` | `run()`, `initialize()`, `clear()`, truth tables |
| `parser` | `unit/parser/test_parser.cpp` | Every error, buses, constants, topological priorities, cycles |
| `synthesizer` | `unit/synthesizer/test_synthesizer.cpp` | Netlist structure, every error |
| `circuits` | `integration/test_circuits.cpp` | Hand-built shift register, counters, ripple counter, adder |
| `netlists` | `integration/test_yosys_netlists.cpp` | Real Yosys netlists in `fixtures/netlists/`, against software models of their Verilog, exhaustive up to 14 input bits |
| `fsm_flow` | `integration/test_fsm_flow.cpp` | FSM → synthesizer → parser → simulator, against the table, including 40 random FSMs |

## Adding a test

1. Put it in the file for its suite, or create `tests/unit/<area>/test_<thing>.cpp`. Every `.cpp` under `tests/` except `tests/archive/` is built into the runner on its own.
2. Name it `TEST(suite, what_it_checks)`. Prefix error cases with `err_`, which is the convention in `parser` and `synthesizer`.
3. To test against a Yosys netlist, add `tests/fixtures/netlists/<name>/test.v` and the `netlist.table` made from it.
4. If the behavior is known to be wrong and is waiting for a fix, use `TEST_XFAIL` with the reason. The test then documents the bug without failing the run.

## Archived tests

```sh
make test-old                     # build and run every archived test
make test-old TESTS=and           # only paths containing "and"
```

These are the standalone programs from before the framework existed. Each is built next to its source and run from its directory, with `input.txt` on standard input. If there is an `expected.txt`, the output must match it, and otherwise the program must exit 0. They are kept for reference. The only test of the FSM analysis functions is here (`unit/synthesizer/analysist`).

> **TODO (synthesizer validation):** if the synthesizer validation adds tests or scripts, list them here.

# Testing

Several layers check the code. Each answers a different question:

| Layer | Where | Question | Run with |
|---|---|---|---|
| 7-category suite | `tests/<category>/Test*.cpp` | Does each part, and the integrated flow, do what we intend? | `./simulator --test all` |
| Parser regression | `tests/unit/parser/test_parser.cpp` | Does the parser accept and reject exactly what it should? | `make parser-test` |
| Example testbenches | `examples/` | Do the shipped examples pass their own assertions? | `make examples-test`, `examples/component_file_io/run_all_checks.sh` |
| Standalone programs | `tests/unit/`, `tests/netlists/` | The original per-component test programs | `make standalone-build` |
| Simulator validation | `validation/icarus/` | Does our simulator agree with Icarus Verilog on the same netlist? | `python3 validation/icarus/validate.py` |
| Synthesizer validation | `validation/synthesizer/` | Does each synthesized netlist behave exactly like its FSM table? Would a bug in the synthesizer be caught? | `python3 validation/synthesizer/validate.py`, `inject_bugs.py` |
| Command-line checks | `validation/synthesizer/test_cli.py` | Do `--fsm`, `--synth`, `-o`, `--dead`, `--unreachable` and `--check` give the right exit codes, output and errors? | `python3 validation/synthesizer/test_cli.py` |
| Benchmarks | `benchmark/` | How do time and memory grow, for the simulator and the synthesizer? | see [benchmarks.md](benchmarks.md) |

Run everything from the project root. Validation and benchmarks have their own pages: [validation.md](validation.md) and [benchmarks.md](benchmarks.md).

## Current results

Run on `main` at commit `b92a64b`:

| Check | Result |
|---|---|
| `./simulator --test all` | 30 / 30 passed |
| `make examples-test` | AND gate 4/4, traffic light 31/31, vending machine 50/50 assertions |
| `examples/component_file_io/run_all_checks.sh` | All components passed |
| `make parser-test` | 52 tests, 203 checks, 0 failed |
| `python3 validation/icarus/validate.py` | 26 / 26 circuits, 62,676 rows, 0 mismatches, both flows |
| `python3 validation/synthesizer/validate.py` | 2,139 FSMs, 1,570,472 checks, 0 mismatches. Of the damaged copies, 8,344 caught, 39 harmless, 0 missed |
| `python3 validation/synthesizer/inject_bugs.py` | 12 / 12 injected bugs caught |
| `python3 validation/synthesizer/test_cli.py` | 28 / 28 checks passed |

`make test` runs the 7-category suite and then the parser regression suite, and exits non-zero if either fails.

## The 7-category suite

```sh
./simulator --test all            # or: unit, integration, validation, blackbox, whitebox, regression, system
make tests/test_suite && ./tests/test_suite all     # the same tests, as a separate binary
```

Each line of output is `[Category] TestName [ PASS ] details`, followed by a summary:

```
 TOTAL TESTS : 30 | PASSED : 30 | FAILED : 0 | SUCCESS RATE : 100%
```

The exit code is 1 if any test failed. The GUI's **Run All Test Categories** button runs the same command.

| Category | File | Tests |
|---|---|---|
| Unit (13) | `tests/unit/TestUnit.cpp` | Truth tables of AND, OR, NOT, XOR (every input row); MUX select; state rules of DFF_P, JKFF_P, TFF_P, DFF_N, JKFF_N, TFF_N; dead and unreachable state detection; parser error on a missing file |
| Integration (3) | `tests/integration/TestIntegration.cpp` | `Worklet1_2_3_FSM_Pipeline`: FSM → synthesizer → parser → simulator → assertions. A shift register netlist shifting on clock pulses. A combinational netlist evaluated in topological order |
| Validation (2) | `tests/validation/TestValidation.cpp` | The synthesized traffic light holds its state with `en = 0`. Red and green are never on together |
| Black box (2) | `tests/blackbox/TestBlackBox.cpp` | Equivalence partition: no clock edge, no state change. Boundary value: state changes exactly on the edge |
| White box (2) | `tests/whitebox/TestWhiteBox.cpp` | All six flip-flop types sample the newest input when it arrives with the active edge. Events drain in topological order |
| Regression (5) | `tests/regression/TestRegression.cpp` | Every row of the AND truth table against an independent table. Four FSM transitions against an independently written sequence. An unknown signal fails an assertion instead of reading 0. A bad bit index fails likewise. `CLOCK clk 10 3` edges at 0, 3, 10, 13, 20 |
| System (3) | `tests/system/TestSystem.cpp` | A full run writes VCD, ASCII and JSON. A wrong assertion is caught and reported. A missing testbench file gives a clear error |

The suite's orchestration is in `src/testbench/TestSuite.cpp`. The validation and system tests require the circuit to load and the run to succeed before they can pass, so they cannot pass by failing to start.

### Adding a test

1. Add it to the `Test<Category>.cpp` file of its category.
2. Build the circuit with a `slms::Testbench` (`loadCircuit`, `addStimulus`, `addAssertion`, `run`), or drive the engine directly.
3. Call `record("Category", "Name", passed, "what was checked")`.

The Makefile picks up every `tests/<category>/Test*.cpp`.

## Parser regression suite

```sh
make parser-test
```

A standalone program of 52 tests and 203 checks: every parser error message, every flip-flop type, buses, constants, topological priorities and cycle detection. It prints one `[ PASS ]` or `[ FAIL ]` line per test and exits 1 if any check fails.

## Example testbenches

```sh
make examples-test                                  # and_gate, traffic_light, vending_machine
bash examples/component_file_io/run_all_checks.sh   # every cell, the shift register, both FSMs
```

`run_all_checks.sh` runs each component's `testbench.tb` and requires at least one assertion and no failures, then runs each combinational `input.txt` through `--interactive`. Logs go to `output/component_file_io/`.

## Standalone programs

```sh
make standalone-build          # builds every program under build/standalone/
```

The original per-component test programs, each with its own `main()`. They are not part of `make test`. Most read `input.txt` from their own folder and print one line per input; where there is an `expected.txt`, the output should match it:

```sh
cd tests/unit/combinational/and
../../../../build/standalone/tests/unit/combinational/and/andt < input.txt | diff - expected.txt
```

Status on commit `b92a64b`:

| Programs | Status |
|---|---|
| `tests/unit/combinational/{and,andnot,mux,nand,nor,not,xor}`, `tests/unit/sequential/dffpt` | Output matches `expected.txt` |
| `tests/unit/combinational/or`, `ornot` | Output does not match: the program reads two values per line, but `input.txt` has four columns. The fixture is wrong, not the gate: OR is covered by the unit truth table and by validation |
| `tests/unit/synthesizer/analysist` | Matches its expected report when run from the project root (see [fsm-analysis.md](synthesizer/fsm-analysis.md#testing)) |
| `tests/unit/synthesizer/traffict` | Only checks that synthesis succeeds |
| `tests/netlists/*/*` | Run and exit 0, no expected output to compare |
| `tests/unit/parser/test_parser` | The parser regression suite above |
| `tests/integration/{ringcounter,simple_dffp_not,sipo_register}` | Not built by any Makefile target |

## Testing strategy

- **Bottom-up.** Gates and flip-flops are tested alone (unit), then wired into netlists (integration), then through the testbench and its output files (system).
- **Independent oracles.** Regression tests compare against tables written by hand, not against the simulator's own earlier output. The simulator validation compares against Icarus Verilog on the same netlist, and the synthesizer validation compares each netlist with its FSM table using code that shares nothing with the synthesizer.
- **Testing the tests.** The synthesizer validation damages netlists and tables, and `inject_bugs.py` puts real bugs into `Synthesizer.cpp`, to show that the checks can fail.
- **Negative tests.** Missing files, unknown signals, bad bit indices and wrong assertions must fail loudly.
- **What is not covered.** No line-coverage measurement is set up on `main`. The GUI is checked by hand and by calling its API, not by a browser test.

# Source code audit fixes (25 October 2026)

1. **Asymmetric clock duty cycles:** `src/testbench/Testbench.cpp` schedules falling edges at `k*period` and rising edges at `k*period + highDuration`, rather than iterating by highDuration. Regression test `AsymmetricClock_10_3` checks expected outputs at 0, 3, 10, 13, and 20.
2. **Invalid signals and bit positions:** read and input operations reject invalid references rather than silently defaulting LOW. `Testbench::run()` catches these exceptions, prints an error, and returns false. Regression tests cover nonexistent outputs and out-of-bounds output indices.
3. **Validation/system false positives:** the relevant tests require successful `loadCircuit()` and `run()` before recording a pass; the safety invariant is additionally required to hold.
4. **Regression oracles:** regression tests now check every row of the AND truth table against the truth-table definition and compare synthesized traffic-light FSM transitions at four successive enabled rising edges with a known independently specified sequence. Includes negative-test coverage and duty-cycle coverage.
5. **Standalone test programs:** `make standalone-build` is an additional opt-in build target for C++ files with their own `main()`. They are intentionally NOT silently counted in `make test`; some are interactive and require stdin, specific working directories, or other runtime setup. Use `build/standalone/<source path without .cpp>` to execute a built program manually, with its required input/setup. Compilation of a representative standalone gate test was verified. **Not every standalone test program was executed.**
6. **Exit code for scripted validation:** `./simulator --test ...` now returns 1 if the requested test category reports failures, 0 if it passes. `./tests/test_suite` remains the recommended dedicated test runner.

## Verified commands

```bash
make simulator tests/test_suite
./tests/test_suite all
./simulator --test regression
./simulator --test all
make standalone-build  # optional: compile standalone test programs; not automatically run
```

## Verification performed
- Main simulator and dedicated test-suite runner compiled.
- Updated 7-category suite passed **25/25** tests (previous baseline was 22).
- A representative standalone C++ gate test compiled with the optional target.
- Full standalone test execution and browser GUI end-to-end validation were not performed.

The traffic-light GUI and existing 7-folder test structure are unchanged.

## Follow-up improvements in this revision

- Testbench `INVARIANT` expressions are no longer hard-coded to traffic-light signal names. The parser supports generic two-state Boolean expressions using `!`, `&&`, `||`, parentheses, `== 0/1`, `!= 0/1`, and bare scalar signals.
- Added `examples/vending_machine/vending_machine.net` and `.tb` as a second FSM example. It exercises the generic testbench path through FSM synthesis, signal assertions, clock scheduling, and invariants.
- `./simulator --testbench <file.tb>` now returns a non-zero exit code when the testbench cannot load or an assertion/invariant fails.
- Added `make examples-test` to run the combinational AND, traffic-light FSM, and vending-machine FSM testbench examples.
- The empty DFF_N/TFF/JKFF source/header placeholders remain intentionally unregistered in the parser. They must be implemented and tested before claiming those variants are supported.

### Current verified results

- `make -j4`: successful.
- `./simulator --test all`: 25/25 tests passed.
- `./simulator --testbench examples/traffic_light/traffic_light.tb`: passed.
- `./simulator --testbench examples/vending_machine/vending_machine.tb`: passed.
- A deliberately invalid assertion was used to verify that testbench failure produces a non-zero process exit status.

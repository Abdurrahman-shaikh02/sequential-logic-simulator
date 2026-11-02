# Seven individually organized test categories

The original tests were split into their own category folders without changing their test assertions or the simulator menu.

| Category | Source file |
|---|---|
| Unit | `tests/unit/TestUnit.cpp` |
| Integration | `tests/integration/TestIntegration.cpp` |
| Validation | `tests/validation/TestValidation.cpp` |
| BlackBox | `tests/blackbox/TestBlackBox.cpp` |
| WhiteBox | `tests/whitebox/TestWhiteBox.cpp` |
| Regression | `tests/regression/TestRegression.cpp` |
| System | `tests/system/TestSystem.cpp` |

The common test reporting / orchestration remains in `src/testbench/TestSuite.cpp` and `include/testbench/TestSuite.hpp`. Each category keeps its original test logic and calls `record()` for pass/fail.

## Build and run

```bash
make -B simulator
./simulator --test all
./simulator --test unit
./simulator --test integration
./simulator --test validation
./simulator --test blackbox
./simulator --test whitebox
./simulator --test regression
./simulator --test system
```

Alternatively build the standalone runner with `make tests/test_suite`, then invoke `./tests/test_suite all` or replace `all` with one category.

Run commands from the project root. This is a code organization change, not additional test coverage.

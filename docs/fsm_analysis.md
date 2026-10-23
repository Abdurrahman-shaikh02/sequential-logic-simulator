# FSM analysis: dead and unreachable states

Two functions that check an FSM description (format: `docs/fsm.net`) directly from its transition table, without synthesizing or simulating it. They live next to the synthesizer.

| Function | Finds |
|---|---|
| `DeadStates::find` (`synthesizer/DeadStates.hpp`) | **Dead (trap) states:** once the FSM enters one, it can never leave. Every transition listed for the state goes back to itself, or the state has no transitions listed. |
| `UnreachableStates::find` (`synthesizer/UnreachableStates.hpp`) | **Unreachable states:** no sequence of inputs leads to them from the reset state. Found with a breadth-first search from the reset state. |

Both functions consider only the rows written in the table. An input combination with no row is ignored. (The synthesizer instead sends such combinations to the reset state.) For example, a state whose only listed row loops back to itself is reported as dead, even if other input combinations are missing.

## Usage

```cpp
#include "synthesizer/DeadStates.hpp"
#include "synthesizer/UnreachableStates.hpp"

std::vector<std::string> dead, unreachable;

// return only: fills the vector, prints nothing
DeadStates::find("docs/fsm_example/dead_unreachable_fsm.net", dead);

// return and print a report to std::cout
UnreachableStates::find("docs/fsm_example/dead_unreachable_fsm.net", unreachable, true);
```

```cpp
static bool find(const std::string &path, std::vector<std::string> &states, bool print = false);
```

- `states` is cleared, then filled with the states found, in the order they first appear in the table (the reset state counts as first). It is empty when there are none.
- `print` is optional and defaults to `false`. Pass `true` to also print a report.
- The function returns `false` when the FSM file can't be opened or is malformed: wrong number of fields or bits, invalid names, a missing `reset:` line, or two rows for the same state and inputs. The error is written to `std::cerr` as `file:line: message`, regardless of `print`.

Reports for `docs/fsm_example/dead_unreachable_fsm.net`:

```
dead states:
  T  (2/2 input combinations listed, all go back to T)
  V  (no transitions listed)
  H  (1/2 input combinations listed, all go back to H)
```

```
reset state: A
reachable:   A B T
unreachable: U V H
```

When there is nothing to report, they print `dead states: none` and `unreachable: none`. The comments in the example file explain why each state is or isn't reported.

## Testing

From the repo root:

```sh
make tests/unit/synthesizer/analysist
./tests/unit/synthesizer/analysist | diff - tests/unit/synthesizer/analysis_expected.txt
```

The test calls both functions in print mode and then in return mode, and prints the returned states. No output from `diff` means the test passed.

# Output files

Every run writes the same four files, plus a report on standard output. `--testbench` (and the GUI, which uses it) writes them as `output/<name>`. `--interactive` writes them as `output/interactive_<name>`. Each run overwrites the previous one.

| `--testbench` | `--interactive` | Contents |
|---|---|---|
| `results.txt` | `interactive_results.txt` | Table of every signal at every recorded time |
| `waveform.txt` | `interactive_waveform.txt` | ASCII timing diagram |
| `waveform.json` | `interactive_waveform.json` | The same data for the GUI |
| `simulation.vcd` | `interactive_simulation.vcd` | IEEE 1364 Value Change Dump, for GTKWave and other viewers |

All four are written by `WaveformGenerator` (`src/waveform/WaveformGenerator.cpp`) from the same record: the value of every primary input and output bit at every event time ([testbench.md](testbench.md#time)). Values between event times are not recorded, because nothing can change there.

## Signals

- Every primary input, then every primary output. The order within each group is the order the parser's hash map gives, not alphabetical, but it is the same in all four files.
- A bus is written one bit per signal, `q[0]`, `q[1]`, ..., LSB first.
- With a `CLOCK`, the clock is one of the inputs.

## Results table

```
Time    	rst	clk	q[0]	q[1]	q[2]	q[3]
0       	1	0	0	0	0	0
10      	1	1	0	0	0	0
20      	0	0	0	0	0	0
30      	0	1	1	0	0	0
```

Tab separated. One row per event time, one column per signal bit. Each row holds the values after that time has settled. Validation and benchmarks read this file.

## ASCII waveform

```
Time(ns)     : 0   10  20  30  40  50  60  70
             : +---+---+---+---+---+---+---+---+
rst          : ^-------\_______________________
clk          : ____/---\___/---\___/---\___/---
q[0]         : ____________/-------\_______/---
q[1]         : ____________________/-----------
```

`-` is 1, `_` is 0, `/` a rising edge, `\` a falling edge. Each event time takes one column of 4 characters, however far apart the times are, so the horizontal scale is not linear in time. The report on standard output starts with the same diagram, and `--interactive` also prints its first 30 lines.

## JSON

```json
{
  "module": "top",
  "signals": ["clk", "reset", "en", "G", "Y", "R"],
  "times": [0, 1, 3, 8, 10, 13],
  "data": {
    "clk": [0, 0, 1, 1, 0, 1],
    ...
  }
}
```

`data[s][i]` is the value of signal `s` at `times[i]`. The GUI draws its waveform viewer from this file.

## VCD

```
$timescale 1ns $end
$scope module top $end
$var wire 1 ! rst $end
$var wire 1 " clk $end
$var wire 1 # q[0] $end
...
#0
$dumpvars
```

One 1-bit `wire` per signal bit, in module `top`, with one time unit written as `1ns`. Open it with `gtkwave output/simulation.vcd`.

## The report

The report on standard output ends with the assertion summary:

```
                      SLMS ASSERTION ENGINE REPORT
======================================================================
 Total Evaluated : 4
 Passed          : 4 (100%)
 Failed          : 0
 Overall Status  : [ PASSED ]
```

When something fails, `Overall Status` is `[ FAILED ]` and each failure is listed:

```
VIOLATIONS DETECTED:
 [!] Time 0     ns | SignalValue:y        : Expected LOW(0) but got HIGH(1) (wrong on purpose)
```

Each invariant check counts once per event time. With no assertions the report shows `Total Evaluated : 0` and `[ PASSED ]`, even though nothing was checked. `--testbench` then prints the output file names and `Final Testbench Result : PASSED` or `FAILED`.

## Truth table (`--truthtable`)

Written to the file named on the command line:

```
a b | y
0 0 | 0
0 1 | 0
1 0 | 0
1 1 | 1
```

Input bits, `|`, output bits, then one row per input combination in binary counting order.

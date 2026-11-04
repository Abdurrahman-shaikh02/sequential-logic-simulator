# Getting started

## Requirements

| Tool | Needed for |
|---|---|
| `g++` with C++17, `make` | Building the simulator and the tests |
| Linux | Tested on Linux. The GUI launcher runs `python3`, and the scripts expect a POSIX shell |
| Python 3 | The web GUI (`gui/server.py`, standard library only), `tools/run_file_testbench.py`, validation and benchmarks |
| [Yosys](https://github.com/YosysHQ/yosys) | Turning Verilog into a netlist. Validation and benchmarks |
| [Icarus Verilog](https://github.com/steveicarus/iverilog) (`iverilog`, `vvp`) | Validation and benchmarks |
| `matplotlib` | Benchmark plots only |

## Build

```sh
make            # builds ./simulator (-O2)
make clean      # removes build/, output/, validation/icarus/out and the binaries
```

Object files go to `build/`. The Makefile picks up every `.cpp` under `src/` on its own.

## Run the tests

```sh
./simulator --test all       # the 7 test categories, 30 tests
make examples-test           # the AND gate, traffic-light and vending-machine testbenches
```

See [testing.md](testing.md) for every suite.

## Ways to give a circuit its inputs

| Way | Command | Good for |
|---|---|---|
| Testbench file | `./simulator --testbench file.tb` | Clocked circuits and FSMs, assertions, invariants. See [formats/testbench.md](formats/testbench.md) |
| Input vectors | `./simulator --interactive`, then a vectors file | One row of 0/1 per step, one column per input bit, optional expected outputs. See [formats/vectors.md](formats/vectors.md) |
| By hand | `./simulator --interactive`, then press Enter | Typing each input value in the terminal |
| Browser | `./simulator --gui 8080` | Every example circuit, the live traffic light, waveform viewer. See [gui.md](gui.md) |

## A first simulation: the traffic-light FSM

```sh
./simulator --testbench examples/traffic_light/traffic_light.tb
```

The testbench names an FSM table (`.net`). The simulator synthesizes it into gates and flip-flops, parses the result, drives the clock and the inputs, and checks 31 assertions. It prints an ASCII waveform and an assertion report, ending with:

```
[+] Final Testbench Result  : PASSED
```

It also writes `output/results.txt`, `output/waveform.txt`, `output/waveform.json` and `output/simulation.vcd`. Open the `.vcd` in a waveform viewer such as GTKWave. The formats are in [formats/output.md](formats/output.md).

## From Verilog to a trace

### 1. Write the circuit

```verilog
// counter.v
module counter (input clk, input rst, output reg [3:0] q);
    always @(posedge clk)
        if (rst) q <= 0;
        else     q <= q + 1;
endmodule
```

### 2. Synthesize it with Yosys

The simulator reads the Yosys `write_table` format, using only the cells listed in [formats/netlist.md](formats/netlist.md). The two flows in `validation/flows/` produce exactly those cells:

```sh
yosys -q -p "read_verilog counter.v; script validation/flows/abc.ys; write_table counter.table"
```

`abc.ys` uses every two-input gate type. `techmap.ys` keeps the netlist closer to the source and uses only AND, OR, NOT, XOR, MUX and D flip-flops. Both flows tie undefined values and flip-flop initial values to 0, which is how our simulator starts every node.

### 3. Write an input-vectors file

`q` is a bus, and a `.tb` file cannot set one bit of an input bus, so input vectors are the simplest way in for Yosys netlists. One row per step, one 0/1 value per input bit, in the order `--interactive` lists the inputs (sorted by name, then by bit):

```
# clk rst
0 1
1 1
0 0
1 0
0 0
1 0
0 0
1 0
```

Change data inputs only on rows where the clock does not change. See [formats/vectors.md](formats/vectors.md#clocks-and-data) for why.

### 4. Simulate

```sh
./simulator --interactive
# Circuit path: counter.table
# Input vectors file (press Enter for manual entry): counter.vec
```

The answers can also come from a pipe: `printf 'counter.table\ncounter.vec\n' | ./simulator --interactive`.

Rows are applied 10 time units apart. `output/interactive_results.txt` then holds:

```
Time    	rst	clk	q[0]	q[1]	q[2]	q[3]
0       	1	0	0	0	0	0
10      	1	1	0	0	0	0
20      	0	0	0	0	0	0
30      	0	1	1	0	0	0
40      	0	0	1	0	0	0
50      	0	1	0	1	0	0
60      	0	0	0	1	0	0
70      	0	1	1	1	0	0
```

Each row holds the values after that input row has settled. Bus bits are listed LSB first, so at time 70 `q` is 3.

## From an FSM table to a simulation

```sh
./simulator --fsm docs/fsm_example/traffic_light_fsm.net
```

This reports dead and unreachable states, then writes the synthesized netlist next to the input as `traffic_light_fsm.table`. The synthesizer adds `clk` and `reset` inputs, and reset is synchronous. A `.tb` file can name the `.net` file directly with `FSM`, and it is synthesized on load. The FSM format is in [formats/fsm.md](formats/fsm.md).

## Truth table of a combinational circuit

```sh
./simulator --truthtable tests/netlists/combinational/and/netlist.table and_truth.txt
# a b | y
# 0 0 | 0
# 0 1 | 0
# 1 0 | 0
# 1 1 | 1
```

This works only for circuits with no flip-flops and at most 20 input bits.

## Next

- [cli.md](cli.md) for every option.
- [architecture/overview.md](architecture/overview.md) for how it works inside.

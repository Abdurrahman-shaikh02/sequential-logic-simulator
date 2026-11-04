# Getting started

## Requirements

| Tool | Needed for |
|---|---|
| `g++` with C++17, `make` | Building the simulator and the tests |
| Linux | `main.cpp` uses `getopt_long` and `getrusage`, and the test runner uses `fork` |
| [Yosys](https://github.com/YosysHQ/yosys) | Turning Verilog into a netlist; validation and benchmarks |
| [Icarus Verilog](https://github.com/steveicarus/iverilog) (`iverilog`, `vvp`) | Validation and benchmarks |
| Python 3 | Validation and benchmarks |
| `matplotlib` | Benchmark plots only |

## Build

```sh
make            # optimized build: ./simulator
make debug      # -O0 build: ./simulator-debug
make asan       # AddressSanitizer + UBSan build: ./simulator-asan
make clean      # removes build/, the binaries and generated validation/benchmark files
```

Object files go to `build/<mode>/`, and the library is linked once into `build/<mode>/libhdlsim.a`. Header dependencies are tracked, so after an edit only the affected files are rebuilt.

## Run the tests

```sh
make test               # every test, debug build
make test TESTS=dff     # only tests whose name contains "dff"
make test-asan          # every test under the sanitizers
```

See [testing.md](testing.md) for more.

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

### 3. Find the port names

```sh
./simulator ports counter.table
# // inputs: clk rst
# // outputs: q[3:0]
```

The first line is the header your stimulus file needs.

### 4. Write a stimulus

One row per time step, with one field per header entry. Buses are written MSB first.

```
// inputs: clk rst
0 1
1 1
0 0
1 0
0 0
1 0
0 0
1 0
```

Change data inputs only while the clock stays low, never on the same row as a clock edge. See [formats/stimulus.md](formats/stimulus.md) for why.

### 5. Simulate

```sh
./simulator sim counter.table -s counter.stim
# // outputs: q[3:0]
# 0000
# 0000
# 0000
# 0001
# 0001
# 0010
# 0010
# 0011
```

Each output row holds the outputs after that input row has settled. Use `-o FILE` to write the trace to a file, and `-p` for timing and work statistics.

## From an FSM table to a trace

```sh
./simulator synth docs/fsm_example/traffic_light_fsm.net -o traffic.table
./simulator ports traffic.table
# // inputs: clk en reset
# // outputs: G R Y
```

The synthesizer adds `clk` and `reset` inputs. Reset is synchronous, so hold `reset` high for a clock edge first, then drive the FSM's inputs. The FSM format is in [formats/fsm.md](formats/fsm.md).

## Truth table of a combinational circuit

```sh
./simulator truth tests/fixtures/netlists/mux/netlist.table
# a[3] a[2] a[1] a[0] b[3] b[2] b[1] b[0] sel | y[3] y[2] y[1] y[0]
# 0 0 0 0 0 0 0 0 0 | 0 0 0 0
# ...
```

This works only for circuits with no flip-flops and at most 20 input bits.

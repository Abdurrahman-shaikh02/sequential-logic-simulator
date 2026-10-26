# Generic GUI simulation

From the project root, build and launch the GUI:

```bash
make
./simulator --gui 8080
```

Open `http://localhost:8080`.

## Combinational netlist

Select **Combinational netlist (.table)** and enter:

- Netlist: `tests/netlists/combinational/and/netlist.table`
- Maximum time: `40`
- Input events:

```text
0: a=0 b=0
10: a=1 b=0
20: a=0 b=1
30: a=1 b=1
```

Click **Run generic simulation**. The signal names must match the selected netlist. The result includes the C++ simulator report and waveform signals/samples.

## FSM netlist

Select **FSM (.net)**, then use `examples/vending_machine/vending_machine.net` as a starting example. FSM simulations may specify a clock period and timed inputs, including reset. Example input events:

```text
0: reset=1 coin5=0 coin10=0
5: reset=0
8: coin5=1 coin10=0
16: coin5=0 coin10=0
28: coin5=0 coin10=1
36: coin5=0 coin10=0
```

The vending panel separately provides Insert 5, Insert 10, and Reset session controls. It replays the selected sequence through the same C++ testbench engine.

Input event syntax is `time: signal=0 signal2=1` or `@time signal=0 signal2=1`. Supported signals and values depend on the selected circuit; scalar inputs use 0/1.

sequential-logic-simulator/
│
├── README.md
├── LICENSE
├── Makefile
├── CMakeLists.txt
│
├── docs/
│   ├── architecture/
│   │   ├── class_diagram.md
│   │   ├── simulation_model.md
│   │   ├── event_scheduling.md
│   │   └── design_decisions.md
│   │
│   ├── netlist/
│   │   └── netlist_format.md
│   │
│   └── waveform/
│       └── waveform_format.md
│
├── include/
│   │
│   ├── core/
│   │   ├── Port.hpp
│   │   ├── Node.hpp
│   │   ├── List.hpp
│   │   └── ListNode.hpp
│   │
│   ├── combinational/
│   │   ├── CombinationalNode.hpp
│   │   ├── NAND.hpp
│   │   ├── AND.hpp
│   │   ├── MUX.hpp
│   │   ├── NOT.hpp
│   │   ├── NOR.hpp
│   │   ├── OR.hpp
│   │   ├── XOR.hpp
│   │   ├── ANDNOT.hpp
│   │   └── ORNOT.hpp
│   │
│   ├── sequential/
│   │   ├── SequentialNode.hpp
│   │   ├── DFF_P.hpp
│   │   ├── DFF_N.hpp
│   │   ├── TFF_P.hpp
│   │   ├── TFF_N.hpp
│   │   ├── JKFF_P.hpp
│   │   └── JKFF_N.hpp
│   │
│   ├── primary/
│   │   ├── PrimaryNode.hpp
│   │   ├── InputNode.hpp
│   │   └── OutputNode.hpp
│   │
│   ├── operations/
│   │   ├── OperationType.hpp
│   │   ├── Operation.hpp
│   │   ├── EvaluateOperation.hpp
│   │   ├── SetPortOperation.hpp
│   │   ├── ChangeStateOperation.hpp
│   │   └── CommitOperation.hpp
│   │
│   ├── simulation/
│   │   ├── OperationQueue.hpp
│   │   └── Simulator.hpp
│   │
│   ├── parser/
│   │   └── Parser.hpp
│   │
│   └── synthesizer/
│       └── Synthesizer.hpp
│
├── src/
│   │
│   ├── core/
│   │   ├── Node.cpp
│   │   ├── List.cpp
│   │   └── ListNode.cpp
│   │
│   ├── combinational/
│   │   ├── NAND.cpp
│   │   ├── AND.cpp
│   │   ├── MUX.cpp
│   │   ├── NOT.cpp
│   │   ├── NOR.cpp
│   │   ├── OR.cpp
│   │   ├── XOR.cpp
│   │   ├── ANDNOT.cpp
│   │   └── ORNOT.cpp
│   │
│   ├── sequential/
│   │   ├── DFF_P.cpp
│   │   ├── DFF_N.cpp
│   │   ├── TFF_P.cpp
│   │   ├── TFF_N.cpp
│   │   ├── JKFF_P.cpp
│   │   └── JKFF_N.cpp
│   │
│   ├── primary/
│   │   ├── InputNode.cpp
│   │   └── OutputNode.cpp
│   │
│   ├── operations/
│   │   ├── Operation.cpp
│   │   ├── EvaluateOperation.cpp
│   │   ├── SetPortOperation.cpp
│   │   ├── ChangeStateOperation.cpp
│   │   └── CommitOperation.cpp
│   │
│   ├── simulation/
│   │   ├── OperationQueue.cpp
│   │   └── Simulator.cpp
│   │
│   ├── parser/
│   │   └── Parser.cpp
│   │
│   └── synthesizer/
│       └── Synthesizer.cpp
│
├── tests/
│   │
│   ├── unit/
│   │   ├── combinational/
│   │   │   ├── test_and.cpp
│   │   │   ├── test_nand.cpp
│   │   │   ├── test_mux.cpp
│   │   │   └── ...
│   │   │
│   │   ├── sequential/
│   │   │   ├── test_dff.cpp
│   │   │   ├── test_tff.cpp
│   │   │   └── test_jkff.cpp
│   │   │
│   │   ├── operations/
│   │   │   └── test_operations.cpp
│   │   │
│   │   └── queue/
│   │       └── test_operation_queue.cpp
│   │
│   ├── integration/
│   │   ├── test_adder.cpp
│   │   ├── test_counter.cpp
│   │   ├── test_register.cpp
│   │   ├── test_fsm.cpp
│   │   └── ...
│   │
│   ├── netlists/
│   │   ├── combinational/
│   │   ├── sequential/
│   │   └── fsm/
│   │
│   └── expected/
│       └── ...
│
├── examples/
│   ├── and_gate/
│   ├── mux/
│   ├── counter/
│   ├── register/
│   └── traffic_light/
│
├── tools/
│   ├── netlist_converter/
│   └── waveform_viewer/
│
└── bin/

## Generic simulation in the Web GUI

Start the simulator and GUI from the project root:

```bash
make
./simulator --gui 8080
```

Open `http://localhost:8080`. The **Generic Netlist Simulation** panel accepts a project-relative netlist path, simulation end time, optional FSM clock period, and timed input changes. Use `.table` files for combinational circuits and `.net` files for FSMs. Input events can be entered as `0: a=0 b=1` or `@10 a=1`; signal names must match the selected circuit. The simulation runs through the C++ testbench engine and returns waveform data.

Example combinational netlist:

- Path: `tests/netlists/combinational/and/netlist.table`
- Events:

```text
0: a=0 b=0
10: a=1 b=0
20: a=0 b=1
30: a=1 b=1
```

The **Vending Machine FSM** panel runs the bundled `examples/vending_machine/vending_machine.net` circuit. Use the coin buttons to replay a coin sequence through the C++ simulator. The GUI panels are served from the local `gui/` directory, so use a hard refresh (`Ctrl+Shift+R`) after replacing files.
## Sequential flip-flop support (updated)

The parser now registers these sequential primitives: `$_DFF_P_`, `$_DFF_N_`, `$_JKFF_P_`, `$_JKFF_N_`, `$_TFF_P_`, and `$_TFF_N_`. Their implementations are in `src/sequential/`, with matching public headers in `include/sequential/`. Port names follow the netlist convention: `C` for clock, `Q` for output, and `D`, `J`, `K`, or `T` for data/control.

Run the main functional suite and the parser regression suite together with `make test`. The main suite covers 30 tests; the parser suite covers 51 cases/check groups, including recognition of all newly registered flip-flop types. `make examples-test` runs the AND-gate, traffic-light, and vending-machine testbench examples.


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

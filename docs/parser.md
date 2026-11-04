# Parser

`Parser::parse` reads a netlist (format: [formats/netlist.md](formats/netlist.md)) and builds the circuit graph the simulator runs. The code is in `src/parser/Parser.cpp`.

```cpp
int Parser::parse(const std::string &path,
                  std::vector<std::unique_ptr<Node>> &nodes,
                  std::unordered_map<std::string, std::vector<Node*>> &primaryInputs,
                  std::unordered_map<std::string, std::vector<Node*>> &primaryOutputs);
```

It returns 0 on success. On error it writes one message to `std::cerr` and returns -1. Normally it is called as `Simulator::parse(path)`, which fills the simulator's own members.

On success:

- `nodes` holds every node, sorted in topological order, with `priority` equal to its index.
- `primaryInputs` and `primaryOutputs` map each port name to its bit nodes, **LSB first** (`bits[0]` is bit 0).
- Every node's connections have been set up.

## Steps

### 1. Read lines

Each line is split into six columns after removing any `#` comment. Columns past the sixth are joined with spaces into the wire name, so `\q [0]` stays one name. Then:

- **Primary ports** (type `-`) are recorded for step 2.
- **Cells**: the type and port are looked up in two tables, `compTypeTable` (Yosys type → port list and a function that creates the node) and `portNameTable` (netlist port name → `Port`). The node is created the first time its cell name appears.
- For each port the parser records, on a temporary `Wire`, its **driver** (an `out` port) or one more **consumer** (an `in` port, with its `Port`).
- A constant `1'0`, `1'1` or `1'x` on an input creates an `InputNode` with that value as the wire's driver.

### 2. Create primary nodes

Each primary port becomes one `InputNode` or `OutputNode` per bit. An input node becomes its wire's driver, and an output node becomes one of its wire's consumers. Bus bits come from a ranged name (`\q [3:0]`) or from every wire named `<base> [i]`, sorted by index.

### 3. Validate

- Every cell has every port of its type.
- Every wire has exactly one driver. A wire with no consumers only causes a warning.

The full list of errors is in [formats/netlist.md](formats/netlist.md#what-the-parser-rejects).

### 4. Connect

For each wire, `driver->addConnection(consumer, port)` is called for each consumer. Connections hold the node and the port, so the wires themselves are not kept after parsing.

### 5. Topological order

The order is computed with Kahn's algorithm:

1. Count each node's incoming edges, **leaving out edges into a flip-flop's data inputs** (`D`, `T`, `J`, `K`). Edges into a flip-flop's clock are counted.
2. Start with the nodes that have no incoming edges: primary inputs, constants and flip-flops with a constant clock. Repeatedly take one node and remove its outgoing edges.
3. If nodes are left at the end, they are on a cycle, and the netlist is rejected with `cycle detected in circuit`.

Because data inputs are left out, a feedback loop through a flip-flop (a counter, a state machine) is not a cycle. A loop made only of gates is. Because clock inputs are kept, a flip-flop always comes after the logic that drives its clock, which is what [simulation.md](architecture/simulation.md#derived-clocks) needs for derived clocks.

`nodes` is then sorted into this order, and each node's `priority` is set to its index. The simulator's topological queue commits nodes in priority order. Parsing fails if there are more nodes than an `int` can index.

An empty file parses as an empty circuit with no error.

## Cost

The whole parse is linear in the number of lines, apart from:

- hash map lookups for cells and wires,
- bus ports given without a range: each one scans every wire with a regex to find its bits, so many such ports cost O(ports × wires).

## Tests

`make parser-test` builds and runs `tests/unit/parser/test_parser.cpp`, a standalone suite of 52 tests and 203 checks covering every error message, every flip-flop type, buses, constants, priorities and cycle detection; see [testing.md](testing.md#parser-regression-suite). The unit test `Parser_Negative_MissingFile` and the integration test `Combinational_Netlist_Evaluation` in the 7-category suite also exercise the parser.

# Netlist Parser

Reads a netlist file, builds the circuit graph, validates it, and orders the nodes topologically. Priorities are assignmed to nodes starting from 0, and going up to the maximum integer value. A lower number means a higher priority.

```cpp
int Parser::parse(const std::string &path, std::vector<std::unique_ptr<Node>> &nodes, std::unordered_map<std::string, std::vector<Node*>> &primaryInputs, std::unordered_map<std::string, std::vector<Node*>> &primaryOutputs);
```

## Input

A text file identified by `path`, with one connection per line and six whitespace-separated columns:

```
module  name  type  port  direction  wire
```

| Column | Meaning |
|---|---|
| `module` | Top-level module name. Must be the same on every line. |
| `name` | Unique component name. |
| `type` | Gate type e.g. `$_AND_` or `-` for a primary input/output. |
| `port` | Port on the gate e.g. `A`, `Y`, or `-` for a primary node. |
| `direction` | `in` / `out` for gate ports, `pi` / `po` for primary nodes. |
| `wire` | Wire name. For primary nodes, the base name or a range. |

- Everything after `#` in a line is a comment. Blank lines are ignored.
- A wire name may contain spaces. Any tokens after the fifth are joined into the wire name.
- Multibit wires may be defined either as `name` or `name [high:low]` only. If a primary input connects to a wire `name`, later instances of `name [index]` are assumed to be bits of the same input node. A bus cannot be specified as a list of discrete or constant valued wires.
- A constant can be used as an input wire specified by `1'0`, `1'1` or `1'x`, where `x` is treated as 0. One `InputNode` per type is created for it and set at parse time.

### Supported gates

| Type | Ports |
|---|---|
| `$_NOT_` | `A`, `Y` |
| `$_AND_`, `$_OR_`, `$_NAND_`, `$_NOR_`, `$_XOR_`, `$_ANDNOT_`, `$_ORNOT_` | `A`, `B`, `Y` |
| `$_MUX_` | `A`, `B`, `S`, `Y` |
| `$_DFF_P_`, `$_DFF_N`, `$_JKFF_P_`, `$_JKFF_N_`, `$_TFF_P_`, `$_TFF_N_` | `D`, `C`, `Q` |

`Y` and `Q` are the output ports. A type is treated as sequential if it has a `C` port.

## Output

**Return value:** `0` on success, `-1` on error with the message written to `std::cerr`.

**`nodes`:** every node in the circuit including gates, primary nodes, and constant input nodes, stored in topological order. Each node's `priority` is set to its index in that order.

**`primaryInputs` / `primaryOutputs`:** map from primary node name to its nodes.
- A single-bit port has one entry.
- A multibit port has one entry per bit, in ascending index order.
- Constant input nodes are not included.

**Graph:** for every wire, the driver node is connected to each consumer node on the consumer's port via `addConnection`.

On error, `nodes` and the primary maps may be partially filled, so the caller should clear them before parsing again.

## Validations

### Netlist File
- The file can be opened.
- Every non-empty line has all 6 columns.
- All lines name the same module.

### Primary Nodes
- The instance name is unique.
- `direction` is `pi` or `po`.
- `port` is `-`.
- For a ranged wire `name [high:low]`, every wire in the range exists in the netlist.
- A primary input's wire is not already driven by something else.

### Gates
- The gate type is known.
- The port name is known and exists on that gate type.
- An instance name is not reused with a different type.
- A port is not defined twice on the same instance.
- Direction is `in` or `out`.
- Direction matches the port, i.e. `out` only on `Y` or `Q`.
- An output does not drive a constant wire.
- A wire has only one driver.

### Whole Circuit
- Every gate has all of its ports connected.
- Every wire has a driver (error displayed otherwise).
- Every wire has at least one consumer (warning only, parsing continues).
- No purely combinational cycles.

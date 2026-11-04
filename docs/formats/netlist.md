# Netlist format

The simulator reads the format Yosys writes with `write_table`. The synthesizer writes it too. Files usually end in `.table`.

## Lines

Each line describes one port of one cell, in six columns separated by whitespace:

```
module   cell                 type      port  direction  wire
counter  clk                  -         -     pi         \clk
counter  q                    -         -     po         \q
counter  $abc$164$...$165     $_AND_    A     in         \q [0]
counter  $abc$164$...$165     $_AND_    B     in         \q [1]
counter  $abc$164$...$165     $_AND_    Y     out        $abc$164$new_n9
```

| Column | Meaning |
|---|---|
| module | Name of the top module. It must be the same on every line, so flatten the design first. |
| cell | Instance name. All lines with the same name belong to one cell. |
| type | Cell type from the table below, or `-` for a primary input or output. |
| port | Port of the cell, or `-` for a primary input or output. |
| direction | `in` or `out` for a cell port, `pi` or `po` for a primary input or output. |
| wire | Name of the wire on that port. It may contain spaces, as in `\q [0]`: everything from the sixth column to the end of the line is the wire name. |

`#` starts a comment, and blank lines are ignored.

## Supported cells

| Type | Ports | Function |
|---|---|---|
| `$_NOT_` | A, Y | Y = ¬A |
| `$_AND_` | A, B, Y | Y = A · B |
| `$_OR_` | A, B, Y | Y = A + B |
| `$_NAND_` | A, B, Y | Y = ¬(A · B) |
| `$_NOR_` | A, B, Y | Y = ¬(A + B) |
| `$_XOR_` | A, B, Y | Y = A ⊕ B |
| `$_ANDNOT_` | A, B, Y | Y = A · ¬B |
| `$_ORNOT_` | A, B, Y | Y = A + ¬B |
| `$_MUX_` | A, B, S, Y | Y = S ? B : A |
| `$_DFF_P_` | D, C, Q | Q ← D on a rising edge of C |
| `$_DFF_N_` | D, C, Q | Q ← D on a falling edge of C |
| `$_TFF_P_` | T, C, Q | Q toggles on a rising edge of C when T = 1 |
| `$_TFF_N_` | T, C, Q | Q toggles on a falling edge of C when T = 1 |
| `$_JKFF_P_` | J, K, C, Q | On a rising edge of C: hold (00), reset (01), set (10), toggle (11) |
| `$_JKFF_N_` | J, K, C, Q | The same on a falling edge |

Yosys has no T or JK flip-flop cells, so `$_TFF_*` and `$_JKFF_*` appear only in netlists written by hand or by another tool. Every flip-flop powers up with Q = 0. There are no set, reset or enable pins, so Yosys must turn those into logic around a plain flip-flop. The flows in `validation/flows/` do this.

`Y` and `Q` are output ports and must have direction `out`. Every other port must be `in`.

## Primary inputs and outputs

A line with type `-` declares a primary port. Its cell column is the port name the stimulus and trace use, and its wire column is the wire it connects to.

```
counter  clk  -  -  pi  \clk
counter  q    -  -  po  \q
```

A port is a bus when its wire is a base name and the netlist has indexed wires `\q [0]`, `\q [1]`, and so on. The bits are taken from every wire named `<base> [i]`. A port may also give its range directly, as in `\q [3:0]`. Every bit from 3 down to 0 must then exist.

## Constants

An input port may connect to `1'0` or `1'1`. The parser creates one driver node per constant value, which every port using that constant shares. A cell output cannot drive a constant.

`1'x` is accepted and treated as 0. End the Yosys script with `setundef -zero` to avoid it.

Constants are recognized only on cell input ports. A primary output tied directly to a constant (`y  -  -  po  2'01`), and multi-bit constants in general, are rejected (see [limitations](../limitations.md)).

## What the parser rejects

The parser stops with an error message and the command exits with code 2 when:

- a line does not have six columns, or two lines name different modules
- a cell type, port name or direction is unknown, or a port does not exist on its cell type
- a cell appears with two different types, has a port listed twice, or is missing a port
- an output port is not `out`, or an input port is not `in`
- a primary port has a port name other than `-`, or a direction other than `pi`/`po`, or a name used twice
- a wire has no driver, or more than one
- a ranged port is missing one of its bits
- the combinational logic has a cycle. Loops through a flip-flop's data input are fine, because the flip-flop breaks them. A loop through a flip-flop's **clock** input, such as a flip-flop clocked by logic fed from its own output, is a cycle.

A wire that nothing reads causes only a warning. Use `-q` to hide warnings.

How the parser builds the graph is described in [parser.md](../parser.md).

## Producing a netlist with Yosys

```sh
yosys -q -p "read_verilog design.v; script validation/flows/abc.ys; write_table design.table"
```

The design must be flattened into one module and mapped to the cells above, with set, reset and enable logic made explicit. `validation/flows/abc.ys` and `validation/flows/techmap.ys` are tested scripts that do this. [getting-started.md](../getting-started.md) shows the whole flow.

A bus output must come from a single vector in the Verilog. When it is built from separate signals, as in `assign q = {d, c, b, a};`, Yosys writes the port as `{ \d \c \b \a }`, which the parser does not read. The error is `wire { \d \c \b \a } has no driver`.

// ============================================================
// TEST 01: 4-bit Boolean Logic
// ============================================================
// Purpose:
//   Test vector-width handling for the basic Boolean primitives.
//
//   Yosys should decompose each 4-bit operation into four
//   1-bit primitive cells.
//
// Expected cells:
//
//   AND     -> 4 x $_AND_
//   NAND    -> 4 x $_NAND_
//   OR      -> 4 x $_OR_
//   NOR     -> 4 x $_NOR_
//   XOR     -> 4 x $_XOR_
//   NOT     -> 4 x $_NOT_
//   ANDNOT  -> 4 x $_ANDNOT_
//   ORNOT   -> 4 x $_ORNOT_
//
// BUF is included as well, although Yosys may optimize/eliminate
// a direct buffer depending on the synthesis flow.
//
// We deliberately use different input combinations so that the
// operations are independent and easy to inspect in the netlist.
// ============================================================

module test_boolean_logic (
    input  [3:0] a,
    input  [3:0] b,

    output [3:0] y_and,
    output [3:0] y_nand,
    output [3:0] y_or,
    output [3:0] y_nor,
    output [3:0] y_xor,
    output [3:0] y_not,
    output [3:0] y_andnot,
    output [3:0] y_ornot,
    output [3:0] y_buf
);

    assign y_and    = a & b;
    assign y_nand   = ~(a & b);

    assign y_or     = a | b;
    assign y_nor    = ~(a | b);

    assign y_xor    = a ^ b;
    assign y_not    = ~a;

    // A & ~B
    assign y_andnot = a & ~b;

    // A | ~B
    assign y_ornot  = a | ~b;

    assign y_buf    = a;

endmodule

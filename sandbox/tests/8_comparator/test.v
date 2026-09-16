// ============================================================
// TEST 08: 4-bit Comparator
// ============================================================
// Checks:
//   eq = 1 when a == b
//   gt = 1 when a >  b
//   lt = 1 when a <  b
//
// Important:
//   We intentionally use HDL comparison operators rather than
//   manually writing Boolean equations.
//
//   This tests whether our Yosys synthesis flow:
//       comparison operator
//            ↓
//       techmap
//            ↓
//       supported primitive cells
//
// Inputs are 4-bit vectors.
// Outputs are scalar flags.
// ============================================================

module test_comparator (
    input  [3:0] a,
    input  [3:0] b,

    output eq,
    output gt,
    output lt
);

    assign eq = (a == b);
    assign gt = (a > b);
    assign lt = (a < b);

endmodule

#!/usr/bin/env python3
"""Writes every benchmark circuit and stimulus to generated/.

    benchmark/simulation/generate.py            everything
    benchmark/simulation/generate.py --quick    small sizes, to try the scripts

Random and mixed circuits are written directly as a .table and a .v, because
Yosys does not finish random logic from about 16k cells up. Multipliers go
through Yosys once, giving one netlist in both formats. Fixed seeds make every
file identical on each run.
"""
import argparse
import csv
import random
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from measure import REPO, measure

GENERATED = HERE / "generated"
MULTIPLIER = HERE / "circuits" / "multiplier.v"
FLOW = REPO / "validation" / "flows" / "abc.ys"

SIZES = (1000, 4000, 16000, 64000, 256000)
WIDTHS = (8, 16, 32, 64)
ACTIVITIES = (0.5, 0.02)    # chance of each input bit flipping per row
ROWS = 900
DEPTH = 32

SWEEP_SIZE = 16000          # stimulus length and depth are varied at this size
ROW_SWEEP = (9000, 90000)
DEPTH_SWEEP = (8, 128)

BUS_WIDTH = 64
FLOP_LAYER = 9              # mixed circuits: every 9th layer is flip-flops
EXPRESSIONS = {
    "AND": "{a} & {b}",
    "OR": "{a} | {b}",
    "NAND": "~({a} & {b})",
    "NOR": "~({a} | {b})",
    "XOR": "{a} ^ {b}",
    "ANDNOT": "{a} & ~{b}",
    "ORNOT": "{a} | ~{b}",
    "NOT": "~{a}",
    "MUX": "{s} ? {b} : {a}",
}
GATES = list(EXPRESSIONS)


def verilog_wire(wire):
    return wire.replace("\\", "").replace(" [", "[")


def write_layered(path, cells, depth, flops):
    """Layers of random gates using every gate type, each fed by the layer before.
    Every wire in a layer drives the next one, so the simulator rejects nothing."""
    rng = random.Random(1)
    width = max(BUS_WIDTH, cells // depth)
    table = ["top\tin\t-\t-\tpi\t\\in", "top\tout\t-\t-\tpo\t\\out"]
    if flops:
        table.insert(0, "top\tclk\t-\t-\tpi\t\\clk")
    declarations, assigns, always = [], [], []
    previous = [f"\\in [{i}]" for i in range(BUS_WIDTH)]
    count = 0

    for layer in range(depth):
        current = []
        flop_layer = flops and layer % FLOP_LAYER == FLOP_LAYER - 1
        for i in range(width):
            output = layer == depth - 1 and i < BUS_WIDTH
            y = f"\\out [{i}]" if output else f"\\n{layer}_{i}"
            if not output:
                declarations.append(f"  wire {verilog_wire(y)};")
            a = previous[i % len(previous)]     # keeps every wire of the last layer in use
            name = f"c{count}"
            count += 1
            if flop_layer:
                table += [f"top\t{name}\t$_DFF_P_\tC\tin\t\\clk",
                          f"top\t{name}\t$_DFF_P_\tD\tin\t{a}",
                          f"top\t{name}\t$_DFF_P_\tQ\tout\t{y}"]
                state = f"r{layer}_{i}"
                declarations.append(f"  reg {state} = 0;")
                assigns.append(f"  assign {verilog_wire(y)} = {state};")
                always.append(f"  always @(posedge clk) {state} <= {verilog_wire(a)};")
            else:
                gate = rng.choice(GATES)
                b, s = rng.choice(previous), rng.choice(previous)
                table.append(f"top\t{name}\t$_{gate}_\tA\tin\t{a}")
                if gate != "NOT":
                    table.append(f"top\t{name}\t$_{gate}_\tB\tin\t{b}")
                if gate == "MUX":
                    table.append(f"top\t{name}\t$_{gate}_\tS\tin\t{s}")
                table.append(f"top\t{name}\t$_{gate}_\tY\tout\t{y}")
                expression = EXPRESSIONS[gate].format(a=verilog_wire(a), b=verilog_wire(b), s=verilog_wire(s))
                assigns.append(f"  assign {verilog_wire(y)} = {expression};")
            current.append(y)
        previous = current

    path.with_suffix(".table").write_text("\n".join(table) + "\n")
    clock = "input clk, " if flops else ""
    path.with_suffix(".v").write_text(
        f"module top({clock}input [{BUS_WIDTH - 1}:0] in, output [{BUS_WIDTH - 1}:0] out);\n"
        + "\n".join(declarations + assigns + always) + "\nendmodule\n")
    return count


def write_multiplier(path, width):
    """Returns (cells, Yosys seconds)."""
    table, verilog = path.with_suffix(".table"), path.with_suffix(".v")
    seconds, _ = measure(["yosys", "-q", "-p",
                          f"read_verilog {MULTIPLIER}; chparam -set WIDTH {width} top; script {FLOW}; "
                          f"write_table {table}; write_verilog -noattr {verilog}"])
    return count_cells(table), seconds


def count_cells(table):
    return len({line.split("\t")[1] for line in table.read_text().splitlines() if "\t$_" in line})


def write_stimulus(path, inputs, clocked, activity, rows):
    """Flips each input bit with chance `activity`. Clocked circuits get three rows
    per cycle, so data only moves while the clock is low and steady."""
    rng = random.Random(7)
    bits = {name: [rng.getrandbits(1) for _ in range(width)] for name, width in inputs}
    entries = [name if width == 1 else f"{name}[{width - 1}:0]" for name, width in inputs]

    with path.open("w") as f:
        f.write("// inputs: " + ("clk " if clocked else "") + " ".join(entries) + "\n")
        for _ in range(rows // 3 if clocked else rows):
            for values in bits.values():
                for i in range(len(values)):
                    if rng.random() < activity:
                        values[i] ^= 1
            row = " ".join("".join(map(str, reversed(values))) for values in bits.values())
            if clocked:
                f.write(f"0 {row}\n1 {row}\n0 {row}\n")
            else:
                f.write(row + "\n")


def build_circuit(family, size, depth):
    """Returns (name, cells, Yosys seconds, input ports, clocked, output entry)."""
    name = f"{family}{size}" + (f"_depth{depth}" if depth != DEPTH else "")
    path = GENERATED / name
    if family == "multiplier":
        cells, seconds = write_multiplier(path, size)
        return name, cells, round(seconds, 3), [("a", size), ("b", size)], True, f"p[{2 * size - 1}:0]"
    flops = family == "mixed"
    cells = write_layered(path, size, depth, flops)
    return name, cells, "", [("in", BUS_WIDTH)], flops, f"out[{BUS_WIDTH - 1}:0]"


def add_cases(cases, circuit, family, size, depth, activities, row_counts):
    name, cells, seconds, inputs, clocked, output = circuit
    for activity in activities:
        for rows in row_counts:
            stimulus = GENERATED / f"{name}.{activity}.{rows}.st"
            write_stimulus(stimulus, inputs, clocked, activity, rows)
            cases.append({"name": f"{name}.{activity}.{rows}", "family": family, "size": size,
                          "cells": cells, "depth": depth, "activity": activity, "rows": rows,
                          "yosys_s": seconds, "table": GENERATED / f"{name}.table",
                          "verilog": GENERATED / f"{name}.v", "stimulus": stimulus, "output": output})


def main():
    parser = argparse.ArgumentParser(description="Write the benchmark circuits and stimulus.")
    parser.add_argument("--quick", action="store_true", help="small sizes, to try the scripts")
    args = parser.parse_args()

    sizes = SIZES[:2] if args.quick else SIZES
    widths = WIDTHS[:2] if args.quick else WIDTHS
    sweep_size = SIZES[0] if args.quick else SWEEP_SIZE

    shutil.rmtree(GENERATED, ignore_errors=True)
    GENERATED.mkdir()
    cases = []

    for family, family_sizes in (("random", sizes), ("mixed", sizes), ("multiplier", widths)):
        for size in family_sizes:
            print(f"{family} {size}", flush=True)
            circuit = build_circuit(family, size, DEPTH)
            add_cases(cases, circuit, family, size, DEPTH, ACTIVITIES, (ROWS,))

    add_cases(cases, build_circuit("mixed", sweep_size, DEPTH), "mixed", sweep_size, DEPTH,
              ACTIVITIES[:1], ROW_SWEEP)
    for depth in DEPTH_SWEEP:
        add_cases(cases, build_circuit("random", sweep_size, depth), "random", sweep_size, depth,
                  ACTIVITIES[:1], (ROWS,))

    with (GENERATED / "manifest.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, list(cases[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(cases)
    print(f"{len(cases)} cases in {GENERATED}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Compare our simulator against Icarus Verilog.

Each circuit goes through Yosys once, giving one netlist in two formats.
Icarus runs netlist.v and our simulator runs netlist.table, both on the same
generated inputs, and their outputs must match on every row.

    validation/icarus/validate.py                 run everything
    validation/icarus/validate.py mux counter     only matching circuits
    validation/icarus/validate.py --seeds 5 --cycles 1000
    validation/icarus/validate.py --flow techmap      use another flow from flows/

Needs yosys, iverilog, vvp and a built simulator (make). Exits 1 on any
failure. Generated files go to validation/icarus/out/.
"""
import argparse
import random
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
VALIDATION = HERE.parent
REPO = VALIDATION.parent
CIRCUITS = VALIDATION / "circuits"
FLOWS = VALIDATION / "flows"
OUT = HERE / "out"

CLOCKS = ("clk", "clock")
RESETS = ("rst", "reset")
RESET_CYCLES = 4        # reset is held for these cycles at the start
RESET_CHANCE = 1 / 16   # then asserted on this fraction of cycles
EXHAUSTIVE_BITS = 12    # combinational circuits this small try every input


class Failure(Exception):
    pass


def run(cmd, what):
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode:
        raise Failure(f"{what}: {(result.stderr or result.stdout).strip()[-200:]}")
    return result


def parse_port(entry):
    """'d[3:0]' -> ('d', 4), 'clk' -> ('clk', 1)"""
    match = re.fullmatch(r"(\w+)(?:\[(\d+):0\])?", entry)
    if not match:
        raise Failure(f"unsupported port '{entry}'")
    return match[1], int(match[2] or 0) + 1


def synthesize(source, flow, work, sim):
    """Returns the top module name, its input entries and its output entries."""
    table, verilog = work / "netlist.table", work / "netlist.v"
    run(["yosys", "-q", "-p",
         f"read_verilog {source}; script {FLOWS / (flow + '.ys')}; "
         f"write_table {table}; write_verilog -noattr {verilog}"], "yosys")

    module = table.read_text().split("\t", 1)[0]
    lines = run([sim, "ports", str(table)], "simulator rejected netlist").stdout.splitlines()
    ports = {}
    for line in lines:
        kind, _, entries = line.removeprefix("// ").partition(":")
        ports[kind] = entries.split()
    return module, ports["inputs"], ports["outputs"]


def random_bits(width, rng):
    return format(rng.getrandbits(width), f"0{width}b")


def combinational_rows(inputs, cycles, rng):
    rows = []
    total = sum(width for _, width in inputs)
    if total <= EXHAUSTIVE_BITS:
        for value in range(1 << total):
            bits = format(value, f"0{total}b")
            row, pos = {}, 0
            for name, width in inputs:
                row[name] = bits[pos:pos + width]
                pos += width
            rows.append(row)
    for _ in range(cycles):
        rows.append({name: random_bits(width, rng) for name, width in inputs})
    return rows


def sequential_rows(inputs, clock, cycles, rng):
    """Three rows per cycle, so inputs only change while clk is low and steady:
    (0,new) (1,new) (0,new). No edge sees moving data, whatever its polarity,
    and a clock gated by an input cannot glitch."""
    data = [port for port in inputs if port[0] != clock]

    def pick(cycle):
        values = {}
        for name, width in data:
            if name in RESETS:
                on = cycle < RESET_CYCLES or rng.random() < RESET_CHANCE
                values[name] = "1" if on else "0"
            else:
                values[name] = random_bits(width, rng)
        return values

    rows = []
    for cycle in range(cycles):
        values = pick(cycle)
        rows.append({**values, clock: "0"})
        rows.append({**values, clock: "1"})
        rows.append({**values, clock: "0"})
    return rows


def check_hold_time(rows, clock):
    """Our simulator samples the new data when data moves with the clock, and
    gives no warning, so such a row would not be a valid comparison."""
    for row, (before, after) in enumerate(zip(rows, rows[1:]), start=1):
        moved = any(after[name] != before[name] for name in after if name != clock)
        if moved and "1" in (before[clock], after[clock]):
            raise Failure(f"invalid stimulus: hold-time violation at row {row}")


def generate_rows(inputs, cycles, seed):
    rng = random.Random(seed)
    clock = next((name for name, _ in inputs if name in CLOCKS), None)
    if clock:
        rows = sequential_rows(inputs, clock, cycles, rng)
        check_hold_time(rows, clock)
        return rows
    return combinational_rows(inputs, cycles, rng)


def write_stimulus(path, entries, inputs, rows):
    with path.open("w") as f:
        f.write("// inputs: " + " ".join(entries) + "\n")
        for row in rows:
            f.write(" ".join(row[name] for name, _ in inputs) + "\n")


def write_testbench(path, module, inputs, outputs, output_entries, rows):
    names = ", ".join(name for name, _ in outputs)
    fmt = " ".join("%b" for _ in outputs)
    with path.open("w") as f:
        f.write("module tb;\n")
        for name, width in inputs:
            # starts at 0 like our simulator; x -> 0 would be a falling clock edge
            f.write(f"    reg [{width - 1}:0] {name} = 0;\n")
        for name, width in outputs:
            f.write(f"    wire [{width - 1}:0] {name};\n")
        connections = ", ".join(f".{name}({name})" for name, _ in inputs + outputs)
        f.write(f"    \\{module} dut ({connections});\n\n    initial begin\n")
        f.write(f'        $display("// outputs: {" ".join(output_entries)}");\n')
        for row in rows:
            drive = " ".join(f"{name} = {width}'b{row[name]};" for name, width in inputs)
            f.write(f'        {drive} #1; $display("{fmt}", {names});\n')
        f.write("        $finish;\n    end\nendmodule\n")


def run_icarus(work, result):
    compiled = work / "tb.vvp"
    # -g2012 applies the declaration values before any always block starts
    run(["iverilog", "-g2012", "-o", str(compiled), str(work / "tb.v"), str(work / "netlist.v")],
        "iverilog")
    output = run(["vvp", str(compiled)], "vvp").stdout
    result.write_text("".join(line + "\n" for line in output.splitlines() if "$finish" not in line))


def run_simulator(sim, work, result):
    run([sim, "sim", str(work / "netlist.table"), "-s", str(work / "stimulus"),
         "-o", str(result)], "simulator")


def compare(icarus_out, sim_out):
    """Returns (rows compared, mismatching rows, first mismatch message)."""
    icarus = icarus_out.read_text().splitlines()
    ours = sim_out.read_text().splitlines()
    if len(icarus) != len(ours):
        raise Failure(f"row counts differ: icarus {len(icarus)}, simulator {len(ours)}")
    if icarus and ours and icarus[0] != ours[0]:
        raise Failure(f"headers differ: '{icarus[0]}' vs '{ours[0]}'")
    bad = [row for row in range(1, len(icarus)) if icarus[row] != ours[row]]
    first = None
    if bad:
        row = bad[0]
        first = f"row {row}: icarus {icarus[row]}, simulator {ours[row]}"
    return len(icarus) - 1, len(bad), first


def validate(source, flow, sim, seeds, cycles):
    """Returns (rows compared, mismatching rows, first mismatch message)."""
    work = OUT / source.parent.name / flow
    work.mkdir(parents=True, exist_ok=True)
    module, input_entries, output_entries = synthesize(source, flow, work, sim)
    inputs = [parse_port(entry) for entry in input_entries]
    outputs = [parse_port(entry) for entry in output_entries]
    if not inputs or not outputs:
        raise Failure("circuit needs at least one input and one output")

    total, mismatches, first = 0, 0, None
    for seed in range(1, seeds + 1):
        rows = generate_rows(inputs, cycles, seed)
        write_stimulus(work / "stimulus", input_entries, inputs, rows)
        write_testbench(work / "tb.v", module, inputs, outputs, output_entries, rows)
        run_icarus(work, work / "icarus.out")
        run_simulator(sim, work, work / "sim.out")

        compared, bad, message = compare(work / "icarus.out", work / "sim.out")
        total += compared
        mismatches += bad
        if bad and first is None:
            first = f"seed {seed}, {message}"
            break   # leaves this seed's files in out/ for debugging
    return total, mismatches, first


def circuit_order(path):
    prefix = path.parent.name.split("_")[0]
    return (int(prefix) if prefix.isdigit() else sys.maxsize, path.parent.name)


def main():
    parser = argparse.ArgumentParser(description="Compare our simulator against Icarus Verilog.")
    parser.add_argument("circuits", nargs="*", help="only circuits whose name contains one of these")
    flows = sorted(path.stem for path in FLOWS.glob("*.ys"))
    parser.add_argument("--flow", choices=flows, default="abc")
    parser.add_argument("--seeds", type=int, default=3)
    parser.add_argument("--cycles", type=int, default=300)
    parser.add_argument("--sim", default=str(REPO / "simulator"), help="simulator binary")
    args = parser.parse_args()

    if not Path(args.sim).is_file():
        sys.exit(f"no simulator at {args.sim} (run make)")

    sources = sorted(CIRCUITS.glob("*/test.v"), key=circuit_order)
    if args.circuits:
        sources = [s for s in sources if any(c in s.parent.name for c in args.circuits)]
    if not sources:
        sys.exit("no matching circuits")

    passed = failed = errors = rows = 0
    for source in sources:
        label = f"{source.parent.name:27} {args.flow:8}"
        try:
            compared, bad, first = validate(source, args.flow, args.sim, args.seeds, args.cycles)
        except Failure as error:
            errors += 1
            print(f"ERROR {label} {error}")
            continue
        rows += compared
        if bad:
            failed += 1
            print(f"FAIL  {label} {compared:7} rows, {bad} mismatched: {first}")
        else:
            passed += 1
            print(f"PASS  {label} {compared:7} rows")

    print(f"\n{passed} passed, {failed} failed, {errors} errors, {rows} rows compared")
    sys.exit(1 if failed or errors else 0)


if __name__ == "__main__":
    main()

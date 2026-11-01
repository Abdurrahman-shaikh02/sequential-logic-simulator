#!/usr/bin/env python3
"""Times Icarus Verilog on every case in ../generated/ and writes results.csv.

    benchmark/simulation/icarus/run.py

iverilog is timed once per case and recorded as compile_s. vvp's timed runs print
nothing, matching our simulator's --no-dump. One extra untimed run with +dump
saves a hash of the outputs, which plot.py compares against the other simulators.
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]))
from measure import measure, output_hash, read_cases, runs_for, write_results


def parse_port(entry):
    """'d[3:0]' -> ('d', 4), 'clk' -> ('clk', 1)"""
    match = re.fullmatch(r"(\w+)(?:\[(\d+):0\])?", entry)
    return match[1], int(match[2] or 0) + 1


def write_testbench(path, stimulus, output_entry):
    lines = Path(stimulus).read_text().splitlines()
    inputs = [parse_port(entry) for entry in lines[0].split(":", 1)[1].split()]
    output, width = parse_port(output_entry)

    with path.open("w") as f:
        f.write("module tb;\n    reg dump = 0;\n")
        for name, bits in inputs:
            # starts at 0 like our simulator; x -> 0 would be a falling clock edge
            f.write(f"    reg [{bits - 1}:0] {name} = 0;\n")
        f.write(f"    wire [{width - 1}:0] {output};\n")
        connections = ", ".join(f".{name}({name})" for name in [port for port, _ in inputs] + [output])
        f.write(f"    \\top dut ({connections});\n\n    initial begin\n")
        f.write('        dump = $test$plusargs("dump");\n')
        f.write(f'        if (dump) $display("// outputs: {output_entry}");\n')
        for line in lines[1:]:
            drive = " ".join(f"{name} = {bits}'b{value};"
                             for (name, bits), value in zip(inputs, line.split()))
            f.write(f'        {drive} #1; if (dump) $display("%b", {output});\n')
        f.write("        $finish;\n    end\nendmodule\n")


def run_case(work, case):
    testbench, compiled = work / "tb.v", work / "tb.vvp"
    write_testbench(testbench, case["stimulus"], case["output"])
    # -g2012 applies the declaration values before any always block starts
    compile_s, _ = measure(["iverilog", "-g2012", "-o", str(compiled), str(testbench), case["verilog"]])
    seconds, memory = measure(["vvp", str(compiled)], runs_for(case["cells"]))

    output = subprocess.run(["vvp", str(compiled), "+dump"], capture_output=True, text=True, check=True)
    lines = [line for line in output.stdout.splitlines() if "$finish" not in line]
    return {"compile_s": round(compile_s, 3), "run_s": round(seconds, 3), "rss_kb": memory,
            "output_sha256": output_hash(lines)}


def main():
    cases = read_cases(HERE.parent / "generated")
    with tempfile.TemporaryDirectory() as work:
        write_results(HERE / "results.csv", cases, lambda case: run_case(Path(work), case))


if __name__ == "__main__":
    main()

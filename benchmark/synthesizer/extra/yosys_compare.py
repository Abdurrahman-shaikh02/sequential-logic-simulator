#!/usr/bin/env python3
"""Compares our synthesizer with Yosys on the same FSMs: gates, depth, time, memory.

    benchmark/synthesizer/extra/yosys_compare.py
    benchmark/synthesizer/extra/yosys_compare.py --timeout 600

Each table becomes a Verilog case statement using the state codes our netlist
uses. Yosys maps it to the same cells ours uses (2-input AND and OR, NOT, $_DFF_P_)
and `-nofsm` keeps those codes, so the comparison shows what logic optimization
gains and nothing else. Both write a real file. Every Yosys netlist is then run
in our simulator and compared with the table, because a smaller netlist counts
only if it does the same thing. In each family, a Yosys timeout ends the sweep.
Writes results/yosys_compare.csv.
"""
import argparse
import tempfile
from pathlib import Path

from common import (FSMS, GENERATED, REPO, RESULTS, SIM, Fsm, limited, measure, netlist_stats, simulate_and_compare,
                    spread, write_csv)

SWEEPS = [
    ("example", [REPO / "docs/fsm_example/traffic_light_fsm.net", REPO / "docs/fsm_example/dead_unreachable_fsm.net",
                 REPO / "examples/vending_machine/vending_machine.net"]),
    ("states", [FSMS / f"states_{n}.net" for n in (16, 64, 256, 1024, 4096, 16384)]),
    ("inputs_full", [FSMS / f"inputs_full_{n}.net" for n in (4, 6, 8, 10, 12)]),
    ("inputs_sparse", [FSMS / f"inputs_sparse_{n}.net" for n in (8, 32, 128)]),
    ("outputs", [FSMS / f"outputs_{n}.net" for n in (1, 16, 64, 256)]),
    ("structure", [FSMS / f"structure_{k}.net" for k in ("random", "ring", "counter", "two_way")]),
    ("unreachable", [FSMS / f"unreachable_0.75_{k}.net" for k in ("kept", "removed")]),
]
FIELDS = ["family", "name", "rows", "states", "inputs", "outputs", "status", "ours_gates", "yosys_gates", "gate_ratio",
          "ours_depth", "yosys_depth", "depth_ratio", "flip_flops", "ours_s", "yosys_s", "yosys_over_ours_time",
          "ours_mb", "yosys_mb", "yosys_mismatches", "yosys_outputs_compared"]
SCRIPT = ("read_verilog {verilog}; synth -top fsm -nofsm; dfflegalize -cell $_DFF_P_ x; abc -g AND,OR; "
          "opt_clean -purge; setundef -zero; setundef -init -zero; write_table {table}")


def state_codes(netlist):
    with open(netlist) as f:
        for line in f:
            if line.startswith("# state encoding"):
                return dict(entry.split("=") for entry in line.split(":", 1)[1].split("(reset")[0].split())
    raise RuntimeError(f"{netlist}: no state encoding")


def write_verilog(path, fsm, codes):
    state_bits = len(next(iter(codes.values())))
    with open(path, "w") as f:
        ports = ["input clk", "input reset"] + [f"input {n}" for n in fsm.inputs] + [f"output reg {n}" for n in fsm.outputs]
        f.write(f"module fsm({', '.join(ports)});\n")
        f.write(f"reg [{state_bits - 1}:0] state, following;\n")
        outs = "{" + ", ".join(fsm.outputs) + "}" if fsm.outputs else None
        key = "{" + ", ".join(["state"] + fsm.inputs) + "}"
        f.write("always @* begin\n  following = 0;\n")
        if outs:
            f.write(f"  {outs} = 0;\n")
        f.write(f"  case ({key})\n")
        for state, rows in fsm.rows.items():
            for bits, (following, out) in rows.items():
                f.write(f"    {state_bits + len(bits)}'b{codes[state]}{bits}: begin following = {state_bits}'b{codes[following]};")
                if outs:
                    f.write(f" {outs} = {len(out)}'b{out};")
                f.write(" end\n")
        f.write("    default: ;\n  endcase\nend\n")
        f.write("always @(posedge clk) state <= reset ? 0 : following;\nendmodule\n")


def timed(command):
    """Median of 3 for quick commands; a long command is timed once."""
    first, peak = measure(command)
    if first > 30:
        return first, peak
    times = [first]
    for _ in range(2):
        seconds, memory = measure(command)
        times.append(seconds)
        peak = max(peak, memory)
    return spread(times)[1], peak


def run_case(family, path, timeout):
    fsm = Fsm(path)
    row = {"family": family, "name": path.stem, "rows": sum(len(r) for r in fsm.rows.values()),
           "states": len(set(fsm.rows) | {n for r in fsm.rows.values() for n, _ in r.values()}),
           "inputs": len(fsm.inputs), "outputs": len(fsm.outputs), "status": "ok"}
    with tempfile.TemporaryDirectory(dir=GENERATED) as work:
        work = Path(work)
        ours_table, yosys_table, verilog = work / "ours.table", work / "yosys.table", work / "fsm.v"
        seconds, memory = timed(limited([SIM, "--fsm", str(path), "--synth", "-o", str(ours_table)], 7, 600))
        ours = netlist_stats(ours_table)
        row.update(ours_gates=ours["gates"], ours_depth=ours["depth"], flip_flops=ours["flip_flops"],
                   ours_s=round(seconds, 3), ours_mb=round(memory / 1024, 1))
        write_verilog(verilog, fsm, state_codes(ours_table))
        script = SCRIPT.format(verilog=verilog, table=yosys_table)
        try:
            seconds, memory = timed(limited(["yosys", "-q", "-p", script], 7, timeout))
        except RuntimeError as error:
            row["status"] = "yosys did not finish: " + " ".join(str(error).split())[-80:]
            print(row, flush=True)
            return row
        theirs = netlist_stats(yosys_table)
        if theirs["flip_flops"] != ours["flip_flops"]:
            row["status"] = f"different flip-flops: {theirs['flip_flops']}"
        check = simulate_and_compare(yosys_table, fsm, 1000)
        row.update(yosys_gates=theirs["gates"], yosys_depth=theirs["depth"], yosys_s=round(seconds, 3),
                   yosys_mb=round(memory / 1024, 1), gate_ratio=round(ours["gates"] / max(theirs["gates"], 1), 2),
                   depth_ratio=round(ours["depth"] / max(theirs["depth"], 1), 2),
                   yosys_over_ours_time=round(seconds / max(row["ours_s"], 0.001), 1),
                   yosys_mismatches=check["mismatches"], yosys_outputs_compared=check["outputs_compared"])
        if check["mismatches"]:
            row["status"] = "yosys netlist differs from the table"
    print(row, flush=True)
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--timeout", type=int, default=300, help="seconds allowed for each Yosys run")
    parser.add_argument("--only", default="", help="only families whose name contains this")
    args = parser.parse_args()
    GENERATED.mkdir(exist_ok=True)
    results = []
    for family, paths in SWEEPS:
        if args.only not in family:
            continue
        for path in paths:
            row = run_case(family, path, args.timeout)
            results.append(row)
            write_csv(RESULTS / "yosys_compare.csv", results, FIELDS)
            if row["status"].startswith("yosys did not finish"):
                break


if __name__ == "__main__":
    main()

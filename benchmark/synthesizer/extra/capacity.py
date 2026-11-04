#!/usr/bin/env python3
"""Finds the largest FSM the synthesizer finishes under a memory limit, doubling
past the benchmark's largest case (262,144 rows) until a run fails.

    benchmark/synthesizer/extra/capacity.py
    benchmark/synthesizer/extra/capacity.py --max-memory-gb 10

FSMs are like the `states` family: random, full table, 2 inputs, 4 outputs. Each
size is timed 3 times with the netlist going to /dev/null, as run.py does, then
once more writing a real file, to count its gates. The FSM stays in generated/
for end_to_end.py. Writes results/capacity.csv.
"""
import argparse
import random
import sys

from common import GENERATED, HERE, RESULTS, SIM, limited, measure, spread, write_csv

sys.path.insert(0, str(HERE.parent))
from generate import random_table, table_text
from run import netlist_size

SIZES = (1 << 20, 3 << 19, 1 << 21, 3 << 20, 1 << 22, 3 << 21, 1 << 23)
MAX_DEPTH_ROWS = 1 << 20        # above this, finding the depth in Python needs too much memory


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--max-memory-gb", type=float, default=7)
    args = parser.parse_args()

    GENERATED.mkdir(exist_ok=True)
    results = []
    for rows in SIZES:
        states = rows // 4
        name = f"capacity_{rows}"
        fsm = GENERATED / f"{name}.net"
        fsm.write_text(table_text(random_table(random.Random(name), states, 2, 4), 2, 4))
        row = {"name": name, "states": states, "rows": rows, "memory_limit_gb": args.max_memory_gb, "status": "ok"}
        try:
            command = limited([SIM, "--fsm", str(fsm), "--synth"], args.max_memory_gb, 1800)
            times, peak = [], 0
            for _ in range(3):
                seconds, memory = measure(command)
                times.append(seconds)
                peak = max(peak, memory)
                print(f"{name} {seconds:.2f} s {memory / 1024:.0f} MB", flush=True)
            low, middle, high = spread(times)
            table = GENERATED / f"{name}.table"
            write_s, _ = measure(limited([SIM, "--fsm", str(fsm), "--synth", "-o", str(table)],
                                         args.max_memory_gb, 1800))
            row.update(time_min_s=round(low, 2), time_median_s=round(middle, 2), time_max_s=round(high, 2),
                       rss_mb=round(peak / 1024), write_file_s=round(write_s, 2),
                       netlist_mb=table.stat().st_size >> 20)
            if rows <= MAX_DEPTH_ROWS:
                size = netlist_size(table)
                row.update(gates=size["gates"], flip_flops=size["flip_flops"], depth=size["depth"])
            else:
                with open(table) as f:
                    row["gates"] = sum("\tY\tout\t" in line for line in f)
            table.unlink()
        except RuntimeError as error:
            row["status"] = "failed: " + " ".join(str(error).split())[-120:]
            print(f"{name}: {row['status']}", flush=True)
        results.append(row)
        write_csv(RESULTS / "capacity.csv", results,
                  ["name", "states", "rows", "memory_limit_gb", "status", "time_min_s", "time_median_s", "time_max_s",
                   "rss_mb", "write_file_s", "netlist_mb", "gates", "flip_flops", "depth"])
        if row["status"] != "ok":
            fsm.unlink()
            break


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Runs the largest benchmark FSMs many times and saves the min, median and max
of their time and memory, because one median hides how much a run can vary.

    benchmark/synthesizer/extra/repeat_runs.py
    benchmark/synthesizer/extra/repeat_runs.py --runs 20

Uses the same command as run.py. The runs take turns, one of each FSM per round,
so slow drift in the machine affects every FSM alike. Writes repeat_runs.csv (one
row per run) and repeat_summary.csv to results/.
"""
import argparse
import csv

from common import FSMS, GB, RESULTS, SIM, limited, measure, spread, write_csv

CASES = ["states_4096", "states_16384", "states_65536", "inputs_full_14", "inputs_sparse_256", "outputs_1024"]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--runs", type=int, default=10)
    args = parser.parse_args()

    with open(FSMS / "manifest.csv") as f:
        rows_of = {row["name"]: int(row["rows"]) for row in csv.DictReader(f)}

    raw = []
    for number in range(1, args.runs + 1):
        for name in CASES:
            command = limited([SIM, "--fsm", str(FSMS / f"{name}.net"), "--synth"], 6, 600)
            seconds, memory = measure(command)
            raw.append({"run": number, "name": name, "rows": rows_of[name], "run_s": round(seconds, 3),
                        "rss_mb": round(memory / 1024, 1)})
            print(f"run {number:2} {name:20} {seconds:8.3f} s {memory / 1024:8.1f} MB", flush=True)
        write_csv(RESULTS / "repeat_runs.csv", raw)

    summary = []
    for name in CASES:
        times = [row["run_s"] for row in raw if row["name"] == name]
        memory = [row["rss_mb"] for row in raw if row["name"] == name]
        low, middle, high = spread(times)
        mem_low, mem_middle, mem_high = spread(memory)
        summary.append({"name": name, "rows": rows_of[name], "runs": len(times),
                        "time_min_s": low, "time_median_s": round(middle, 3), "time_max_s": high,
                        "max_over_min": round(high / low, 2),
                        "memory_min_mb": mem_low, "memory_median_mb": round(mem_middle, 1), "memory_max_mb": mem_high})
    write_csv(RESULTS / "repeat_summary.csv", summary)


if __name__ == "__main__":
    main()

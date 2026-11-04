#!/usr/bin/env python3
"""Finishes the extra tests using only cases that are safe on a laptop that
overheats: at most 16,384 rows, mostly 4,096 or fewer, and a short Yosys limit.

    benchmark/synthesizer/extra/light_run.py

Before each case it waits for the CPU to cool to COOL_C, and stops if that takes
longer than MAX_WAIT_S. Every case is pinned to one core by measure.py.

Rows already in results/end_to_end.csv are kept. Left out on purpose, because of
memory and heat: outputs_1024 and capacity_1048576 end to end, the states_65536
controls, and the larger Yosys sizes in SKIP_YOSYS. Writes end_to_end.csv,
end_to_end_controls.csv and yosys_compare.csv in results/.
"""
import csv
import time
from pathlib import Path

import end_to_end
import yosys_compare
from common import FSMS, GENERATED, RESULTS, write_csv

COOL_C = 80
MAX_WAIT_S = 600
YOSYS_TIMEOUT_S = 60
END_TO_END = [("structure_counter", 1000), ("outputs_64", 1000)]    # outputs_64 stands in for outputs_1024
CONTROLS = [("states_1024", "output", 1000), ("states_1024", "next", 1000),
            ("states_4096", "output", 200), ("states_4096", "next", 200)]
SKIP_YOSYS = {"states_4096", "states_16384", "inputs_full_10", "inputs_full_12", "inputs_sparse_128", "outputs_256"}


def cpu_temperature():
    for zone in Path("/sys/class/thermal").glob("thermal_zone*"):
        if (zone / "type").read_text().strip() == "x86_pkg_temp":
            return int((zone / "temp").read_text()) / 1000
    raise SystemExit("no CPU temperature sensor found, so not running without the guard")


def wait_until_cool():
    start = time.monotonic()
    while (temperature := cpu_temperature()) > COOL_C:
        if time.monotonic() - start > MAX_WAIT_S:
            raise SystemExit(f"stopping: CPU still at {temperature} C after {MAX_WAIT_S} s")
        print(f"CPU at {temperature} C, waiting", flush=True)
        time.sleep(30)
    print(f"[CPU {temperature} C]", flush=True)


def run_end_to_end():
    path = RESULTS / "end_to_end.csv"
    names = {name for name, _ in END_TO_END}
    with open(path) as f:
        results = [row for row in csv.DictReader(f) if row["name"] not in names]
    for name, cycles in END_TO_END:
        wait_until_cool()
        results.append(end_to_end.run_case(name, FSMS / f"{name}.net", cycles))
        write_csv(path, results, end_to_end.FIELDS)


def run_controls():
    results = []
    for name, kind, cycles in CONTROLS:
        wait_until_cool()
        results.append(end_to_end.control(name, kind, cycles))
        write_csv(RESULTS / "end_to_end_controls.csv", results)
    if not all(row["noticed"] for row in results):
        raise SystemExit("a damaged FSM was not noticed")


def run_yosys():
    results = []
    for family, paths in yosys_compare.SWEEPS:
        for path in paths:
            if path.stem in SKIP_YOSYS:
                continue
            wait_until_cool()
            row = yosys_compare.run_case(family, path, YOSYS_TIMEOUT_S)
            results.append(row)
            write_csv(RESULTS / "yosys_compare.csv", results, yosys_compare.FIELDS)
            if row["status"].startswith("yosys did not finish"):
                break


def main():
    GENERATED.mkdir(exist_ok=True)
    run_end_to_end()
    run_controls()
    run_yosys()


if __name__ == "__main__":
    main()

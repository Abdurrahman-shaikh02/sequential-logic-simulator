"""Timing shared by every benchmark, so all programs are measured the same way.

Each command runs alone, pinned to one CPU core, and is timed from the outside as
a whole process. That includes loading and parsing, and needs no counters from
the program being measured. The wait is blocking, because polling rounds short
runs to about 50 ms.
"""
import csv
import hashlib
import statistics
import subprocess
import tempfile
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PIN = ["taskset", "-c", "0"]

RUNS = 3
LARGE_RUNS = 2          # circuits this big take minutes, so they run fewer times
LARGE_CELLS = 100_000

FIELDS = ["family", "size", "cells", "depth", "activity", "rows",
          "yosys_s", "compile_s", "run_s", "rss_kb", "output_sha256"]


def measure(cmd, runs=1):
    """Returns (median wall seconds, peak memory in KB) over `runs` runs."""
    times = []
    peak = 0
    with tempfile.NamedTemporaryFile(mode="r") as report:
        for _ in range(runs):
            start = time.perf_counter()
            done = subprocess.run(["/usr/bin/time", "-q", "-f", "%M", "-o", report.name] + PIN + cmd,
                                  stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
            times.append(time.perf_counter() - start)
            if done.returncode:
                raise RuntimeError(f"{cmd[0]} failed: {done.stderr.strip()[-200:]}")
            report.seek(0)
            peak = max(peak, int(report.read().split()[-1]))
    return statistics.median(times), peak


def runs_for(cells):
    return LARGE_RUNS if int(cells) >= LARGE_CELLS else RUNS


def output_hash(lines):
    """Same text from two simulators gives the same hash, however it was produced."""
    return hashlib.sha256("\n".join(lines).encode()).hexdigest()


def read_cases(generated):
    manifest = Path(generated) / "manifest.csv"
    if not manifest.is_file():
        raise SystemExit(f"no {manifest} (run generate.py)")
    with manifest.open() as f:
        return list(csv.DictReader(f))


def write_results(path, cases, run_case):
    """Writes one CSV row per case, as each finishes, so a stopped run keeps its rows."""
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, FIELDS, lineterminator="\n")
        writer.writeheader()
        for case in cases:
            row = {name: case[name] for name in FIELDS if name in case}
            row.update(run_case(case))
            writer.writerow(row)
            f.flush()
            print(f"{case['name']:32} {row['run_s']:>9} s", flush=True)

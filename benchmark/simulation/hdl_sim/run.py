#!/usr/bin/env python3
"""Times our simulator on every case in ../generated/ and writes results.csv.

    make
    benchmark/simulation/hdl_sim/run.py
    benchmark/simulation/hdl_sim/run.py --sim path/to/simulator

The timed runs do not write a trace. One extra untimed run saves a hash of the
outputs, which plot.py compares against the other simulators.
"""
import argparse
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]))
from measure import REPO, measure, output_hash, read_cases, runs_for, write_results


def run_case(sim, case):
    command = [sim, "sim", case["table"], "-s", case["stimulus"], "-q"]
    seconds, memory = measure(command + ["--no-dump"], runs_for(case["cells"]))
    with tempfile.NamedTemporaryFile(mode="r") as trace:
        measure(command + ["-o", trace.name])
        trace.seek(0)
        outputs = trace.read().splitlines()
    return {"compile_s": 0, "run_s": round(seconds, 3), "rss_kb": memory,
            "output_sha256": output_hash(outputs)}


def main():
    parser = argparse.ArgumentParser(description="Time our simulator on the benchmark circuits.")
    parser.add_argument("--sim", default=str(REPO / "simulator"), help="simulator binary")
    args = parser.parse_args()
    if not Path(args.sim).is_file():
        sys.exit(f"no simulator at {args.sim} (run make)")

    cases = read_cases(HERE.parent / "generated")
    write_results(HERE / "results.csv", cases, lambda case: run_case(args.sim, case))


if __name__ == "__main__":
    main()

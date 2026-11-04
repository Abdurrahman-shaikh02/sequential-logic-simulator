#!/usr/bin/env python3
"""Times our synthesizer on every FSM in fsms/ and writes results/current/results.csv.

    make
    benchmark/synthesizer/run.py
    benchmark/synthesizer/run.py --sim path/to/simulator
    benchmark/synthesizer/run.py --max-memory-gb 10

Timed runs send the netlist to /dev/null. Two extra untimed runs save it, to
measure its size and check that both runs wrote the same bytes.

Each run has a memory limit, so a case that needs too much memory fails alone
instead of slowing or crashing the whole machine. Its time and memory stay
empty in results.csv.
"""
import argparse
import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from measure import REPO, measure, read_cases, runs_for, write_results

FIELDS = ["name", "family", "value", "variant", "states", "inputs", "outputs", "rows", "output_ones", "file_kb",
          "run_s", "rss_kb", "gates", "and_gates", "or_gates", "not_gates", "flip_flops", "depth",
          "netlist_kb", "output_sha256"]


def netlist_size(path):
    """Counts gates and finds the longest path through them, in gates.

    The synthesizer writes each gate after the gates feeding it, so one pass in
    file order is enough. A wire not seen yet is a flip-flop output, or a primary
    input, and both start a path at depth 0."""
    counts = {"$_AND_": 0, "$_OR_": 0, "$_NOT_": 0, "$_DFF_P_": 0}
    depth_of = {}
    deepest = 0
    reading = 0     # deepest input of the gate being read
    with open(path) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            _, _, kind, port, direction, wire = line.rstrip("\n").split("\t")
            if kind not in counts:
                continue
            if port == "Y":
                counts[kind] += 1
                depth_of[wire] = reading + 1
                deepest = max(deepest, reading + 1)
                reading = 0
            elif port == "Q":
                counts[kind] += 1
                reading = 0
            elif port != "C":
                reading = max(reading, depth_of.get(wire, 0))
    return {"gates": counts["$_AND_"] + counts["$_OR_"] + counts["$_NOT_"], "and_gates": counts["$_AND_"],
            "or_gates": counts["$_OR_"], "not_gates": counts["$_NOT_"], "flip_flops": counts["$_DFF_P_"],
            "depth": deepest}


def file_hash(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def run_case(sim, limit, case):
    command = ["prlimit", f"--as={limit}", sim, "--fsm", case["file"], "--synth"]
    try:
        seconds, memory = measure(command, runs_for(case["rows"]))
    except RuntimeError as error:
        print(f"{case['name']}: {error}", file=sys.stderr, flush=True)
        return {"run_s": "", "rss_kb": ""}

    with tempfile.TemporaryDirectory() as folder:
        first, second = Path(folder) / "first.table", Path(folder) / "second.table"
        for path in (first, second):
            subprocess.run(command + ["-o", str(path)], check=True, stdin=subprocess.DEVNULL, capture_output=True)
        digest = file_hash(first)
        if digest != file_hash(second):
            raise SystemExit(f"{case['name']}: two runs wrote different netlists")
        size = netlist_size(first)
        return {"run_s": round(seconds, 3), "rss_kb": memory, "netlist_kb": first.stat().st_size // 1024,
                "output_sha256": digest, **size}


def main():
    parser = argparse.ArgumentParser(description="Time our synthesizer on the benchmark FSMs.")
    parser.add_argument("--sim", default=str(REPO / "simulator"), help="simulator binary")
    parser.add_argument("--max-memory-gb", type=float, default=6, help="memory limit of each run")
    args = parser.parse_args()
    if not Path(args.sim).is_file():
        sys.exit(f"no simulator at {args.sim} (run make)")

    limit = int(args.max_memory_gb * (1 << 30))
    cases = read_cases(HERE / "fsms")
    results = HERE / "results" / "current"
    results.mkdir(parents=True, exist_ok=True)
    write_results(results / "results.csv", cases, lambda case: run_case(args.sim, limit, case), FIELDS)


if __name__ == "__main__":
    main()

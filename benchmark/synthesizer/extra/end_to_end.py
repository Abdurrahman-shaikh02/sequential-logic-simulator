#!/usr/bin/env python3
"""Runs generated netlists through the whole tool chain: synthesize, parse,
simulate, and compare the simulator's outputs with the FSM table.

    benchmark/synthesizer/extra/end_to_end.py

validation/ proves behaviour exhaustively, but only up to 1,024 states, and does
not simulate. This covers the larger sizes, on a random walk through the table:
a spot check, not a proof. Damaged FSMs show that the comparison can fail. Writes
results/end_to_end.csv and results/end_to_end_controls.csv.
"""
from pathlib import Path

from common import (FSMS, GENERATED, RESULTS, SIM, Fsm, limited, measure, simulate_and_compare, synthesize, walk,
                    write_csv)

CASES = [("states_256", 1000), ("states_1024", 1000), ("states_4096", 1000), ("states_16384", 200),
         ("states_65536", 200), ("inputs_full_14", 200), ("inputs_sparse_256", 200), ("outputs_1024", 200),
         ("structure_counter", 1000)]
FIELDS = ["name", "rows", "status", "synth_s", "check_s", "check_mb", "sim_s", "sim_mb", "cycles", "states_visited",
          "outputs_compared", "mismatches"]


def run_case(name, path, cycles):
    fsm = Fsm(path)
    row = {"name": name, "rows": sum(len(rows) for rows in fsm.rows.values()), "status": "ok"}
    table = GENERATED / f"{name}.e2e.table"
    try:
        seconds, _ = synthesize(path, table)
        row["synth_s"] = round(seconds, 3)
        seconds, memory = measure(limited([SIM, "--check", str(table)], 7, 1800))
        row.update(check_s=round(seconds, 3), check_mb=round(memory / 1024, 1))
        row.update(simulate_and_compare(table, fsm, cycles))
        if row["mismatches"]:
            row["status"] = "MISMATCH"
    except RuntimeError as error:
        row["status"] = "failed: " + " ".join(str(error).split())[-120:]
    finally:
        table.unlink(missing_ok=True)
    print(row, flush=True)
    return row


def damaged(path, target, change):
    """Rewrites one row of the table. `change` gets (next state, outputs)."""
    state, bits = target
    lines = []
    for line in Path(path).read_text().splitlines():
        parts = [part.strip() for part in line.split("|")]
        if len(parts) == 4 and parts[0] == state and "".join(parts[1].split()) == bits:
            following, out = change(parts[2], parts[3])
            line = f"{state} | {parts[1]} | {following} | {out}"
        lines.append(line)
    return "\n".join(lines) + "\n"


def control(name, kind, cycles):
    """Synthesizes a damaged table and compares it with the original on a row the
    walk uses: the comparison must report a difference."""
    path = FSMS / f"{name}.net"
    fsm = Fsm(path)
    _, _, _, used = walk(fsm, cycles, 1)
    target = used[len(used) // 2]
    if kind == "output":
        change = lambda following, out: (following, ("0" if out[0] == "1" else "1") + out[1:])
    else:
        other = next(state for state in fsm.rows if state != fsm.rows[target[0]][target[1]][0])
        change = lambda following, out: (other, out)
    bad_fsm = GENERATED / f"{name}.{kind}.net"
    bad_table = GENERATED / f"{name}.{kind}.table"
    bad_fsm.write_text(damaged(path, target, change))
    try:
        synthesize(bad_fsm, bad_table)
        result = simulate_and_compare(bad_table, fsm, cycles)
    finally:
        bad_fsm.unlink(missing_ok=True)
        bad_table.unlink(missing_ok=True)
    row = {"name": name, "damage": kind, "mismatches": result["mismatches"], "noticed": result["mismatches"] > 0}
    print(row, flush=True)
    return row


def main():
    GENERATED.mkdir(exist_ok=True)
    results = []
    cases = [(name, FSMS / f"{name}.net", cycles) for name, cycles in CASES]
    cases += [(path.stem, path, 100) for path in sorted(GENERATED.glob("capacity_*.net"), key=lambda p: int(p.stem[9:]))]
    for name, path, cycles in cases:
        results.append(run_case(name, path, cycles))
        write_csv(RESULTS / "end_to_end.csv", results, FIELDS)

    controls = [control("states_1024", "output", 1000), control("states_1024", "next", 1000),
                control("states_65536", "output", 200), control("states_65536", "next", 200)]
    write_csv(RESULTS / "end_to_end_controls.csv", controls)
    if not all(row["noticed"] for row in controls):
        raise SystemExit("a damaged FSM was not noticed")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Turns every <simulator>/results.csv into graphs in plots/.

    benchmark/simulation/plot.py

Exits 1 if two simulators produced different outputs on any case, because their
timings cannot be compared then. Needs matplotlib.
"""
import csv
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter, NullFormatter

HERE = Path(__file__).resolve().parent
PLOTS = HERE / "plots"

FAMILIES = ("random", "mixed", "multiplier")
ACTIVITY, ROWS, DEPTH = "0.5", 900, 32   # the size graphs hold these fixed
LABELS = {"hdl_sim": "Our simulator", "icarus": "Icarus"}
COLORS = ("#2a78d6", "#eb6834", "#1baf7a", "#eda100")   # a new simulator takes the next one


def load():
    """Returns {simulator: rows}, with our simulator first so it keeps its colour."""
    results = {}
    for path in sorted(HERE.glob("*/results.csv"), key=lambda p: (p.parent.name != "hdl_sim", p.parent.name)):
        with path.open() as f:
            results[path.parent.name] = [{**row, "cells": int(row["cells"]), "rows": int(row["rows"]),
                                          "depth": int(row["depth"]), "run_s": float(row["run_s"]),
                                          "compile_s": float(row["compile_s"]),
                                          "yosys_s": float(row["yosys_s"] or 0),
                                          "rss_kb": int(row["rss_kb"])} for row in csv.DictReader(f)]
    if not results:
        sys.exit("no results.csv found (run hdl_sim/run.py and icarus/run.py)")
    return results


def key(row):
    return row["family"], row["size"], row["depth"], row["activity"], row["rows"]


def compare_outputs(results):
    """Prints and returns the cases where simulators disagree."""
    hashes = defaultdict(dict)
    for simulator, rows in results.items():
        for row in rows:
            hashes[key(row)][simulator] = row["output_sha256"]
    different = [case for case, seen in hashes.items() if len(set(seen.values())) > 1]
    partial = sum(len(seen) < len(results) for seen in hashes.values())
    print(f"outputs compared on {len(hashes)} cases, {len(different)} differ")
    if partial:
        print(f"{partial} cases are missing from at least one simulator")
    for case in different:
        print("  DIFFERENT", *case)
    return different


def at_default_settings(rows, family):
    return sorted((row for row in rows if row["family"] == family and row["activity"] == ACTIVITY
                   and row["rows"] == ROWS and row["depth"] == DEPTH), key=lambda row: row["cells"])


def style(axis, title, xlabel, ylabel):
    axis.set_xscale("log")
    axis.set_yscale("log")
    for ticks in (axis.xaxis, axis.yaxis):
        ticks.set_major_formatter(FuncFormatter(lambda value, _: f"{value:g}"))
        ticks.set_minor_formatter(NullFormatter())
    axis.set_title(title, loc="left", fontsize=11)
    axis.set_xlabel(xlabel)
    axis.set_ylabel(ylabel)
    axis.grid(True, which="major", color="#e4e3df", linewidth=0.7)
    axis.spines[["top", "right"]].set_visible(False)


def draw(axis, simulator, color, x, y):
    axis.plot(x, y, color=color, linewidth=2, marker="o", markersize=5, label=LABELS.get(simulator, simulator))


def by_size(results, filename, title, ylabel, value):
    """One panel per circuit family, so each keeps a readable scale."""
    figure, axes = plt.subplots(1, len(FAMILIES), figsize=(12, 3.8), constrained_layout=True)
    for axis, family in zip(axes, FAMILIES):
        for (simulator, rows), color in zip(results.items(), COLORS):
            points = at_default_settings(rows, family)
            draw(axis, simulator, color, [row["cells"] for row in points], [value(row) for row in points])
        style(axis, family.capitalize(), "Cells", ylabel)
    axes[0].legend(frameon=False)
    figure.suptitle(title, x=0.01, ha="left", fontsize=13)
    figure.savefig(PLOTS / filename, dpi=150)
    plt.close(figure)


def sweep(results, filename, title, xlabel, field, chosen):
    points_by_simulator = {simulator: sorted(filter(chosen, rows), key=lambda row: row[field])
                           for simulator, rows in results.items()}
    figure, axis = plt.subplots(figsize=(5.5, 3.8), constrained_layout=True)
    for (simulator, points), color in zip(points_by_simulator.items(), COLORS):
        draw(axis, simulator, color, [row[field] for row in points], [row["run_s"] for row in points])
    style(axis, title, xlabel, "Run time (s)")
    axis.legend(frameon=False)
    figure.savefig(PLOTS / filename, dpi=150)
    plt.close(figure)


def write_slopes(results):
    """1 means time grows in step with size, 2 means quadratic."""
    with (PLOTS / "slopes.csv").open("w", newline="") as f:
        writer = csv.writer(f, lineterminator="\n")
        writer.writerow(["simulator", "family", "activity", "points", "run_slope", "compile_slope"])
        for simulator, rows in results.items():
            for family in FAMILIES:
                for activity in sorted({row["activity"] for row in rows}):
                    points = sorted((row for row in rows if row["family"] == family and row["activity"] == activity
                                     and row["rows"] == ROWS and row["depth"] == DEPTH),
                                    key=lambda row: row["cells"])
                    if len(points) < 2:
                        continue
                    cells = np.log([row["cells"] for row in points])
                    slope = lambda name: round(float(np.polyfit(cells, np.log([max(row[name], 1e-3) for row in points]), 1)[0]), 2)
                    compile_slope = slope("compile_s") if any(row["compile_s"] for row in points) else ""
                    writer.writerow([simulator, family, activity, len(points), slope("run_s"), compile_slope])


def main():
    results = load()
    different = compare_outputs(results)
    PLOTS.mkdir(exist_ok=True)

    by_size(results, "run_time.png", "Run time by circuit size", "Run time (s)", lambda row: row["run_s"])
    by_size(results, "total_time.png", "Total time: Yosys, compile and run", "Total time (s)",
            lambda row: row["yosys_s"] + row["compile_s"] + row["run_s"])
    by_size(results, "memory.png", "Peak memory by circuit size", "Peak memory (MB)", lambda row: row["rss_kb"] / 1024)

    size_of = lambda family: max((row["size"] for rows in results.values() for row in rows
                                  if row["family"] == family and (row["rows"] != ROWS or row["depth"] != DEPTH)), default=None)
    mixed, shallow = size_of("mixed"), size_of("random")
    sweep(results, "stimulus_length.png", "Run time by stimulus length", "Rows", "rows",
          lambda row: row["family"] == "mixed" and row["size"] == mixed and row["activity"] == ACTIVITY and row["depth"] == DEPTH)
    sweep(results, "depth.png", "Run time by logic depth", "Layers", "depth",
          lambda row: row["family"] == "random" and row["size"] == shallow and row["activity"] == ACTIVITY and row["rows"] == ROWS)
    write_slopes(results)
    print(f"graphs in {PLOTS}")
    sys.exit(1 if different else 0)


if __name__ == "__main__":
    main()

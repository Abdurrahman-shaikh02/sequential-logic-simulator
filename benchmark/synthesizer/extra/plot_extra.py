#!/usr/bin/env python3
"""Turns the extra tests' CSVs into graphs in results/plots/. Runs nothing else, so
it is safe on a machine that overheats.

    benchmark/synthesizer/extra/plot_extra.py

Draws yosys_growth.png, yosys_ratios.png, end_to_end.png and capacity.png. The
Yosys ratios use the 18 comparable FSMs: ring, counter and two-way are left out
because Yosys removed flip-flops on them. Needs matplotlib.
"""
import csv
import statistics
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import matplotlib.pyplot as plt
from matplotlib.ticker import FixedLocator, FuncFormatter, NullLocator

from plot import BLUE, GRID, INK, MUTED, ORANGE, finish, line, panels, style

VIOLET = "#8a5cd0"
RESULTS = HERE / "results"
PLOTS = RESULTS / "plots"
CURRENT = HERE.parent / "results" / "current" / "results.csv"
MB_PER_GB = 1024

END_TO_END_NAMES = {
    "states_256": "256 states",
    "states_1024": "1,024 states",
    "states_4096": "4,096 states",
    "states_16384": "16,384 states",
    "states_65536": "65,536 states",
    "inputs_full_14": "14 inputs, all rows",
    "inputs_sparse_256": "256 inputs, sparse",
    "structure_counter": "Counter",
    "outputs_64": "64 outputs",
}


def read(path):
    with open(path) as f:
        return list(csv.DictReader(f))


def label_end(axis, x, y, text):
    axis.annotate(text, (x, y), xytext=(7, -3), textcoords="offset points", color=INK, fontsize=9)


def yosys_growth(rows):
    """Ours and Yosys on the same FSMs, as states are added."""
    points = sorted((r for r in rows if r["family"] == "states"), key=lambda r: int(r["rows"]))
    x = [int(r["rows"]) for r in points]
    figure, axes, path = panels(PLOTS / "yosys_growth.png", "Ours against Yosys as states are added (2 inputs, 4 outputs)", 4, 14.0)
    measures = (("Gates", "ours_gates", "yosys_gates"), ("Longest path (gates)", "ours_depth", "yosys_depth"),
                ("Time (s)", "ours_s", "yosys_s"), ("Peak memory (MB)", "ours_mb", "yosys_mb"))
    for axis, (title, ours_key, yosys_key) in zip(axes, measures):
        ours = [float(r[ours_key]) for r in points]
        yosys = [float(r[yosys_key]) for r in points]
        line(axis, x, ours, BLUE, "Ours")
        line(axis, x, yosys, ORANGE, "Yosys")
        style(axis, title, "Rows")
        axis.set_xlim(right=x[-1] * 2.4)      # room for the labels at the line ends
        label_end(axis, x[-1], ours[-1], "Ours")
        label_end(axis, x[-1], yosys[-1], "Yosys")
    axes[0].legend(frameon=False, labelcolor=INK, loc="upper left")
    finish(figure, path)


def yosys_ratios(rows):
    """How far apart the two tools are on every comparable FSM."""
    comparable = [r for r in rows if r["status"] == "ok"]
    measures = (("Time: Yosys ÷ ours", [float(r["yosys_over_ours_time"]) for r in comparable], BLUE),
                ("Memory: Yosys ÷ ours", [float(r["yosys_mb"]) / float(r["ours_mb"]) for r in comparable], BLUE),
                ("Gates: ours ÷ Yosys", [float(r["gate_ratio"]) for r in comparable], ORANGE),
                ("Longest path: ours ÷ Yosys", [float(r["depth_ratio"]) for r in comparable], ORANGE))
    figure, axis = plt.subplots(figsize=(10, 4.2), constrained_layout=True)
    figure.suptitle(f"How many times apart the two tools are, on {len(comparable)} FSMs of up to 4,096 rows",
                    x=0.01, ha="left", fontsize=13, color=INK)
    for row, (_, values, color) in enumerate(measures):
        ordered = sorted(values)
        spread = [row + 0.22 * ((i % 5) - 2) / 2 for i in range(len(ordered))]
        axis.scatter(ordered, spread, s=44, color=color, edgecolor="white", linewidth=0.8, zorder=3)
        middle = statistics.median(values)
        axis.plot([middle, middle], [row - 0.34, row + 0.34], color=INK, linewidth=2, zorder=4)
        axis.annotate(f"median {middle:.1f}×", (middle, row - 0.34), xytext=(0, 3), textcoords="offset points",
                      ha="center", color=INK, fontsize=9)
    axis.axvline(1, color=MUTED, linewidth=1)
    axis.set_yticks(range(len(measures)), [m[0] for m in measures])
    axis.invert_yaxis()
    style(axis, "", "Times apart, log scale (1× means equal)", log_y=False, category_axis="y")
    axis.set_xlim(0.8, 400)
    ticks = [1, 2, 5, 10, 20, 50, 100, 200]
    axis.xaxis.set_major_locator(FixedLocator(ticks))
    axis.xaxis.set_major_formatter(FuncFormatter(lambda value, _: f"{value:g}×"))
    axis.xaxis.set_minor_locator(NullLocator())
    axis.grid(False, axis="y")
    axis.tick_params(axis="y", colors=INK, length=0)
    handles = [axis.scatter([], [], s=44, color=BLUE), axis.scatter([], [], s=44, color=ORANGE),
               axis.plot([], [], color=INK, linewidth=2)[0]]
    axis.legend(handles, ["Ours uses less", "Yosys uses less", "Median"], frameon=False, labelcolor=INK,
                ncol=3, loc="lower center", bbox_to_anchor=(0.5, 1.03))
    finish(figure, PLOTS / "yosys_ratios.png")


def end_to_end(rows):
    """Where the time goes once the netlist is used by the other tools."""
    rows = sorted(rows, key=lambda r: (int(r["rows"]), r["name"]))
    figure, axis = plt.subplots(figsize=(10, 4.6), constrained_layout=True)
    figure.suptitle("Later steps cost more than synthesis, most of all with many inputs or outputs",
                    x=0.01, ha="left", fontsize=13, color=INK)
    steps = (("Synthesize", "synth_s", BLUE, "o"), ("Check netlist", "check_s", ORANGE, "s"),
             ("Simulation run", "sim_s", VIOLET, "^"))
    labels = []
    for y, row in enumerate(rows):
        labels.append(f"{END_TO_END_NAMES[row['name']]}: {int(row['rows']):,} rows, {int(row['cycles']):,} cycles")
        values = [float(row[key]) for _, key, _, _ in steps]
        axis.plot([min(values), max(values)], [y, y], color=GRID, linewidth=2, zorder=1)
    for name, key, color, marker in steps:
        axis.scatter([float(r[key]) for r in rows], range(len(rows)), s=64, color=color, marker=marker,
                     edgecolor="white", linewidth=0.8, zorder=3, label=name)
    axis.set_yticks(range(len(rows)), labels)
    axis.invert_yaxis()
    style(axis, "", "Seconds, log scale", log_y=False, category_axis="y")
    axis.grid(False, axis="y")
    axis.tick_params(axis="y", colors=INK, length=0)
    axis.legend(frameon=False, labelcolor=INK, ncol=3, loc="lower center", bbox_to_anchor=(0.5, 1.0))
    finish(figure, PLOTS / "end_to_end.png")


def capacity(current, repeats, limits):
    """Time and memory up to the point where the memory limit stops the run."""
    points = {int(r["rows"]): (float(r["run_s"]), int(r["rss_kb"]) / 1024) for r in current if r["family"] == "states"}
    for r in repeats:
        if r["name"].startswith("states_"):
            points[int(r["rows"])] = (float(r["time_median_s"]), float(r["memory_median_mb"]))
    finished = [r for r in limits if r["status"] == "ok"][0]
    failed = [r for r in limits if r["status"] != "ok"][0]
    points[int(finished["rows"])] = (float(finished["time_median_s"]), float(finished["rss_mb"]))
    x = sorted(points)
    limit_mb = float(finished["memory_limit_gb"]) * MB_PER_GB
    failed_rows = int(failed["rows"])

    figure, axes, path = panels(PLOTS / "capacity.png", "Growth up to the memory limit, adding states (2 inputs, 4 outputs)", 2, 10.0)
    line(axes[0], x, [points[n][0] for n in x], BLUE)
    style(axes[0], "Time (s)", "Rows")
    line(axes[1], x, [points[n][1] for n in x], BLUE)
    axes[1].axhline(limit_mb, color=MUTED, linewidth=1, linestyle="--")
    axes[1].scatter([failed_rows], [limit_mb], s=70, marker="x", color=ORANGE, linewidth=2.5, zorder=3)
    style(axes[1], "Peak memory (MB)", "Rows")
    axes[1].annotate(f"{failed_rows:,} rows did not finish\nunder the {finished['memory_limit_gb']} GB limit", (failed_rows, limit_mb),
                     xytext=(failed_rows, 45), ha="right", va="top", color=INK, fontsize=9,
                     arrowprops={"arrowstyle": "-", "color": MUTED, "linewidth": 1, "relpos": (1, 1)})
    axes[1].annotate(f"{finished['memory_limit_gb']} GB limit", (x[0], limit_mb), xytext=(0, -13), textcoords="offset points",
                     color=INK, fontsize=9)
    finish(figure, path)


def main():
    PLOTS.mkdir(exist_ok=True)
    yosys = read(RESULTS / "yosys_compare.csv")
    yosys_growth(yosys)
    yosys_ratios(yosys)
    end_to_end(read(RESULTS / "end_to_end.csv"))
    capacity(read(CURRENT), read(RESULTS / "repeat_summary.csv"), read(RESULTS / "capacity.csv"))
    print(f"wrote graphs to {PLOTS}")


if __name__ == "__main__":
    main()

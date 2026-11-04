#!/usr/bin/env python3
"""Turns one folder's results.csv into graphs and slopes.csv in its plots/ folder.

    benchmark/synthesizer/plot.py                      plots results/current/
    benchmark/synthesizer/plot.py before_memory_fix    plots a saved run
    benchmark/synthesizer/plot.py --compare before_memory_fix
                                                       also draws current against a saved run

Exits 1 if a compared run built netlists of a different size on any FSM, because
then the two are not doing the same work. Needs matplotlib.
"""
import argparse
import csv
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter, LogLocator, NullFormatter

HERE = Path(__file__).resolve().parent
RESULTS = HERE / "results"

BLUE, ORANGE = "#2a78d6", "#eb6834"
INK, MUTED, GRID = "#1f1f1d", "#6b6a66", "#e4e3df"
NOISY_S = 0.1       # shorter runs are noise, so slopes leave them out

NUMBERS = ("states", "inputs", "outputs", "rows", "output_ones", "gates", "and_gates", "or_gates", "not_gates",
           "flip_flops", "depth", "rss_kb", "netlist_kb")

TIME = ("Time (s)", lambda row: row["run_s"])
MEMORY = ("Peak memory (MB)", lambda row: row["rss_kb"] / 1024)
GATES = ("Gates", lambda row: row["gates"])
DEPTH = ("Longest path (gates)", lambda row: row["depth"])


def load(path):
    rows = []
    with path.open() as f:
        for row in csv.DictReader(f):
            if not row["run_s"]:    # went over the memory limit
                continue
            parsed = {**row, "run_s": float(row["run_s"])}
            parsed.update({name: int(row[name]) for name in NUMBERS})
            rows.append(parsed)
    if not rows:
        sys.exit(f"no results in {path}")
    return rows


def of_family(rows, family):
    return [row for row in rows if row["family"] == family]


def compact(value, _=None):
    """1500 -> 1.5k, so the long axes of the largest runs stay readable. Exact
    powers of two such as 1024 stay as they are."""
    for limit, suffix in ((1e6, "M"), (1e3, "k")):
        if abs(value) >= limit and value % (limit / 10) == 0:
            return f"{value / limit:g}{suffix}"
    return f"{value:g}"


def number_axis(ticks, is_log, span):
    if is_log and span < 100:       # a short log axis would show only one or two labels
        ticks.set_major_locator(LogLocator(subs=(1, 2, 5)))
    ticks.set_major_formatter(FuncFormatter(compact))
    ticks.set_minor_formatter(NullFormatter())


def style(axis, title, xlabel, log_x=True, log_y=True, category_axis=None):
    """The panel title names the measure, so the y axis needs no label of its own."""
    if log_x:
        axis.set_xscale("log")
    if log_y:
        axis.set_yscale("log")
    if category_axis != "x":
        low, high = axis.get_xlim()
        number_axis(axis.xaxis, log_x, high / low if log_x else 0)
    if category_axis != "y":
        low, high = axis.get_ylim()
        number_axis(axis.yaxis, log_y, high / low if log_y else 0)
    axis.set_title(title, loc="left", fontsize=11, color=INK)
    axis.set_xlabel(xlabel, color=MUTED)
    axis.tick_params(colors=MUTED)
    axis.grid(True, which="major", color=GRID, linewidth=0.7)
    axis.set_axisbelow(True)
    axis.spines[["top", "right"]].set_visible(False)
    axis.spines[["left", "bottom"]].set_color(GRID)


def line(axis, x, y, color, label=None):
    axis.plot(x, y, color=color, linewidth=2, marker="o", markersize=5, label=label)


def panels(path, title, count, width=12.0):
    figure, axes = plt.subplots(1, count, figsize=(width, 3.8), constrained_layout=True)
    figure.suptitle(title, x=0.01, ha="left", fontsize=13, color=INK)
    return figure, axes, path


def finish(figure, path):
    figure.savefig(path, dpi=150)
    plt.close(figure)


def rows_graph(rows, plots):
    """The same row counts reached in two ways: more states, or more inputs."""
    figure, axes, path = panels(plots / "rows.png", "Growth with rows, reached by adding states or inputs", 3)
    series = (("states", "More states", BLUE), ("inputs_full", "More inputs", ORANGE))
    for axis, (label, value) in zip(axes, (TIME, MEMORY, DEPTH)):
        for family, name, color in series:
            points = sorted(of_family(rows, family), key=lambda row: row["rows"])
            line(axis, [row["rows"] for row in points], [value(row) for row in points], color, name)
        style(axis, label, "Rows")
    axes[0].legend(frameon=False, labelcolor=INK)
    finish(figure, path)


def single_sweep(rows, plots, filename, title, family, field, xlabel, measures, log_x=True, log_y=True):
    points = sorted(of_family(rows, family), key=lambda row: field(row))
    figure, axes, path = panels(plots / filename, title, len(measures))
    for axis, (label, value) in zip(axes, measures):
        line(axis, [field(row) for row in points], [value(row) for row in points], BLUE)
        style(axis, label, xlabel, log_x, log_y)
        if log_x:
            axis.set_xticks([field(row) for row in points])
        if not log_y:
            axis.set_ylim(bottom=0)
    finish(figure, path)


def unreachable_graph(rows, plots):
    """Each fraction starts from one table, measured as it is and with those states removed."""
    points = of_family(rows, "unreachable")
    fractions = sorted({row["value"] for row in points}, key=float)
    figure, axes, path = panels(plots / "unreachable.png", "Cost of keeping unreachable states", 2, width=9.5)
    width = 0.38
    for axis, (label, value) in zip(axes, (GATES, TIME)):
        for offset, variant, name, color in ((-width / 2, "kept", "Kept", ORANGE), (width / 2, "removed", "Removed", BLUE)):
            chosen = {row["value"]: value(row) for row in points if row["variant"] == variant}
            axis.bar([n + offset for n in range(len(fractions))], [chosen[f] for f in fractions], width * 0.92,
                     color=color, label=name)
        style(axis, label, "Share of states made unreachable", False, False, "x")
        axis.set_xticks(range(len(fractions)), [f"{float(f):.0%}" for f in fractions])
        axis.grid(False, axis="x")
    handles, names = axes[0].get_legend_handles_labels()
    figure.legend(handles, names, frameon=False, labelcolor=INK, loc="upper right", ncols=2)
    finish(figure, path)


def comparison_graph(rows, plots):
    """Gates only: run times here are all near the noise level."""
    figure, axes, path = panels(plots / "structure_and_order.png", "Gates by kind of FSM and by row order", 2, width=9.5)
    for axis, family, title in ((axes[0], "structure", "Kind of FSM (1,024 states)"),
                                (axes[1], "row_order", "Row order (256 states)")):
        points = of_family(rows, family)
        names = [row["value"].replace("_", " ") for row in points]
        bars = axis.barh(names, [row["gates"] for row in points], 0.6, color=BLUE)
        axis.bar_label(bars, [f"{row['gates']:,}" for row in points], padding=4, color=INK, fontsize=9)
        style(axis, title, "Gates", False, False, "y")
        axis.margins(x=0.18)
        axis.grid(False, axis="y")
        axis.invert_yaxis()
    finish(figure, path)


def same_netlists(rows, other):
    """Prints and returns the FSMs where the two runs built netlists of a different size."""
    fields = ("gates", "flip_flops", "depth")
    earlier = {row["name"]: row for row in other}
    different = [row["name"] for row in rows if row["name"] not in earlier
                 or any(row[field] != earlier[row["name"]][field] for field in fields)]
    print(f"netlist sizes compared on {len(rows)} FSMs, {len(different)} differ")
    for name in different:
        print("  DIFFERENT", name)
    return not different


def compare_graph(rows, other, other_name, plots):
    """The sweeps that reach large sizes: one column each, time above and memory below."""
    sweeps = (("states", "rows", "Rows, adding states"), ("inputs_full", "rows", "Rows, adding inputs"),
              ("inputs_sparse", "inputs", "Inputs, at 4,096 rows"), ("outputs", "outputs", "Outputs, at 4,096 rows"))
    figure, axes = plt.subplots(2, len(sweeps), figsize=(14, 6.6), constrained_layout=True)
    figure.suptitle(f"This run against {other_name.replace('_', ' ')}", x=0.01, ha="left", fontsize=13, color=INK)
    for column, (family, field, xlabel) in zip(axes.T, sweeps):
        for axis, (label, value) in zip(column, (TIME, MEMORY)):
            for run, name, color in ((other, "Before", ORANGE), (rows, "After", BLUE)):
                points = sorted(of_family(run, family), key=lambda row: row[field])
                line(axis, [row[field] for row in points], [value(row) for row in points], color, name)
            style(axis, label, xlabel)
    handles, names = axes[0][0].get_legend_handles_labels()
    figure.legend(handles, names, frameon=False, labelcolor=INK, loc="upper right", ncols=2)
    figure.savefig(plots / f"compare_{other_name}.png", dpi=150)
    plt.close(figure)


def write_slopes(rows, plots):
    """1 means the value grows in step with size, 2 means quadratic."""
    sweeps = (("states", "rows"), ("inputs_full", "rows"), ("inputs_sparse", "inputs"), ("outputs", "outputs"))
    measures = (("time", lambda row: row["run_s"]), ("memory", lambda row: row["rss_kb"]),
                ("gates", lambda row: row["gates"]), ("depth", lambda row: row["depth"]))
    with (plots / "slopes.csv").open("w", newline="") as f:
        writer = csv.writer(f, lineterminator="\n")
        writer.writerow(["family", "against", "time_points", "time", "memory", "gates", "depth"])
        for family, against in sweeps:
            points = of_family(rows, family)
            timed = [row for row in points if row["run_s"] >= NOISY_S]
            values = []
            for name, value in measures:
                chosen = timed if name == "time" else points
                if len(chosen) < 2:
                    values.append("")
                    continue
                values.append(round(float(np.polyfit(np.log([row[against] for row in chosen]),
                                                     np.log([value(row) for row in chosen]), 1)[0]), 2))
            writer.writerow([family, against, len(timed), *values])


def main():
    parser = argparse.ArgumentParser(description="Draw graphs from one benchmark run.")
    parser.add_argument("folder", nargs="?", default="current", help="folder under results/")
    parser.add_argument("--compare", metavar="FOLDER", help="saved run under results/ to draw this one against")
    args = parser.parse_args()

    folder = RESULTS / args.folder
    results = folder / "results.csv"
    if not results.is_file():
        sys.exit(f"no {results} (run run.py)")
    rows = load(results)
    plots = folder / "plots"
    plots.mkdir(exist_ok=True)

    three = (TIME, MEMORY, GATES)
    rows_graph(rows, plots)
    single_sweep(rows, plots, "inputs.png", "Growth with inputs, at a fixed 4,096 rows", "inputs_sparse",
                 lambda row: row["inputs"], "Inputs", three)
    single_sweep(rows, plots, "outputs.png", "Growth with outputs, at a fixed 4,096 rows", "outputs",
                 lambda row: row["outputs"], "Outputs", three)
    single_sweep(rows, plots, "output_ones.png", "Effect of how many output bits are 1", "output_ones",
                 lambda row: float(row["value"]) * 100, "Output bits that are 1 (%)", (TIME, GATES, DEPTH), False, False)
    single_sweep(rows, plots, "fill.png", "Effect of leaving rows out of the table", "fill",
                 lambda row: row["rows"], "Rows present (of 16,384)", (TIME, MEMORY, GATES), False, False)
    unreachable_graph(rows, plots)
    comparison_graph(rows, plots)
    write_slopes(rows, plots)
    same = True
    if args.compare:
        other = load(RESULTS / args.compare / "results.csv")
        same = same_netlists(rows, other)
        compare_graph(rows, other, args.compare, plots)
    print(f"graphs in {plots}")
    sys.exit(0 if same else 1)


if __name__ == "__main__":
    main()

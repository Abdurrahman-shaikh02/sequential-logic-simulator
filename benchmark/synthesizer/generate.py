#!/usr/bin/env python3
"""Writes every benchmark FSM to fsms/, as one .net file each.

    benchmark/synthesizer/generate.py            everything
    benchmark/synthesizer/generate.py --quick    small sizes, to try the scripts

Rows, states and inputs are tied together, because a full table has
rows = states * 2^inputs. So each family changes one thing and holds the others
still. Fixed seeds make every file identical on each run, so the files are not
committed.
"""
import argparse
import csv
import random
import shutil
from pathlib import Path

HERE = Path(__file__).resolve().parent
FSMS = HERE / "fsms"

BASE = dict(states=256, inputs=4, outputs=4)    # 4096 rows when full


def input_bits(value, width):
    return format(value, f"0{width}b")


def table_text(table, inputs, outputs, reset="s0"):
    """`table` is [(state, input bits, next state, output bits)]."""
    lines = ["inputs: " + " ".join(f"i{n}" for n in range(inputs)),
             "outputs: " + " ".join(f"o{n}" for n in range(outputs)),
             f"reset: {reset}"]
    lines += [f"{state} | {bits} | {following} | {out}" for state, bits, following, out in table]
    return "\n".join(lines) + "\n"


def random_outputs(rng, outputs, ones):
    """Exact at 0 and 1, so those two ends of the sweep are not affected by chance."""
    if ones in (0, 1):
        return str(ones) * outputs
    if ones == 0.5:
        return input_bits(rng.getrandbits(outputs), outputs)
    return "".join("1" if rng.random() < ones else "0" for _ in range(outputs))


def random_table(rng, states, inputs, outputs, fill=1.0, ones=0.5):
    names = [f"s{n}" for n in range(states)]
    return [(state, input_bits(value, inputs), rng.choice(names), random_outputs(rng, outputs, ones))
            for state in names for value in range(1 << inputs) if fill == 1 or rng.random() < fill]


def sparse_table(rng, states, inputs, outputs, rows):
    """`rows` distinct rows with random input values, for FSMs with many inputs."""
    names = [f"s{n}" for n in range(states)]
    keys = set()
    while len(keys) < rows:
        keys.add((rng.choice(names), input_bits(rng.getrandbits(inputs), inputs)))
    return [(state, bits, rng.choice(names), random_outputs(rng, outputs, 0.5)) for state, bits in sorted(keys)]


def structured_table(rng, kind, states, inputs, outputs):
    """Real designs are more regular than random tables, which may change the gates."""
    table = []
    for state in range(states):
        out = input_bits(state % (1 << outputs), outputs)       # depends only on the state
        for value in range(1 << inputs):
            if kind == "ring":
                following = state + 1
            elif kind == "counter":
                following = state + 1 if value else state
            else:       # "two_way": a binary tree of states, as a sequence detector has
                following = state * 2 + 1 + (value & 1)
            table.append((f"s{state}", input_bits(value, inputs), f"s{following % states}", out))
    return table


def unreachable_table(rng, states, inputs, outputs, fraction):
    """The last `fraction` of the states can never be entered from the others."""
    normal = states - int(states * fraction)
    table = []
    for state in range(states):
        for value in range(1 << inputs):
            following = rng.randrange(normal if state < normal else states)
            table.append((f"s{state}", input_bits(value, inputs), f"s{following}",
                          random_outputs(rng, outputs, 0.5)))
    return table


def without_unreachable(table, reset="s0"):
    successors = {}
    for state, _, following, _ in table:
        successors.setdefault(state, []).append(following)
    reached, todo = {reset}, [reset]
    while todo:
        for following in successors.get(todo.pop(), []):
            if following not in reached:
                reached.add(following)
                todo.append(following)
    return [row for row in table if row[0] in reached]


def build_cases(quick):
    """(family, value on the x axis, variant, seed text, function that makes the table, inputs, outputs)."""
    states_values = (16, 64, 256, 1024) if quick else (16, 64, 256, 1024, 4096, 16384, 65536)
    inputs_full = (2, 4, 6) if quick else (2, 4, 6, 8, 10, 12, 14)
    inputs_sparse = (8, 16) if quick else (8, 16, 32, 64, 128, 256)
    outputs_values = (1, 4, 16) if quick else (1, 4, 16, 64, 256, 1024)

    def case(family, value, build, inputs, outputs, variant=""):
        return family, value, variant, build, inputs, outputs

    s, i, o = BASE["states"], BASE["inputs"], BASE["outputs"]
    cases = []
    # rows grow with the states; 2 inputs and 4 outputs stay still
    for n in states_values:
        cases.append(case("states", n, lambda r, n=n: random_table(r, n, 2, o), 2, o))
    # rows grow with the inputs too, because the table is full
    for n in inputs_full:
        cases.append(case("inputs_full", n, lambda r, n=n: random_table(r, 16, n, o), n, o))
    # rows stay still, so only the width of each row changes
    for n in inputs_sparse:
        cases.append(case("inputs_sparse", n, lambda r, n=n: sparse_table(r, s, n, o, 4096), n, o))
    for n in outputs_values:
        cases.append(case("outputs", n, lambda r, n=n: random_table(r, s, i, n), i, n))
    for ones in (0, 0.1, 0.25, 0.5, 0.75, 1):
        cases.append(case("output_ones", ones, lambda r, x=ones: random_table(r, s, i, 16, ones=x), i, 16))
    for fill in (0.1, 0.25, 0.5, 0.75, 1):
        cases.append(case("fill", fill, lambda r, x=fill: random_table(r, s, 6, o, fill=x), 6, o))
    for kind in ("random", "ring", "counter", "two_way"):
        build = (lambda r: random_table(r, 1024, 2, o)) if kind == "random" else (
            lambda r, k=kind: structured_table(r, k, 1024, 2, o))
        cases.append(case("structure", kind, build, 2, o))
    for order in ("sorted", "shuffled"):
        def ordered(r, order=order):
            table = random_table(r, s, i, o)
            if order == "shuffled":
                r.shuffle(table)
            return table
        cases.append(case("row_order", order, ordered, i, o))
    for fraction in (0, 0.25, 0.5, 0.75):
        build = lambda r, x=fraction: unreachable_table(r, 1024, 2, o, x)
        cases.append(case("unreachable", fraction, build, 2, o, "kept"))
        cases.append(case("unreachable", fraction, lambda r, b=build: without_unreachable(b(r)), 2, o, "removed"))
    return cases


def main():
    parser = argparse.ArgumentParser(description="Write the benchmark FSMs.")
    parser.add_argument("--quick", action="store_true", help="small sizes, to try the scripts")
    args = parser.parse_args()

    shutil.rmtree(FSMS, ignore_errors=True)
    FSMS.mkdir()
    rows = []
    for family, value, variant, build, inputs, outputs in build_cases(args.quick):
        name = f"{family}_{value}" + (f"_{variant}" if variant else "")
        # The variants of a value share a seed, so they start from the same table
        table = build(random.Random(f"{family}/{value}"))
        path = FSMS / f"{name}.net"
        path.write_text(table_text(table, inputs, outputs))
        states = {row[0] for row in table} | {row[2] for row in table} | {"s0"}
        one_bits = sum(row[3].count("1") for row in table)
        rows.append({"name": name, "family": family, "value": value, "variant": variant, "file": str(path),
                     "states": len(states), "inputs": inputs, "outputs": outputs, "rows": len(table),
                     "output_ones": one_bits, "file_kb": path.stat().st_size // 1024})
        print(f"{name:28} {len(table):8} rows", flush=True)

    with (FSMS / "manifest.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    print(f"{len(rows)} cases in {FSMS}")


if __name__ == "__main__":
    main()

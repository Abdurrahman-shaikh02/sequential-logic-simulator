#!/usr/bin/env python3
"""Times our simulator on every case in ../generated/ and writes results.csv.

    make
    benchmark/simulation/hdl_sim/run.py
    benchmark/simulation/hdl_sim/run.py --sim path/to/simulator

The simulator is driven through its Worklet 3 interface, `simulator
--interactive`: stdin gives the netlist path and an input-vectors file, and the
run writes its results table, waveform, JSON and VCD to output/. Those files are
part of every timed run, because the interface cannot turn them off.

The .st stimulus from generate.py is converted to a vectors file first, one
column per input bit, untimed. The results table of the last timed run is
turned into the same text the Icarus testbench prints, and its hash is saved
for plot.py to compare.
"""
import argparse
import re
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]))
from measure import REPO, measure, output_hash, read_cases, runs_for, write_results


def parse_port(entry):
    """'d[3:0]' -> ('d', 4), 'clk' -> ('clk', 1)"""
    match = re.fullmatch(r"(\w+)(?:\[(\d+):0\])?", entry)
    return match[1], int(match[2] or 0) + 1


def bit_label(name, width, bit):
    return name if width == 1 else f"{name}[{bit}]"


def write_vectors(path, stimulus):
    """One 0/1 value per input bit. The simulator lists its inputs sorted by
    name and then by bit index, and a vectors file uses that column order."""
    lines = Path(stimulus).read_text().splitlines()
    inputs = [parse_port(entry) for entry in lines[0].split(":", 1)[1].split()]
    columns = sorted((name, bit) for position, (name, width) in enumerate(inputs) for bit in range(width))
    order = {name: position for position, (name, _) in enumerate(inputs)}
    widths = dict(inputs)

    with path.open("w") as f:
        f.write("# inputs: " + " ".join(bit_label(name, widths[name], bit) for name, bit in columns) + "\n")
        for line in lines[1:]:
            values = line.split()
            f.write(" ".join(values[order[name]][widths[name] - 1 - bit] for name, bit in columns) + "\n")


def read_outputs(results, output_entry):
    """The results table has one column per bit. Returns the header and one
    line per row, the output written most significant bit first."""
    lines = results.read_text().splitlines()
    header = lines[0].split("\t")
    name, width = parse_port(output_entry)
    columns = [header.index(bit_label(name, width, bit)) for bit in reversed(range(width))]
    return [f"// outputs: {output_entry}"] + ["".join(line.split("\t")[i] for i in columns) for line in lines[1:]]


def run_case(sim, case):
    with tempfile.TemporaryDirectory() as work:
        work = Path(work)
        write_vectors(work / "vectors", case["stimulus"])
        (work / "answers").write_text(f"{Path(case['table']).resolve()}\n{work / 'vectors'}\n")
        seconds, memory = measure([sim, "--interactive"], runs_for(case["cells"]),
                                  stdin=work / "answers", cwd=work)
        outputs = read_outputs(work / "output" / "interactive_results.txt", case["output"])
    if len(outputs) != int(case["rows"]) + 1:
        raise RuntimeError(f"{case['name']}: {len(outputs) - 1} result rows, expected {case['rows']}")
    return {"compile_s": 0, "run_s": round(seconds, 3), "rss_kb": memory,
            "output_sha256": output_hash(outputs)}


def main():
    parser = argparse.ArgumentParser(description="Time our simulator on the benchmark circuits.")
    parser.add_argument("--sim", default=str(REPO / "simulator"), help="simulator binary")
    args = parser.parse_args()
    if not Path(args.sim).is_file():
        sys.exit(f"no simulator at {args.sim} (run make)")
    sim = str(Path(args.sim).resolve())

    cases = read_cases(HERE.parent / "generated")
    write_results(HERE / "results.csv", cases, lambda case: run_case(sim, case))


if __name__ == "__main__":
    main()

"""Shared by the scripts in this folder: reading FSM tables, measuring netlist
size, and checking a netlist in the real simulator against its FSM table."""
import csv
import random
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent.parent))
from measure import REPO, measure

SIM = str(REPO / "simulator")
FSMS = HERE.parent / "fsms"
GENERATED = HERE / "generated"      # not committed: large FSMs and netlists
RESULTS = HERE / "results"
GB = 1 << 30


def limited(command, memory_gb, timeout_s):
    """A command with a time limit and a memory limit, so one case fails alone."""
    return ["timeout", str(timeout_s), "prlimit", f"--as={int(memory_gb * GB)}"] + command


def write_csv(path, rows, fields=None):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fields or list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def spread(values):
    return min(values), statistics.median(values), max(values)


class Fsm:
    def __init__(self, path):
        self.rows = {}          # state -> {input bits: (next state, output bits)}
        self.inputs, self.outputs, self.reset = [], [], None
        for line in Path(path).read_text().splitlines():
            line = line.split("#")[0].strip()
            if "|" in line:
                current, bits, following, out = (part.strip() for part in line.split("|"))
                self.rows.setdefault(current, {})["".join(bits.split())] = (following, "".join(out.split()))
            elif ":" in line:
                key, _, values = line.partition(":")
                if key.strip() == "inputs":
                    self.inputs = values.split()
                elif key.strip() == "outputs":
                    self.outputs = values.split()
                elif key.strip() == "reset":
                    self.reset = values.split()[0]

    def expected(self, state, bits, reset):
        """(next state, output bits). A missing row goes to reset with zero
        outputs. Reset changes the next state, not the outputs."""
        row = self.rows.get(state, {}).get(bits)
        out = row[1] if row else "0" * len(self.outputs)
        return (self.reset if reset or not row else row[0]), out


def synthesize(fsm_path, out_path, memory_gb=7, timeout_s=1800):
    command = limited([SIM, "--fsm", str(fsm_path), "--synth", "-o", str(out_path)], memory_gb, timeout_s)
    return measure(command)


def netlist_stats(path):
    """Gates, flip-flops and longest path, in any cell order, so it also reads
    Yosys netlists. The file must hold only 2-input gates, NOT and $_DFF_P_."""
    cells = {}
    with open(path) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            _, cell, kind, port, direction, wire = line.rstrip("\n").split("\t")
            if kind != "-":
                cells.setdefault(cell, [kind, {}])[1][port] = wire
    unknown = {kind for kind, _ in cells.values()} - {"$_AND_", "$_OR_", "$_NOT_", "$_DFF_P_"}
    if unknown:
        raise RuntimeError(f"{path}: unexpected cells {unknown}")

    driver = {ports["Y"]: [ports[p] for p in ("A", "B") if p in ports]
              for kind, ports in cells.values() if kind != "$_DFF_P_"}
    depth = {}
    for start in driver:
        stack = [start]
        while stack:
            wire = stack[-1]
            if wire in depth:
                stack.pop()
                continue
            waiting = [w for w in driver[wire] if w in driver and w not in depth]
            if waiting:
                stack.extend(waiting)
            else:
                depth[wire] = 1 + max((depth.get(w, 0) for w in driver[wire]), default=0)
                stack.pop()
    flip_flops = sum(kind == "$_DFF_P_" for kind, _ in cells.values())
    return {"gates": len(cells) - flip_flops, "flip_flops": flip_flops, "depth": max(depth.values(), default=0)}


def walk(fsm, cycles, seed):
    """A random run through the table, two simulator rows per clock cycle: new
    inputs with clk low, then the same inputs with clk high. Mostly follows
    listed rows, because a random value on a wide FSM would nearly always miss."""
    rng = random.Random(seed)
    columns = sorted(fsm.inputs + ["clk", "reset"])
    width = len(fsm.inputs)
    state = fsm.reset
    rows, visited, used = [], {state}, []
    for cycle in range(cycles):
        reset = cycle == 0 or rng.random() < 0.02
        listed = fsm.rows.get(state)
        if listed and rng.random() < 0.95:
            bits = rng.choice(list(listed))
        else:
            bits = format(rng.getrandbits(width), f"0{width}b") if width else ""
        following, out = fsm.expected(state, bits, reset)
        values = dict(zip(fsm.inputs, bits), reset=str(int(reset)))
        rows.append(({**values, "clk": "0"}, out))
        if not reset and listed and bits in listed:
            used.append((state, bits))
        state = following
        visited.add(state)
        rows.append(({**values, "clk": "1"}, fsm.expected(state, bits, reset)[1]))
    return columns, rows, len(visited), used


def compare_outputs(results_path, fsm, rows):
    """Number of output values that differ from the table. Also confirms that
    the simulator applied the inputs we meant, so a wrong column order fails."""
    lines = Path(results_path).read_text().splitlines()
    header = lines[0].split("\t")
    table = [line.split("\t") for line in lines[1:] if line.strip()]
    if len(table) != len(rows):
        raise RuntimeError(f"{len(table)} result rows, expected {len(rows)}")
    where = {name: i for i, name in enumerate(header)}
    mismatches = 0
    for got, (inputs, out) in zip(table, rows):
        for name, value in inputs.items():
            if got[where[name]] != value:
                raise RuntimeError(f"input {name} was not applied as written")
        mismatches += sum(got[where[name]] != bit for name, bit in zip(fsm.outputs, out))
    return mismatches


def simulate_and_compare(table_path, fsm, cycles, seed=1, memory_gb=7, timeout_s=1800):
    """Runs the netlist in `simulator --interactive` on a random walk. Returns
    its time, memory and the comparison with the table."""
    columns, rows, visited, _ = walk(fsm, cycles, seed)
    GENERATED.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=GENERATED) as work:
        work = Path(work)
        with open(work / "vectors", "w") as f:
            f.write("# inputs: " + " ".join(columns) + "\n")
            for values, _ in rows:
                f.write(" ".join(values[name] for name in columns) + "\n")
        (work / "answers").write_text(f"{Path(table_path).resolve()}\n{work / 'vectors'}\n")
        seconds, memory = measure(limited([SIM, "--interactive"], memory_gb, timeout_s),
                                  stdin=work / "answers", cwd=work)
        mismatches = compare_outputs(work / "output" / "interactive_results.txt", fsm, rows)
    return {"sim_s": round(seconds, 3), "sim_mb": round(memory / 1024, 1), "cycles": cycles,
            "states_visited": visited, "outputs_compared": len(rows) * len(fsm.outputs), "mismatches": mismatches}

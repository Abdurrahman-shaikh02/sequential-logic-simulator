#!/usr/bin/env python3
"""Check that synthesized netlists behave exactly like their FSM tables.

The table is the reference. This script reads the table and the netlist itself,
sharing no code with the synthesizer, the parser or the simulator.

Each FSM gets these checks:
  structure  only the documented cells, one driver per wire, no loops, the
             expected flip-flops, and the parser accepts the netlist
  walk       from reset, every reachable state with every input and both reset
             values. A state and input always behave the same way, so this
             covers every input sequence of any length
  forced     every flip-flop value is set directly, which reaches unreachable
             states and unused codes

Then it damages netlists and tables on purpose, and the checks must notice.

    validation/synthesizer/validate.py                 run everything
    validation/synthesizer/validate.py edge tiny       only matching groups
    validation/synthesizer/validate.py --seeds 10 --mutants 100
    validation/synthesizer/validate.py --full          more tiny FSMs (minutes)

Needs a built simulator (make). Exits 1 on any failure. The FSM that failed is
saved in validation/synthesizer/out/failures/.
"""
import argparse
import itertools
import random
import subprocess
import sys
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
FSMS = HERE / "fsms"
OUT = HERE / "out"

EXHAUSTIVE_BITS = 16    # state bits + inputs + reset; wider FSMs use sampled inputs
RANDOM_INPUTS = 64      # extra random input values for a sampled FSM

# (states, inputs, outputs): every possible table of this shape is tested
TINY = [(1, 0, 0), (1, 0, 1), (1, 0, 2), (1, 1, 0), (1, 1, 1), (1, 1, 2), (1, 2, 0), (1, 2, 1),
        (2, 0, 0), (2, 0, 1), (2, 0, 2), (2, 1, 0), (2, 1, 1), (3, 0, 0), (3, 0, 1), (4, 0, 0)]
TINY_FULL = [(1, 2, 2), (2, 2, 0), (4, 0, 1)]

# name: settings for random_fsm, one FSM per entry and seed
FAMILIES = {
    "full": [dict(states=s, inputs=i, outputs=o)
             for s, i, o in ((2, 1, 1), (3, 2, 2), (5, 3, 3), (8, 2, 4), (16, 3, 8), (33, 2, 5), (64, 4, 8))],
    "sparse": [dict(states=s, inputs=i, outputs=3, fill=f)
               for s, i, f in ((4, 3, 0.2), (6, 3, 0.5), (12, 4, 0.1), (32, 4, 0.7))],
    "power_of_two": [dict(states=s, inputs=1, outputs=2)
                     for s in (2, 3, 4, 5, 8, 9, 16, 17, 32, 33, 64, 65)],
    "unreachable": [dict(states=s, inputs=2, outputs=2, unreachable=u)
                    for s, u in ((4, 1), (6, 3), (9, 1), (20, 15))],
    "traps": [dict(states=s, inputs=2, outputs=2, sinks=t, fill=0.8)
              for s, t in ((3, 1), (6, 3), (10, 9))],
    "output_density": [dict(states=8, inputs=3, outputs=6, ones=d)
                       for d in (0.02, 0.2, 0.8, 1.0)],
    "many_outputs": [dict(states=4, inputs=2, outputs=o) for o in (32, 200)],
    "many_states": [dict(states=s, inputs=1, outputs=2) for s in (300, 1024)],
    "wide": [dict(states=s, inputs=i, outputs=2, rows=r)
             for s, i, r in ((4, 20, 40), (8, 40, 60), (16, 70, 80), (3, 128, 20))],
}


class Failure(Exception):
    pass


# ---------------------------------------------------------------- FSM tables

@dataclass
class Fsm:
    inputs: list
    outputs: list
    reset: str
    states: list                # in the order that decides their codes; reset first
    rows: dict                  # (state, input bits) -> (next state, output bits)

    @property
    def bits(self):
        needed = 1
        while (1 << needed) < len(self.states):
            needed += 1
        return needed

    def expected(self, state, inputs, reset):
        """The table's answer: (next state, output bits). A missing row goes to
        reset with zero outputs. Reset changes the next state, not the outputs."""
        row = self.rows.get((state, inputs))
        outputs = row[1] if row else "0" * len(self.outputs)
        return (self.reset if reset or not row else row[0]), outputs


def parse_fsm(text):
    headers, rows = {}, {}
    states = []
    for line in text.splitlines():
        line = line.split("#")[0].strip()
        if not line:
            continue
        if "|" in line:
            current, inputs, following, outputs = (part.strip() for part in line.split("|"))
            rows[current, "".join(inputs.split())] = (following, "".join(outputs.split()))
            states.extend(s for s in (current, following) if s not in states)
        else:
            key, _, values = line.partition(":")
            headers[key.strip()] = values.split()
    reset = headers["reset"][0]
    states = [reset] + [s for s in states if s != reset]
    return Fsm(headers.get("inputs", []), headers.get("outputs", []), reset, states, rows)


def bits_of(value, width):
    return format(value, f"0{width}b") if width else ""


def random_fsm(rng, states, inputs, outputs, fill=1.0, ones=0.5, unreachable=0, sinks=0, rows=None):
    """Table text with a shuffled row order and a randomly placed reset state.

    The last `unreachable` states are never a next state of the others. `sinks`
    states loop only to themselves. `rows` picks that many random input values
    instead of trying them all, for FSMs with too many inputs to list."""
    names = [f"s{n}" for n in range(states)]
    normal = names[:states - unreachable]
    reset = rng.choice(normal)
    trapped = set(rng.sample([n for n in normal if n != reset], sinks))

    def add(table, state, values):
        following = state if state in trapped else rng.choice(normal if state in normal else names)
        table[state, values] = (following, "".join("1" if rng.random() < ones else "0" for _ in range(outputs)))

    table = {}
    if rows is not None:
        while len(table) < rows:
            add(table, rng.choice(names), bits_of(rng.getrandbits(inputs), inputs))
    else:
        for state, value in itertools.product(names, range(1 << inputs)):
            if rng.random() < fill:
                add(table, state, bits_of(value, inputs))

    entries = list(table.items())
    rng.shuffle(entries)
    packed = rng.random() < 0.5
    spread = (lambda bits: bits) if packed else " ".join
    lines = [f"inputs: {' '.join(f'i{n}' for n in range(inputs))}",
             f"outputs: {' '.join(f'o{n}' for n in range(outputs))}",
             f"reset: {reset}"]
    lines += [f"{state} | {spread(values)} | {following} | {spread(out)}"
              for (state, values), (following, out) in entries]
    return "\n".join(lines) + "\n"


def all_tables(states, inputs, outputs):
    """Every table of this shape, using all input values and all output values."""
    names = [f"s{n}" for n in range(states)]
    keys = list(itertools.product(names, range(1 << inputs)))
    choices = [None] + list(itertools.product(names, range(1 << outputs)))
    header = (f"inputs: {' '.join(f'i{n}' for n in range(inputs))}\n"
              f"outputs: {' '.join(f'o{n}' for n in range(outputs))}\nreset: s0\n")
    for picked in itertools.product(choices, repeat=len(keys)):
        yield header + "".join(
            f"{state} | {bits_of(value, inputs)} | {choice[0]} | {bits_of(choice[1], outputs)}\n"
            for (state, value), choice in zip(keys, picked) if choice)


# ------------------------------------------------------------------ netlists

@dataclass
class Netlist:
    gates: list = field(default_factory=list)   # [type, a, b or None, y]
    dffs: dict = field(default_factory=dict)    # bit -> D wire
    primary_outputs: dict = field(default_factory=dict)     # name -> wire

    def copy(self):
        return Netlist([list(gate) for gate in self.gates], dict(self.dffs), dict(self.primary_outputs))


def q_wire(bit):
    return f"$dff$q{bit}_Q"


def input_wire(name):
    return "\\" + name


def check_structure(fsm, text):
    """Reads the netlist text, rejecting anything outside the documented format."""
    primary_inputs, primary_outputs, cells = {}, {}, {}
    modules = set()
    encoding = None
    for number, line in enumerate(text.splitlines(), start=1):
        if line.startswith("# state encoding"):
            encoding = dict(entry.split("=") for entry in line.split(":", 1)[1].split("(reset")[0].split())
        if not line.strip() or line.startswith("#"):
            continue
        columns = line.split("\t")
        if len(columns) != 6:
            raise Failure(f"structure: netlist line {number} does not have 6 columns")
        module, cell, kind, port, direction, wire = columns
        modules.add(module)
        if direction == "pi":
            primary_inputs[cell] = wire
        elif direction == "po":
            primary_outputs[cell] = wire
        else:
            entry = cells.setdefault(cell, {"type": kind, "ports": {}})
            if entry["type"] != kind or port in entry["ports"]:
                raise Failure(f"structure: cell {cell} is written inconsistently")
            entry["ports"][port] = wire

    if len(modules) != 1:
        raise Failure(f"structure: expected one module, found {sorted(modules)}")
    expected_inputs = {name: input_wire(name) for name in fsm.inputs + ["clk", "reset"]}
    if primary_inputs != expected_inputs:
        raise Failure(f"structure: primary inputs are {sorted(primary_inputs)}")
    if set(primary_outputs) != set(fsm.outputs):
        raise Failure(f"structure: primary outputs are {sorted(primary_outputs)}")

    wanted_encoding = {s: bits_of(code, fsm.bits) for code, s in enumerate(fsm.states)}
    if encoding != wanted_encoding:
        raise Failure(f"structure: state encoding {encoding}, expected {wanted_encoding}")

    net = Netlist(primary_outputs=primary_outputs)
    drivers = set(primary_inputs.values())

    def drive(wire, cell):
        if wire in drivers:
            raise Failure(f"structure: wire {wire} has two drivers (second is {cell})")
        drivers.add(wire)

    ports_of = {"$_NOT_": {"A", "Y"}, "$_AND_": {"A", "B", "Y"}, "$_OR_": {"A", "B", "Y"},
                "$_DFF_P_": {"D", "C", "Q"}}
    for cell, entry in cells.items():
        kind, ports = entry["type"], entry["ports"]
        if ports_of.get(kind) != set(ports):
            raise Failure(f"structure: cell {cell} has type {kind} and ports {sorted(ports)}")
        if kind == "$_DFF_P_":
            if ports["C"] != "\\clk":
                raise Failure(f"structure: flip-flop {cell} is clocked by {ports['C']}")
            if not cell.startswith("$dff$q") or ports["Q"] != cell + "_Q":
                raise Failure(f"structure: unexpected flip-flop {cell} with Q {ports['Q']}")
            net.dffs[int(cell.removeprefix("$dff$q"))] = ports["D"]
            drive(ports["Q"], cell)
        else:
            net.gates.append([kind, ports["A"], ports.get("B"), ports["Y"]])
            drive(ports["Y"], cell)

    if sorted(net.dffs) != list(range(fsm.bits)):
        raise Failure(f"structure: flip-flops {sorted(net.dffs)}, expected {fsm.bits}")
    read = [wire for gate in net.gates for wire in gate[1:3] if wire]
    read += list(net.dffs.values()) + list(primary_outputs.values())
    for wire in read:
        if wire not in drivers:
            raise Failure(f"structure: wire {wire} has no driver")
    if "\\clk" in [wire for gate in net.gates for wire in gate[1:3]]:
        raise Failure("structure: the clock feeds a logic gate")
    unread = drivers - set(read) - {"\\clk"}
    if unread:
        raise Failure(f"structure: wires nobody reads: {sorted(unread)}")
    return net


def in_order(gates, sources):
    """Gates sorted so each comes after its inputs. Fails on a combinational loop."""
    readers, waiting = {}, []
    ready = deque()
    for index, (_, a, b, _y) in enumerate(gates):
        needed = {wire for wire in (a, b) if wire and wire not in sources}
        waiting.append(len(needed))
        for wire in needed:
            readers.setdefault(wire, []).append(index)
        if not needed:
            ready.append(index)
    order = []
    while ready:
        index = ready.popleft()
        order.append(gates[index])
        for reader in readers.get(gates[index][3], []):
            waiting[reader] -= 1
            if not waiting[reader]:
                ready.append(reader)
    if len(order) != len(gates):
        raise Failure("structure: combinational loop")
    return order


def evaluate(fsm, net, vectors):
    """Runs the gates on every vector at once: each wire is an integer holding
    one bit per vector. Returns {output name or D bit: string with one char per vector}."""
    count = len(vectors)
    mask = (1 << count) - 1

    def pattern(bit_of):
        return int("".join(bit_of(vector) for vector in reversed(vectors)), 2) if count else 0

    values = {q_wire(bit): pattern(lambda v, b=bit: str((v[0] >> b) & 1)) for bit in range(fsm.bits)}
    for position, name in enumerate(fsm.inputs):
        values[input_wire(name)] = pattern(lambda v, p=position: v[1][p])
    values["\\reset"] = pattern(lambda v: str(v[2]))
    values["\\clk"] = 0

    for kind, a, b, y in in_order(net.gates, values):
        if kind == "$_NOT_":
            values[y] = values[a] ^ mask
        elif kind == "$_AND_":
            values[y] = values[a] & values[b]
        else:
            values[y] = values[a] | values[b]

    text = lambda wire: format(values[wire], f"0{count}b")[::-1]
    results = {name: text(net.primary_outputs[name]) for name in fsm.outputs}
    results.update({bit: text(net.dffs[bit]) for bit in range(fsm.bits)})
    return results


# ------------------------------------------------------------ behaviour checks

def input_values(fsm, rng):
    """(values to try, whether that is all of them)."""
    width = len(fsm.inputs)
    if fsm.bits + width + 1 <= EXHAUSTIVE_BITS:
        return [bits_of(value, width) for value in range(1 << width)], True

    # Every listed value, and each with one bit flipped: a dropped or inverted
    # literal in a minterm changes the answer on one of these.
    chosen = set()
    for _, values in fsm.rows:
        chosen.add(values)
        for position in range(width):
            flipped = "1" if values[position] == "0" else "0"
            chosen.add(values[:position] + flipped + values[position + 1:])
    while len(chosen) < len(fsm.rows) + RANDOM_INPUTS:
        chosen.add(bits_of(rng.getrandbits(width), width))
    return sorted(chosen), False


def make_vectors(fsm, values):
    vectors = [(code, inputs, reset) for code in range(1 << fsm.bits) for inputs in values for reset in (0, 1)]
    return vectors, {vector: index for index, vector in enumerate(vectors)}


def answer(fsm, results, index):
    outputs = "".join(results[name][index] for name in fsm.outputs)
    following = sum(int(results[bit][index]) << bit for bit in range(fsm.bits))
    return following, outputs


class Mismatch(Exception):
    def __init__(self, stage, message):
        super().__init__(f"{stage}: {message}")
        self.stage = stage


def walk(fsm, results, index, values):
    """Follows the table and the circuit together from reset. Uses no encoding:
    only the outputs are compared, as an outside observer would see them."""
    start = (fsm.reset, 0)
    came_from = {start: None}
    todo = deque([start])
    checked = 0
    while todo:
        pair = todo.popleft()
        state, code = pair
        for inputs, reset in itertools.product(values, (0, 1)):
            following, outputs = fsm.expected(state, inputs, reset)
            got_code, got_outputs = answer(fsm, results, index[code, inputs, reset])
            checked += 1
            if got_outputs != outputs:
                path, step = [], pair
                while came_from[step]:
                    step, used = came_from[step]
                    path.append(used)
                raise Mismatch("walk", f"after inputs {path[::-1]}, in state {state}, inputs '{inputs}', "
                                       f"reset {reset}: outputs '{got_outputs}', table says '{outputs}'")
            successor = (following, got_code)
            if successor not in came_from:
                came_from[successor] = (pair, (inputs, reset))
                todo.append(successor)
    return checked


def forced(fsm, results, index, values):
    """Sets every flip-flop value directly. Used codes must match their state.
    Unused codes must output zeros and go to reset, like a missing row."""
    code_of = {state: code for code, state in enumerate(fsm.states)}
    checked = 0
    for code in range(1 << fsm.bits):
        state = fsm.states[code] if code < len(fsm.states) else None
        for inputs, reset in itertools.product(values, (0, 1)):
            if state is None:
                want_code, want_outputs = 0, "0" * len(fsm.outputs)
            else:
                following, want_outputs = fsm.expected(state, inputs, reset)
                want_code = code_of[following]
            got_code, got_outputs = answer(fsm, results, index[code, inputs, reset])
            checked += 1
            if (got_code, got_outputs) != (want_code, want_outputs):
                raise Mismatch("forced", f"code {bits_of(code, fsm.bits)} ({state or 'unused'}), inputs '{inputs}', "
                                         f"reset {reset}: next {bits_of(got_code, fsm.bits)}, outputs '{got_outputs}'; "
                                         f"expected next {bits_of(want_code, fsm.bits)}, outputs '{want_outputs}'")
    return checked


def check_behaviour(fsm, net, values, index, vectors):
    results = evaluate(fsm, net, vectors)
    return walk(fsm, results, index, values) + forced(fsm, results, index, values)


# ------------------------------------------------------- deliberate damage

def damaged_netlists(fsm, net, rng):
    """(description, change) for each small wiring or gate mistake. A change edits
    a copy of the netlist, so only the mistakes actually tried cost a copy."""
    made_by = {gate[3]: gate for gate in net.gates}
    inverter_of = {gate[1]: gate[3] for gate in net.gates if gate[0] == "$_NOT_"}
    sources = [q_wire(bit) for bit in range(fsm.bits)] + [input_wire(name) for name in fsm.inputs] + ["\\reset"]
    wires = sources + [gate[3] for gate in in_order(net.gates, set(sources))]
    place = {wire: position for position, wire in enumerate(wires)}

    def set_type(position, kind):
        def change(mutant):
            mutant.gates[position][0] = kind
        return change

    def set_input(position, port, wire):
        def change(mutant):
            mutant.gates[position][port] = wire
        return change

    def set_dffs(updates):
        return lambda mutant: mutant.dffs.update(updates)

    def set_outputs(updates):
        return lambda mutant: mutant.primary_outputs.update(updates)

    for position, gate in enumerate(net.gates):
        if gate[0] != "$_NOT_":
            yield f"gate type of {gate[3]}", set_type(position, "$_OR_" if gate[0] == "$_AND_" else "$_AND_")
        for port in (1, 2):
            wire = gate[port]
            if not wire:
                continue
            source = made_by.get(wire)
            flipped = source[1] if source and source[0] == "$_NOT_" else inverter_of.get(wire)
            if flipped and flipped != gate[3]:
                yield f"inverted input of {gate[3]}", set_input(position, port, flipped)
            # Only wires earlier in the order, so no loop can form
            other = wires[rng.randrange(place[gate[3]])]
            if other != wire:
                yield f"rewired input of {gate[3]} to {other}", set_input(position, port, other)
    for bit in range(fsm.bits):
        yield f"flip-flop {bit} holds its value", set_dffs({bit: q_wire(bit)})
    for first, second in itertools.combinations(range(fsm.bits), 2):
        yield f"flip-flops {first} and {second} swapped", set_dffs({first: net.dffs[second], second: net.dffs[first]})
    for first, second in itertools.combinations(fsm.outputs[:16], 2):
        if net.primary_outputs[first] != net.primary_outputs[second]:
            yield f"outputs {first} and {second} swapped", set_outputs(
                {first: net.primary_outputs[second], second: net.primary_outputs[first]})


def damaged_tables(fsm):
    """(description, table): each differs from the circuit on at least one case."""
    for key, (following, outputs) in fsm.rows.items():
        def changed(new_row):
            rows = dict(fsm.rows)
            if new_row:
                rows[key] = new_row
            else:
                del rows[key]
            return Fsm(fsm.inputs, fsm.outputs, fsm.reset, fsm.states, rows)

        for position in range(len(outputs)):
            flipped = "1" if outputs[position] == "0" else "0"
            yield f"output {position} of row {key}", changed((following, outputs[:position] + flipped + outputs[position + 1:]))
        other = next((s for s in fsm.states if s != following), None)
        if other:
            yield f"next state of row {key}", changed((other, outputs))
        if following != fsm.reset or "1" in outputs:
            yield f"row {key} removed", changed(None)


@dataclass
class Stats:
    fsms: int = 0
    rejected: int = 0
    exhaustive: int = 0
    sampled: int = 0
    checks: int = 0
    caught_walk: int = 0        # noticed from outside, starting at reset
    caught_forced: int = 0      # only noticed by setting the flip-flops directly
    harmless: int = 0           # the damaged netlist still computes the same function
    missed: int = 0

    def add(self, other):
        for name in vars(self):
            setattr(self, name, getattr(self, name) + getattr(other, name))


def check_damage(fsm, net, values, index, vectors, rng, limit, stats):
    original = evaluate(fsm, net, vectors)

    def try_one(description, candidate_fsm, candidate_net):
        try:
            check_behaviour(candidate_fsm, candidate_net, values, index, vectors)
        except Mismatch as caught:
            if caught.stage == "walk":
                stats.caught_walk += 1
            else:
                stats.caught_forced += 1
            return
        if candidate_net is not net and evaluate(fsm, candidate_net, vectors) == original:
            stats.harmless += 1
            return
        stats.missed += 1
        raise Failure(f"damage not noticed: {description}")

    netlists = list(damaged_netlists(fsm, net, rng))
    for description, change in rng.sample(netlists, min(limit, len(netlists))):
        mutant = net.copy()
        change(mutant)
        try_one(description, fsm, mutant)
    tables = list(damaged_tables(fsm))
    for description, table in rng.sample(tables, min(limit, len(tables))):
        try_one(description, table, net)


# ------------------------------------------------------------------ one FSM

def run_tool(cmd):
    # Closed stdin, so a command that falls into the simulator's menu exits instead of waiting
    return subprocess.run(cmd, stdin=subprocess.DEVNULL, capture_output=True, text=True)


def validate_fsm(text, sim, rng, stats, mutants=0):
    """Raises Failure when the synthesized netlist is wrong."""
    fsm = parse_fsm(text)
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "current.net"
    path.write_text(text)
    done = run_tool([sim, "--fsm", str(path), "--synth"])
    stats.fsms += 1

    # Every minterm reads all state bits and inputs, so a wire goes unread
    # exactly when no row sets any bit to 1. The synthesizer must reject those.
    useful = any(following != fsm.reset or "1" in outputs for following, outputs in fsm.rows.values())
    if not useful:
        if done.returncode != 1:
            raise Failure(f"expected a rejection (exit 1), got exit {done.returncode}")
        stats.rejected += 1
        return
    if done.returncode:
        raise Failure(f"synth failed with exit {done.returncode}: {done.stderr.strip()[-200:]}")

    net = check_structure(fsm, done.stdout)
    table = OUT / "current.table"
    table.write_text(done.stdout)
    parsed = run_tool([sim, "--check", str(table)])
    if parsed.returncode:
        raise Failure(f"structure: the parser rejected the netlist: {parsed.stderr.strip()[-200:]}")

    values, exhaustive = input_values(fsm, rng)
    vectors, index = make_vectors(fsm, values)
    try:
        stats.checks += check_behaviour(fsm, net, values, index, vectors)
    except Mismatch as mismatch:
        raise Failure(str(mismatch)) from mismatch
    if exhaustive:
        stats.exhaustive += 1
        if mutants:
            check_damage(fsm, net, values, index, vectors, rng, mutants, stats)
    else:
        stats.sampled += 1


def save_failure(group, number, text):
    folder = OUT / "failures"
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / f"{group.replace('/', '_')}_{number}.net"
    path.write_text(text)
    return path


def run_group(name, tables, sim, seed, mutants, totals):
    """Stops at the group's first failure, and keeps that FSM."""
    stats = Stats()
    rng = random.Random(f"{name}/{seed}")
    failure, kept = None, None
    number = -1
    for number, text in enumerate(tables):
        try:
            validate_fsm(text, sim, rng, stats, mutants)
        except Failure as error:
            failure, kept = error, save_failure(name, number, text)
            break
    totals.add(stats)
    summary = (f"{stats.fsms:6} FSMs ({stats.rejected} rejected, {stats.sampled} sampled) {stats.checks:9} checks, "
               f"damage {stats.caught_walk + stats.caught_forced} caught, {stats.harmless} harmless")
    if failure:
        print(f"FAIL  {name:24} {summary}\n      {failure}\n      kept in {kept}")
        return False
    print(f"PASS  {name:24} {summary}", flush=True)
    return True


def groups(args):
    """(name, table texts, damage limit per FSM), lazily, so a filter skips the work."""
    examples = sorted(REPO.glob("docs/fsm_example/*_fsm.net")) + sorted(FSMS.glob("*.net"))
    for path in examples:
        yield f"edge/{path.stem}", lambda p=path: [p.read_text()], args.mutants
    for shape in TINY + (TINY_FULL if args.full else []):
        yield f"tiny/{shape[0]}s_{shape[1]}i_{shape[2]}o", lambda s=shape: all_tables(*s), 0
    for name, settings in FAMILIES.items():
        def tables(family=name, chosen=settings):
            for seed in range(1, args.seeds + 1):
                rng = random.Random(f"{family}/{seed}")
                for setting in chosen:
                    yield random_fsm(rng, **setting)
        # Damage needs a full comparison per mistake, so the big families skip it
        limit = 0 if name in ("many_outputs", "many_states", "wide") else args.mutants
        yield f"random/{name}", tables, limit


def main():
    global OUT
    parser = argparse.ArgumentParser(description="Check synthesized netlists against their FSM tables.")
    parser.add_argument("groups", nargs="*", help="only groups whose name contains one of these")
    parser.add_argument("--seeds", type=int, default=3, help="runs of each random family")
    parser.add_argument("--mutants", type=int, default=40, help="damaged netlists and tables per FSM")
    parser.add_argument("--full", action="store_true", help="also the larger tiny shapes (minutes)")
    parser.add_argument("--sim", default=str(REPO / "simulator"), help="simulator binary")
    parser.add_argument("--out", default=str(OUT), help="folder for working files and failures")
    args = parser.parse_args()
    OUT = Path(args.out)

    if not Path(args.sim).is_file():
        sys.exit(f"no simulator at {args.sim} (run make)")

    chosen = [group for group in groups(args) if not args.groups or any(g in group[0] for g in args.groups)]
    if not chosen:
        sys.exit("no matching groups")

    totals = Stats()
    passed = failed = 0
    for name, tables, limit in chosen:
        if run_group(name, tables(), args.sim, 1, limit, totals):
            passed += 1
        else:
            failed += 1
    for leftover in ("current.net", "current.table"):
        (OUT / leftover).unlink(missing_ok=True)

    print(f"\n{passed} groups passed, {failed} failed")
    print(f"{totals.fsms} FSMs: {totals.exhaustive} exhaustive, {totals.sampled} sampled, "
          f"{totals.rejected} rejected as expected; {totals.checks} checks")
    print(f"damage: {totals.caught_walk} caught by walk, {totals.caught_forced} only by forced, "
          f"{totals.harmless} harmless, {totals.missed} missed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Check that validate.py notices real bugs in the synthesizer's code.

Each bug is one small edit to a temporary copy of Synthesizer.cpp, which is
built into its own simulator. validate.py must fail on every one of them, and
must pass on the unedited simulator first, or a failure would mean nothing.
The real source file is never changed.

    make
    validation/synthesizer/inject_bugs.py                  all bugs
    validation/synthesizer/inject_bugs.py reset constant   only matching bugs

Exits 1 if a bug goes unnoticed, or if a bug's line no longer exists in
Synthesizer.cpp (then update its entry in BUGS).
"""
import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
SOURCE = REPO / "src/synthesizer/Synthesizer.cpp"
VALIDATE = HERE / "validate.py"
BUILD = REPO / "build/release"

# Enough variety to reach each bug, without the slower damage checks
GROUPS = ["edge", "random", "tiny/1s", "tiny/2s_1i_1o"]

# name: (what goes wrong, text in Synthesizer.cpp, replacement)
BUGS = {
    "reset_not_gated": (
        "the D inputs ignore reset",
        "int notReset = net.notGate(reset);\n                d = net.andGate(sumOfProducts(dRows[i]), notReset);",
        "d = sumOfProducts(dRows[i]);"),
    "state_literal_inverted": (
        "minterms read the state bits inverted",
        "((currentCode[n] >> i) & 1) != 0",
        "((currentCode[n] >> i) & 1) == 0"),
    "first_input_dropped": (
        "minterms skip the first input",
        "for (size_t k = 0; k < fsm.inputs.size(); k++)\n                addLiteral",
        "for (size_t k = 1; k < fsm.inputs.size(); k++)\n                addLiteral"),
    "last_state_bit_dropped": (
        "minterms skip the top state bit",
        "for (int i = 0; i < bits; i++)\n                addLiteral",
        "for (int i = 0; i < bits - 1; i++)\n                addLiteral"),
    "too_few_bits": (
        "one flip-flop too few at 2^k + 1 states",
        "while ((size_t(1) << bits) < states.size())",
        "while ((size_t(1) << bits) + 1 < states.size())"),
    "one_bit_too_many": (
        "one flip-flop too many at 2^k states",
        "while ((size_t(1) << bits) < states.size())",
        "while ((size_t(1) << bits) <= states.size())"),
    "sharing_ignores_second_input": (
        "gates with different second inputs get shared",
        "uint64_t key = (static_cast<uint64_t>(a) << 32) | static_cast<uint32_t>(b);",
        "uint64_t key = static_cast<uint64_t>(a) << 32;"),
    "not_sharing_wrong": (
        "wires numbered 100 and up share a NOT, so needs 100+ inputs",
        "if (wires[a].inverted != -1)\n                return wires[a].inverted;",
        "if (wires[a >= 100 ? 100 : a].inverted != -1)\n                return wires[a >= 100 ? 100 : a].inverted;"),
    "constant_is_one": (
        "lines that are never 1 get tied to 1",
        "zero = net.andGate(reset, net.notGate(reset));",
        "zero = net.orGate(reset, net.notGate(reset));"),
    "or_chain_keeps_last_only": (
        "each OR keeps only its last minterm",
        "acc = acc == -1 ? wire : orGate(acc, wire);",
        "acc = wire;"),
    "output_needs_nonzero_next": (
        "outputs are lost on rows that go to code 0",
        "if (r.outputs[j] == \x271\x27)",
        "if (r.outputs[j] == \x271\x27 && next != 0)"),
    "reset_not_code_zero": (
        "the first state in the table gets code 0 instead of the reset state",
        "std::vector<std::string> states = {fsm.reset};",
        "std::vector<std::string> states = {fsm.reset};\n"
        "        if (!fsm.rows.empty() && fsm.rows[0].current != fsm.reset) {\n"
        "            code = {{fsm.rows[0].current, 0}};\n"
        "            states = {fsm.rows[0].current};\n"
        "        }"),
}


def build(source, binary):
    """Links the edited file ahead of the library, so it replaces the original."""
    command = ["g++", "-std=c++17", "-O1", f"-I{REPO / 'include'}", str(source),
               str(BUILD / "main.o"), str(BUILD / "libhdlsim.a"), "-o", str(binary)]
    done = subprocess.run(command, capture_output=True, text=True)
    if done.returncode:
        sys.exit(f"could not build {source.name}:\n{done.stderr[-400:]}")


def validate(binary, out):
    """Returns (exit code, the groups that failed, the first failure's message)."""
    command = [sys.executable, str(VALIDATE), *GROUPS, "--seeds", "1", "--mutants", "0",
               "--sim", str(binary), "--out", str(out)]
    done = subprocess.run(command, capture_output=True, text=True)
    failed = re.findall(r"^FAIL\s+(\S+)", done.stdout, re.M)
    if done.returncode and not failed:
        sys.exit(f"validate.py did not run:\n{done.stderr[-400:]}")
    first = re.search(r"^FAIL.*\n\s+(.*)", done.stdout, re.M)
    return done.returncode, failed, first[1] if first else ""


def main():
    parser = argparse.ArgumentParser(description="Check that validate.py notices real synthesizer bugs.")
    parser.add_argument("bugs", nargs="*", help="only bugs whose name contains one of these")
    args = parser.parse_args()

    if not (BUILD / "main.o").is_file():
        sys.exit(f"no {BUILD / 'main.o'} (run make)")
    chosen = {name: bug for name, bug in BUGS.items() if not args.bugs or any(b in name for b in args.bugs)}
    if not chosen:
        sys.exit("no matching bugs")

    original = SOURCE.read_text()
    outdated = [name for name, (_, old, _new) in chosen.items() if original.count(old) != 1]
    if outdated:
        sys.exit(f"these bugs no longer match Synthesizer.cpp exactly once: {', '.join(outdated)}")

    with tempfile.TemporaryDirectory() as folder:
        work = Path(folder)
        clean = work / "clean"
        source = work / "Synthesizer.cpp"
        source.write_text(original)
        build(source, clean)
        code, failed, first = validate(clean, work / "out")
        if code:
            sys.exit(f"the unedited synthesizer already fails, in {failed}: {first}")
        print("PASS    unedited synthesizer")

        missed = 0
        for name, (meaning, old, new) in chosen.items():
            source.write_text(original.replace(old, new))
            binary = work / name
            build(source, binary)
            code, failed, _ = validate(binary, work / "out")
            if code:
                print(f"CAUGHT  {name:30} {len(failed):3} groups failed, first {failed[0] if failed else '-':28} {meaning}",
                      flush=True)
            else:
                missed += 1
                print(f"MISSED  {name:30} {meaning}", flush=True)

    print(f"\n{len(chosen) - missed} of {len(chosen)} bugs caught")
    sys.exit(1 if missed else 0)


if __name__ == "__main__":
    main()

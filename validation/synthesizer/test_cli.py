#!/usr/bin/env python3
"""Checks the simulator's FSM commands from the command line.

    make
    validation/synthesizer/test_cli.py

The C++ test suite does not build main.cpp, so these run the real program. They
check the options, exit codes and output, not the netlist's logic: validate.py
checks that. Exits 1 on any failure.
"""
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
SIMULATOR = str(REPO / "simulator")
EXAMPLE = REPO / "docs/fsm_example/dead_unreachable_fsm.net"
REJECTED = Path(__file__).resolve().parent / "fsms/rejected_all_zero.net"

failures = 0


def run(*arguments):
    return subprocess.run([SIMULATOR, *map(str, arguments)], stdin=subprocess.DEVNULL, capture_output=True, text=True)


def check(name, passed):
    global failures
    print(("PASS  " if passed else "FAIL  ") + name)
    failures += not passed


def main():
    if not Path(SIMULATOR).is_file():
        sys.exit(f"no simulator at {SIMULATOR} (run make)")

    with tempfile.TemporaryDirectory() as folder:
        work = Path(folder)
        fsm = work / "machine.net"
        fsm.write_text(EXAMPLE.read_text())
        table = work / "machine.table"

        done = run("--fsm", fsm, "--synth")
        check("--synth exits 0", done.returncode == 0)
        check("--synth prints only the netlist", all(line.startswith(("#", "machine\t")) or not line
                                                     for line in done.stdout.splitlines()))
        check("--synth writes no file by itself", not table.exists())
        netlist = done.stdout

        out = work / "chosen.table"
        done = run("--fsm", fsm, "--synth", "-o", out)
        check("--synth -o exits 0 and prints nothing", done.returncode == 0 and not done.stdout)
        check("--synth -o writes the same netlist", out.is_file() and out.read_text() == netlist)
        check("--check accepts the netlist", run("--check", out).returncode == 0)

        keep = work / "keep.table"
        keep.write_text("old contents")
        done = run("--fsm", REJECTED, "--synth", "-o", keep)
        check("--synth exits 1 on a rejected FSM", done.returncode == 1)
        check("a failed --synth leaves its -o file alone", keep.read_text() == "old contents")
        check("a rejected FSM prints no netlist", not run("--fsm", REJECTED, "--synth").stdout)

        done = run("--fsm", fsm, "--dead")
        check("--dead exits 0 and lists the states", done.returncode == 0 and "dead states:" in done.stdout
              and all(f"  {state}  (" in done.stdout for state in "TVH"))
        check("--dead does not synthesize", "unreachable" not in done.stdout and not table.exists())

        done = run("--fsm", fsm, "--unreachable")
        check("--unreachable exits 0 and lists the states", done.returncode == 0
              and "unreachable: U V H" in done.stdout and "dead" not in done.stdout)
        check("--unreachable does not synthesize", not table.exists())

        done = run("--fsm", fsm)
        check("plain --fsm does all three steps", done.returncode == 0 and "dead states:" in done.stdout
              and "unreachable: U V H" in done.stdout and table.is_file())
        table.unlink()
        rejected = work / "rejected.net"
        rejected.write_text(REJECTED.read_text())
        check("plain --fsm exits 1 when synthesis fails", run("--fsm", rejected).returncode == 1)

        missing = work / "missing.net"
        for action in ("--synth", "--dead", "--unreachable"):
            check(f"{action} exits 1 on a missing file", run("--fsm", missing, action).returncode == 1)
        check("plain --fsm exits 1 on a missing file", run("--fsm", missing).returncode == 1)

        for name, arguments in (("no file", ["--fsm"]),
                                ("an option in place of the file", ["--fsm", "--dead"]),
                                ("two actions", ["--fsm", fsm, "--dead", "--synth"]),
                                ("an unknown option", ["--fsm", fsm, "--nope"]),
                                ("-o with no file", ["--fsm", fsm, "--synth", "-o"]),
                                ("-o without --synth", ["--fsm", fsm, "--dead", "-o", out])):
            done = run(*arguments)
            check(f"--fsm rejects {name}", done.returncode == 1 and "usage:" in done.stderr
                  and "Select an Option" not in done.stdout)

        bad = work / "bad.table"
        bad.write_text("this is not a netlist\n")
        check("--check rejects a bad netlist", run("--check", bad).returncode == 1)
        check("--check rejects a missing file", run("--check", missing).returncode == 1)
        check("--check needs one file", run("--check").returncode == 1)

    print(f"\n{failures} failed")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()

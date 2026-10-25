#!/usr/bin/env python3
"""
SLMS Web GUI Server
Zero-dependency HTTP server bridging the frontend with the C++ simulator.
Includes traffic-light live simulation, custom testbenches, test-suite execution,
and an interactive vending-machine FSM endpoint.
"""

import sys
import os
import json
import subprocess
import threading
from http.server import HTTPServer, SimpleHTTPRequestHandler

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(BASE_DIR)
SIMULATOR = os.path.join(PROJECT_ROOT, "simulator")
OUTPUT_JSON = os.path.join(PROJECT_ROOT, "output", "waveform.json")
BUILD_DIR = os.path.join(PROJECT_ROOT, "build")

# The simulator writes shared waveform files, so only one simulation may
# run at a time. RLock allows helpers to be safely called within locked code.
SIMULATOR_LOCK = threading.RLock()
LIVE_LOCK = threading.RLock()
VENDING_LOCK = threading.RLock()

LIVE_SESSIONS = {}
VENDING_SESSIONS = {}

MAX_LIVE_TICKS = 200
MAX_VENDING_COINS = 100

TRAFFIC_NETLIST = "tests/netlists/fsm/traffic_light/traffic_light_fsm.net"
VENDING_NETLIST = (
    "examples/vending_machine_advanced/vending_machine_advanced.net"
)


def run_testbench(script, filename, timeout=30):
    """Run a testbench through the real C++ simulator and return its waveform."""
    os.makedirs(BUILD_DIR, exist_ok=True)
    temp_path = os.path.join(BUILD_DIR, filename)

    with open(temp_path, "w", encoding="utf-8") as fh:
        fh.write(script)

    with SIMULATOR_LOCK:
        completed = subprocess.run(
            [SIMULATOR, "--testbench", temp_path],
            cwd=PROJECT_ROOT,
            text=True,
            capture_output=True,
            timeout=timeout,
        )

        if completed.returncode != 0:
            raise RuntimeError(
                (completed.stderr + "\n" + completed.stdout)[-3000:]
            )

        if not os.path.isfile(OUTPUT_JSON):
            raise RuntimeError("The C++ simulator did not generate waveform.json.")

        with open(OUTPUT_JSON, encoding="utf-8") as fh:
            waveform = json.load(fh)

    if not waveform.get("times"):
        raise RuntimeError("The C++ simulator returned an empty waveform.")

    return completed, waveform


def evaluate_live(steps):
    """Replay traffic-light inputs through the actual C++ simulator."""
    end = 6 if not steps else 10 * len(steps) + 6

    lines = [
        f"FSM {TRAFFIC_NETLIST}",
        "CLOCK clk 10",
        f"MAX_TIME {end}",
        "@0 reset=1 en=0",
        "@6 reset=0 en=0",
    ]

    for i, item in enumerate(steps):
        lines.append(
            f"@{10 * (i + 1)} en={item['en']} reset={item['reset']}"
        )

    _, wave = run_testbench(
        "\n".join(lines) + "\n",
        "live_gui_testbench.tb",
    )

    times = wave["times"]
    targets = [0, 5] if not steps else [10 * len(steps), 10 * len(steps) + 5]

    samples = []
    for time in targets:
        if time not in times:
            raise RuntimeError(
                f"Expected traffic-light sample at {time} ns was not found."
            )

        i = times.index(time)
        samples.append({
            "time": time,
            "signals": {
                signal: int(values[i])
                for signal, values in wave["data"].items()
            },
        })

    return samples


def evaluate_vending(coins):
    """
    Replay a vending-machine session through the actual C++ FSM.

    Accepted coins: ₹1, ₹2, ₹5, ₹10.
    Product price: ₹10.

    Each coin has a separate clock slot. A full idle clock edge after each
    slot allows the FSM to leave its one-cycle dispense/change state before
    another coin is sampled.
    """
    lines = [
        f"FSM {VENDING_NETLIST}",
        "CLOCK clk 10",
        f"MAX_TIME {max(30, 20 * len(coins) + 30)}",
        "@0 reset=1 coin1=0 coin2=0 coin5=0 coin10=0",
        "@6 reset=0",
    ]

    for i, coin in enumerate(coins):
        start = 8 + 20 * i
        end = start + 8

        lines.append(
            f"@{start} "
            f"coin1={int(coin == 1)} "
            f"coin2={int(coin == 2)} "
            f"coin5={int(coin == 5)} "
            f"coin10={int(coin == 10)}"
        )
        lines.append(
            f"@{end} coin1=0 coin2=0 coin5=0 coin10=0"
        )

    _, wave = run_testbench(
        "\n".join(lines) + "\n",
        "live_vending_testbench.tb",
    )

    times = wave["times"]
    target_time = 6 if not coins else 16 + 20 * (len(coins) - 1)

    if target_time not in times:
        raise RuntimeError(
            f"Expected vending-machine sample at {target_time} ns was not found."
        )

    sample_index = times.index(target_time)
    signals = {
        name: int(values[sample_index])
        for name, values in wave["data"].items()
    }

    def decode_binary(prefix):
        return sum(
            (1 << bit) * signals.get(f"{prefix}{1 << bit}", 0)
            for bit in range(4)
        )

    # The actual netlist uses bal1/bal2/bal4/bal8 and
    # change1/change2/change4/change8 output bits.
    balance = sum(
        weight * signals.get(f"bal{weight}", 0)
        for weight in (1, 2, 4, 8)
    )
    change = sum(
        weight * signals.get(f"change{weight}", 0)
        for weight in (1, 2, 4, 8)
    )
    vend = signals.get("vend", 0)

    return {
        "time": target_time,
        "signals": signals,
        "balance": balance,
        "vend": vend,
        "change": change if vend else 0,
        "waveform": wave,
    }


class SLMSRequestHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=BASE_DIR, **kwargs)

    def do_POST(self):
        if self.path == "/api/live":
            self.handle_live()
        elif self.path == "/api/simulate":
            self.handle_simulate()
        elif self.path == "/api/vending":
            self.handle_vending()
        else:
            self.send_error(404, "Endpoint not found")

    def do_GET(self):
        if self.path == "/api/tests":
            self.handle_tests()
        else:
            super().do_GET()

    def read_json_request(self):
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0 or length > 1_000_000:
            raise ValueError("Invalid or oversized request body.")

        return json.loads(self.rfile.read(length).decode("utf-8"))

    def send_json(self, data, code=200):
        payload = json.dumps(data).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(payload)

    def handle_live(self):
        try:
            req = self.read_json_request()
            session = str(req.get("session", "default"))[:100]
            operation = req.get("action", "step")

            with LIVE_LOCK:
                if operation == "reset":
                    steps = []
                elif operation == "step":
                    if session not in LIVE_SESSIONS:
                        raise ValueError("Session not initialized; reset first.")

                    steps = list(LIVE_SESSIONS[session])
                    steps.append({
                        "en": int(bool(req.get("en", 0))),
                        "reset": int(bool(req.get("reset", 0))),
                    })

                    if len(steps) > MAX_LIVE_TICKS:
                        raise ValueError(
                            "Maximum 200 ticks reached. Press Reset."
                        )
                else:
                    raise ValueError("Unknown traffic-light action.")

                samples = evaluate_live(steps)
                LIVE_SESSIONS[session] = steps

            self.send_json({
                "success": True,
                "samples": samples,
                "tick": len(steps),
            })

        except Exception as exc:
            self.send_json({"success": False, "error": str(exc)}, 500)

    def handle_vending(self):
        try:
            req = self.read_json_request()
            session = str(req.get("session", "default"))[:100]
            action = req.get("action", "insert")

            with VENDING_LOCK:
                if action == "reset":
                    coins = []

                elif action == "insert":
                    coins = list(VENDING_SESSIONS.get(session, []))
                    coin = req.get("coin")

                    # bool is a subclass of int in Python, so reject it too.
                    if isinstance(coin, bool) or coin not in (1, 2, 5, 10):
                        raise ValueError(
                            "Select a valid coin: ₹1, ₹2, ₹5, or ₹10."
                        )

                    coins.append(coin)

                    if len(coins) > MAX_VENDING_COINS:
                        raise ValueError(
                            "Maximum 100 coin insertions reached. Reset first."
                        )

                else:
                    raise ValueError("Unknown vending-machine action.")

                result = evaluate_vending(coins)
                VENDING_SESSIONS[session] = coins

            self.send_json({
                "success": True,
                "steps": len(coins),
                "result": result,
            })

        except Exception as exc:
            self.send_json({"success": False, "error": str(exc)}, 500)

    def handle_simulate(self):
        try:
            req = self.read_json_request()
            tb_content = req.get("testbench", "")

            if not isinstance(tb_content, str) or not tb_content.strip():
                raise ValueError("Enter a non-empty testbench script.")

            completed, wave = run_testbench(
                tb_content,
                "temp_gui_testbench.tb",
                timeout=30,
            )

            assertions = {
                "total": 0,
                "passed": 0,
                "failed": 0,
                "records": [],
            }

            for line in completed.stdout.splitlines():
                if "Total Evaluated" in line:
                    try:
                        assertions["total"] = int(line.split(":", 1)[1].strip())
                    except (ValueError, IndexError):
                        pass

                elif "Passed" in line and "(" in line:
                    try:
                        assertions["passed"] = int(
                            line.split(":", 1)[1].split("(")[0].strip()
                        )
                    except (ValueError, IndexError):
                        pass

                elif "Failed" in line and ":" in line:
                    try:
                        assertions["failed"] = int(
                            line.split(":", 1)[1].strip()
                        )
                    except (ValueError, IndexError):
                        pass

            self.send_json({
                "success": completed.returncode == 0,
                "stdout": completed.stdout,
                "stderr": completed.stderr,
                "waveform": wave,
                "assertions": assertions,
            })

        except Exception as exc:
            self.send_json({"success": False, "error": str(exc)}, 500)

    def handle_tests(self):
        try:
            with SIMULATOR_LOCK:
                completed = subprocess.run(
                    [SIMULATOR, "--test", "all"],
                    cwd=PROJECT_ROOT,
                    capture_output=True,
                    text=True,
                    timeout=30,
                )

            tests = []
            for line in completed.stdout.splitlines():
                if (
                    "[" in line
                    and "]" in line
                    and ("[ PASS ]" in line or "[ FAIL ]" in line)
                ):
                    parts = line.split()
                    if len(parts) >= 2:
                        category = parts[0].strip("[]")
                        name = parts[1]
                        tests.append({
                            "cat": category,
                            "name": name,
                            "ok": "[ PASS ]" in line,
                        })

            self.send_json({
                "success": completed.returncode == 0,
                "total": len(tests),
                "passed": sum(test["ok"] for test in tests),
                "failed": sum(not test["ok"] for test in tests),
                "tests": tests,
                "stdout": completed.stdout,
                "stderr": completed.stderr,
            })

        except Exception as exc:
            self.send_json({"success": False, "error": str(exc)}, 500)


if __name__ == "__main__":
    print(f"[*] Starting SLMS Web GUI Server on http://localhost:{PORT}")
    httpd = HTTPServer(("0.0.0.0", PORT), SLMSRequestHandler)

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[*] Server stopped.")
    finally:
        httpd.server_close()

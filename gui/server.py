#!/usr/bin/env python3
"""
SLMS Web GUI Server
Zero-dependency HTTP server bridging the web frontend with the C++ simulator engine.
"""

import sys
import os
import json
import subprocess
import threading
import re
from http.server import HTTPServer, SimpleHTTPRequestHandler

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(BASE_DIR)

# Each browser session has an independent sequence of clock inputs. The C++
# testbench is replayed through the latest edge; only the newly calculated
# samples are returned to the browser. This avoids precomputed future waves.
LIVE_LOCK = threading.RLock()
LIVE_SESSIONS = {}
MAX_LIVE_TICKS = 200
SIMULATOR_LOCK = threading.RLock()
VENDING_SESSIONS = {}
MAX_GENERIC_EVENTS = 500

def evaluate_live(steps):
    end = 6 if not steps else 10 * len(steps) + 6
    lines = [
        'FSM tests/netlists/fsm/traffic_light/traffic_light_fsm.net',
        'CLOCK clk 10', f'MAX_TIME {end}',
        '@0 reset=1 en=0', '@6 reset=0 en=0',
    ]
    for i, item in enumerate(steps):
        lines.append(f"@{10 * (i+1)} en={item['en']} reset={item['reset']}")
    script = '\n'.join(lines) + '\n'
    temp_path = os.path.join(PROJECT_ROOT, 'build', 'live_gui_testbench.tb')
    os.makedirs(os.path.dirname(temp_path), exist_ok=True)
    with open(temp_path, 'w', encoding='utf-8') as fh:
        fh.write(script)
    sim_bin = os.path.join(PROJECT_ROOT, 'simulator')
    completed = subprocess.run([sim_bin, '--testbench', temp_path],
                               cwd=PROJECT_ROOT, text=True, capture_output=True, timeout=30)
    if completed.returncode:
        raise RuntimeError((completed.stderr + '\n' + completed.stdout)[-2500:])
    with open(os.path.join(PROJECT_ROOT, 'output', 'waveform.json'), encoding='utf-8') as fh:
        wave = json.load(fh)
    times = wave['times']
    # Show ONLY samples for the current tick. No future values are sent.
    targets = [0, 5] if not steps else [10*len(steps), 10*len(steps)+5]
    samples = []
    for time in targets:
        i = times.index(time)
        samples.append({'time': time, 'signals': {s: int(values[i]) for s, values in wave['data'].items()}})
    return samples

class SLMSRequestHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=BASE_DIR, **kwargs)

    def do_POST(self):
        if self.path == '/api/live':
            self.handle_live()
        elif self.path == '/api/simulate':
            self.handle_simulate()
        elif self.path == '/api/generic-simulate':
            self.handle_generic_simulate()
        elif self.path == '/api/vending':
            self.handle_vending()
        else:
            self.send_error(404, "Endpoint not found")

    def do_GET(self):
        if self.path == '/api/tests':
            self.handle_tests()
        else:
            super().do_GET()

    def handle_live(self):
        try:
            n = int(self.headers.get('Content-Length', '0'))
            req = json.loads(self.rfile.read(n).decode('utf-8'))
            session = str(req.get('session', 'default'))[:100]
            operation = req.get('action', 'step')
            with LIVE_LOCK:
                if operation == 'reset':
                    steps = []
                elif operation == 'step':
                    if session not in LIVE_SESSIONS:
                        raise ValueError('Session not initialized; reset first')
                    steps = LIVE_SESSIONS[session] + [
                        {'en': int(bool(req.get('en', 0))), 'reset': int(bool(req.get('reset', 0)))}
                    ]
                    if len(steps) > MAX_LIVE_TICKS:
                        raise ValueError('Maximum 200 ticks reached. Press Reset.')
                else:
                    raise ValueError('Unknown action')
                samples = evaluate_live(steps)
                LIVE_SESSIONS[session] = steps
            self.send_json({'success': True, 'samples': samples, 'tick': len(steps)})
        except Exception as exc:
            self.send_json({'success': False, 'error': str(exc)}, 500)

    def send_json(self, data, code=200):
        payload = json.dumps(data).encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def handle_simulate(self):
        content_length = int(self.headers.get('Content-Length', 0))
        post_data = self.rfile.read(content_length)
        req = json.loads(post_data.decode('utf-8'))
        tb_content = req.get('testbench', '')

        temp_tb_path = os.path.join(PROJECT_ROOT, 'build', 'temp_gui_testbench.tb')
        os.makedirs(os.path.dirname(temp_tb_path), exist_ok=True)
        with open(temp_tb_path, 'w') as f:
            f.write(tb_content)

        # Run simulator
        sim_bin = os.path.join(PROJECT_ROOT, 'simulator')
        try:
            res = subprocess.run([sim_bin, '--testbench', temp_tb_path], 
                                 cwd=PROJECT_ROOT, 
                                 capture_output=True, 
                                 text=True, 
                                 timeout=10)
            
            # Read output/waveform.json
            wave_json_path = os.path.join(PROJECT_ROOT, 'output', 'waveform.json')
            wave_data = {}
            if os.path.exists(wave_json_path):
                with open(wave_json_path) as wf:
                    wave_data = json.load(wf)

            # Parse assertions from stdout
            assertions = {"total": 0, "passed": 0, "failed": 0, "records": []}
            # Add records from log if available
            for line in res.stdout.splitlines():
                if "Passed" in line and "(" in line:
                    parts = line.split(":")
                    if len(parts) > 1:
                        try:
                            assertions["passed"] = int(parts[1].split("(")[0].strip())
                        except: pass
                if "Failed" in line and ":" in line:
                    parts = line.split(":")
                    if len(parts) > 1:
                        try:
                            assertions["failed"] = int(parts[1].strip())
                        except: pass
                if "Total Evaluated" in line:
                    parts = line.split(":")
                    if len(parts) > 1:
                        try:
                            assertions["total"] = int(parts[1].strip())
                        except: pass

            response_data = {
                "success": res.returncode == 0,
                "stdout": res.stdout,
                "waveform": wave_data,
                "assertions": assertions
            }

            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(response_data).encode('utf-8'))

        except Exception as e:
            self.send_response(500)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({"error": str(e)}).encode('utf-8'))

    def run_generated_testbench(self, script, timeout=20):
        """Run a generated testbench through the real simulator, safely serialized."""
        temp_dir = os.path.join(PROJECT_ROOT, 'build')
        os.makedirs(temp_dir, exist_ok=True)
        # Each request gets its own file; simulator output files are shared, so execution is locked.
        import tempfile
        fd, temp_path = tempfile.mkstemp(prefix='gui_', suffix='.tb', dir=temp_dir, text=True)
        try:
            with os.fdopen(fd, 'w', encoding='utf-8') as fh:
                fh.write(script)
            sim_bin = os.path.join(PROJECT_ROOT, 'simulator')
            with SIMULATOR_LOCK:
                res = subprocess.run([sim_bin, '--testbench', temp_path], cwd=PROJECT_ROOT,
                                     capture_output=True, text=True, timeout=timeout)
                wave_path = os.path.join(PROJECT_ROOT, 'output', 'waveform.json')
                wave = {}
                if os.path.isfile(wave_path):
                    with open(wave_path, encoding='utf-8') as wf:
                        wave = json.load(wf)
            return res, wave
        finally:
            try:
                os.remove(temp_path)
            except OSError:
                pass

    def handle_generic_simulate(self):
        """Run a user-selected .table combinational netlist or .net FSM with timed inputs."""
        try:
            n = int(self.headers.get('Content-Length', '0'))
            if n <= 0 or n > 1_000_000:
                raise ValueError('Invalid request size.')
            req = json.loads(self.rfile.read(n).decode('utf-8'))
            circuit_type = str(req.get('circuitType', 'netlist')).lower()
            circuit_path = str(req.get('netlist', '')).strip()
            if not circuit_path or os.path.isabs(circuit_path) or '..' in circuit_path.split('/'):
                raise ValueError('Enter a project-relative netlist path (no absolute paths or ..).')
            full_path = os.path.realpath(os.path.join(PROJECT_ROOT, circuit_path))
            if not full_path.startswith(os.path.realpath(PROJECT_ROOT) + os.sep) or not os.path.isfile(full_path):
                raise ValueError('Netlist file not found inside the project. Check the relative path.')
            if circuit_type == 'fsm' and not circuit_path.endswith('.net'):
                raise ValueError('FSM simulation expects a .net file.')
            if circuit_type != 'fsm' and not circuit_path.endswith(('.table', '.tbl')):
                raise ValueError('Combinational netlist simulation expects a .table file.')
            max_time = int(req.get('maxTime', 40))
            if max_time < 1 or max_time > 100000:
                raise ValueError('Maximum time must be between 1 and 100000.')
            lines = [('FSM ' if circuit_type == 'fsm' else 'NETLIST ') + circuit_path]
            clock = str(req.get('clockPeriod', '')).strip()
            if circuit_type == 'fsm' and clock:
                period = int(clock)
                if period < 2 or period > 10000:
                    raise ValueError('Clock period must be between 2 and 10000.')
                lines.append(f'CLOCK clk {period}')
            lines.append(f'MAX_TIME {max_time}')
            stimuli = str(req.get('stimuli', '')).splitlines()
            events = 0
            for raw in stimuli:
                raw = raw.strip()
                if not raw or raw.startswith('#'):
                    continue
                # Accept `0: a=0 b=1` or `@0 a=0 b=1`.
                match = re.fullmatch(r'@?(\d+)\s*:\s*(.+)|@(\d+)\s+(.+)', raw)
                if not match:
                    raise ValueError(f'Invalid input line: {raw!r}. Use `0: a=0 b=1` or `@0 a=0 b=1`.')
                time = int(match.group(1) or match.group(3))
                assignments = match.group(2) or match.group(4)
                if time > max_time:
                    raise ValueError(f'Input time {time} exceeds MAX_TIME {max_time}.')
                if not re.fullmatch(r'\s*[A-Za-z_][A-Za-z0-9_]*(?:\[-?\d+\])?\s*=\s*[01](?:\s+[A-Za-z_][A-Za-z0-9_]*(?:\[-?\d+\])?\s*=\s*[01])*\s*', assignments):
                    raise ValueError(f'Invalid assignments at time {time}: use signal=0/1 pairs.')
                lines.append(f'@{time} {assignments}')
                events += 1
                if events > MAX_GENERIC_EVENTS:
                    raise ValueError(f'At most {MAX_GENERIC_EVENTS} input events are allowed.')
            if not events:
                raise ValueError('Add at least one input event.')
            res, wave = self.run_generated_testbench('\n'.join(lines) + '\n')
            self.send_json({'success': res.returncode == 0, 'stdout': res.stdout,
                            'stderr': res.stderr, 'waveform': wave,
                            'generatedTestbench': '\n'.join(lines) + '\n'},
                           200 if res.returncode == 0 and wave.get('times') else 400)
        except Exception as exc:
            self.send_json({'success': False, 'error': str(exc)}, 400)

    def handle_vending(self):
        """Run the bundled vending FSM for a sequence of 5/10-unit coins."""
        try:
            n = int(self.headers.get('Content-Length', '0'))
            if n <= 0 or n > 100000:
                raise ValueError('Invalid request size.')
            req = json.loads(self.rfile.read(n).decode('utf-8'))
            coins = req.get('coins', [])
            if not isinstance(coins, list) or len(coins) > 50 or any(c not in (5, 10) for c in coins):
                raise ValueError('Coins must be a list of 5 or 10 values (maximum 50 coins).')
            lines = [
                'FSM examples/vending_machine/vending_machine.net',
                'CLOCK clk 10',
                f'MAX_TIME {max(40, 20 * len(coins) + 20)}',
                '@0 reset=1 coin5=0 coin10=0',
                '@5 reset=0',
            ]
            for i, coin in enumerate(coins):
                start = 8 + 20 * i
                lines.append(f'@{start} coin5={int(coin == 5)} coin10={int(coin == 10)}')
                lines.append(f'@{start + 8} coin5=0 coin10=0')
            res, wave = self.run_generated_testbench('\n'.join(lines) + '\n')
            if res.returncode != 0 or not wave.get('times'):
                raise RuntimeError((res.stderr + '\n' + res.stdout)[-2500:] or 'Vending simulation returned no waveform.')
            times = wave['times']
            # Sample the meaningful response to the final coin, not the idle end of the testbench.
            if not coins:
                target_time = 6
            elif coins[-1] == 10 or (coins[-1] == 5 and len(coins) > 1 and coins[-2] == 5):
                # A 10 coin vends on the next rising edge; a second 5 coin
                # completes the purchase when the FSM already holds 5 credit.
                target_time = 10 + 20 * (len(coins) - 1)
            else:
                # A first 5 coin is reflected as stored credit after the edge.
                target_time = 16 + 20 * (len(coins) - 1)
            sample = next((i for i, t in enumerate(times) if t >= target_time), len(times) - 1)
            signals = {name: int(values[sample]) for name, values in wave.get('data', {}).items() if sample < len(values)}
            response = {'success': True, 'coins': coins, 'time': times[sample], 'signals': signals,
                        'vend': signals.get('vend', 0), 'credit5': signals.get('credit5', 0),
                        'waveform': wave, 'stdout': res.stdout}
            self.send_json(response)
        except Exception as exc:
            self.send_json({'success': False, 'error': str(exc)}, 400)

    def handle_tests(self):
        sim_bin = os.path.join(PROJECT_ROOT, 'simulator')
        try:
            res = subprocess.run([sim_bin, '--test', 'all'], 
                                 cwd=PROJECT_ROOT, 
                                 capture_output=True, 
                                 text=True, 
                                 timeout=10)
            
            tests = []
            for line in res.stdout.splitlines():
                if "[" in line and "]" in line and ("[ PASS ]" in line or "[ FAIL ]" in line):
                    parts = line.split()
                    cat = parts[0].replace("[", "").replace("]", "")
                    name = parts[1]
                    status = "PASS" in line
                    tests.append({"cat": cat, "name": name, "ok": status})

            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({"total": len(tests), "passed": sum(t["ok"] for t in tests), "failed": sum(not t["ok"] for t in tests), "tests": tests}).encode('utf-8'))
        except Exception as e:
            self.send_response(500)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps({"error": str(e)}).encode('utf-8'))

if __name__ == '__main__':
    print(f"[*] Starting SLMS Web GUI Server on http://localhost:{PORT}")
    httpd = HTTPServer(('0.0.0.0', PORT), SLMSRequestHandler)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[*] Server stopped.")

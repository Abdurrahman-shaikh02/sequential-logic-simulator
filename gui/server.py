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
import tempfile
import shutil
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


def assertion_summary(stdout, stderr=''):
    """Read the assertion totals printed by the C++ engine; never infer a pass."""
    report = (stdout or '') + '\n' + (stderr or '')
    def number(pattern):
        match = re.search(pattern, report, re.I)
        return int(match.group(1)) if match else 0
    return {
        'total': number(r'Total Evaluated\s*:\s*(\d+)'),
        'passed': number(r'Passed\s*:\s*(\d+)'),
        'failed': number(r'Failed\s*:\s*(\d+)'),
    }


def resolve_project_path(raw_path):
    """Accept project-relative paths and absolute paths inside this project only."""
    raw_path = str(raw_path or '').strip()
    if not raw_path:
        raise ValueError('Choose a netlist path.')
    root = os.path.realpath(PROJECT_ROOT)
    # Browser autofill/paste sometimes supplies the full path to a file in this project.
    candidate_input = raw_path if os.path.isabs(raw_path) else os.path.join(root, raw_path)
    candidate = os.path.realpath(candidate_input)
    # If a copied absolute path contains a renamed parent folder, recover only
    # the suffix beneath this project's own directory name, then re-check it.
    if os.path.isabs(raw_path):
        marker = os.sep + os.path.basename(root) + os.sep
        if not os.path.commonpath([root, candidate]) == root and marker in raw_path:
            suffix = raw_path.split(marker, 1)[1]
            candidate = os.path.realpath(os.path.join(root, suffix))
    try:
        if os.path.commonpath([root, candidate]) != root:
            raise ValueError('Netlist must be inside the project folder. Use a project-relative path.')
    except ValueError:
        raise ValueError('Netlist must be inside the project folder. Use a project-relative path.')
    if not os.path.isfile(candidate):
        raise ValueError('Netlist file not found inside the project. Check the path and filename.')
    return os.path.relpath(candidate, root).replace(os.sep, '/')

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
        elif self.path == '/api/file-simulate':
            self.handle_file_simulate()
        elif self.path == '/api/generic-simulate':
            self.handle_generic_simulate()
        elif self.path == '/api/vending':
            self.handle_vending()
        else:
            self.send_error(404, "Endpoint not found")

    def do_GET(self):
        if self.path == '/api/tests':
            self.handle_tests()
        elif self.path == '/api/catalog':
            self.handle_catalog()
        else:
            super().do_GET()

    def handle_catalog(self):
        """Return only circuits and matching reference testbenches present in this project."""
        entries = [
            # Combinational examples
            ('combinational', 'AND gate', 'netlist', 'examples/component_file_io/combinational/and/netlist.table', 'examples/component_file_io/combinational/and/testbench.tb', 40, ''),
            ('combinational', 'AND-NOT gate', 'netlist', 'examples/component_file_io/combinational/andnot/netlist.table', 'examples/component_file_io/combinational/andnot/testbench.tb', 40, ''),
            ('combinational', 'MUX', 'netlist', 'examples/component_file_io/combinational/mux/netlist.table', 'examples/component_file_io/combinational/mux/testbench.tb', 80, ''),
            ('combinational', 'NAND gate', 'netlist', 'examples/component_file_io/combinational/nand/netlist.table', 'examples/component_file_io/combinational/nand/testbench.tb', 40, ''),
            ('combinational', 'NOR gate', 'netlist', 'examples/component_file_io/combinational/nor/netlist.table', 'examples/component_file_io/combinational/nor/testbench.tb', 40, ''),
            ('combinational', 'NOT gate', 'netlist', 'examples/component_file_io/combinational/not/netlist.table', 'examples/component_file_io/combinational/not/testbench.tb', 40, ''),
            ('combinational', 'OR gate', 'netlist', 'examples/component_file_io/combinational/or/netlist.table', 'examples/component_file_io/combinational/or/testbench.tb', 40, ''),
            ('combinational', 'OR-NOT gate', 'netlist', 'examples/component_file_io/combinational/ornot/netlist.table', 'examples/component_file_io/combinational/ornot/testbench.tb', 40, ''),
            ('combinational', 'XOR gate', 'netlist', 'examples/component_file_io/combinational/xor/netlist.table', 'examples/component_file_io/combinational/xor/testbench.tb', 40, ''),
            # Sequential examples: all are clocked .table netlists, not FSM .net files.
            ('sequential', 'D Flip-Flop (positive edge)', 'netlist', 'examples/component_file_io/sequential/dff_positive/netlist.table', 'examples/component_file_io/sequential/dff_positive/testbench.tb', 50, 'Positive-edge triggered D flip-flop.'),
            ('sequential', 'D Flip-Flop (negative edge)', 'netlist', 'examples/component_file_io/sequential/dff_negative/netlist.table', 'examples/component_file_io/sequential/dff_negative/testbench.tb', 50, 'Negative-edge triggered D flip-flop.'),
            ('sequential', 'JK Flip-Flop (positive edge)', 'netlist', 'examples/component_file_io/sequential/jkff_positive/netlist.table', 'examples/component_file_io/sequential/jkff_positive/testbench.tb', 50, ''),
            ('sequential', 'JK Flip-Flop (negative edge)', 'netlist', 'examples/component_file_io/sequential/jkff_negative/netlist.table', 'examples/component_file_io/sequential/jkff_negative/testbench.tb', 50, ''),
            ('sequential', 'T Flip-Flop (positive edge)', 'netlist', 'examples/component_file_io/sequential/tff_positive/netlist.table', 'examples/component_file_io/sequential/tff_positive/testbench.tb', 50, ''),
            ('sequential', 'T Flip-Flop (negative edge)', 'netlist', 'examples/component_file_io/sequential/tff_negative/netlist.table', 'examples/component_file_io/sequential/tff_negative/testbench.tb', 50, ''),
            ('sequential', '4-bit Shift Register', 'netlist', 'examples/component_file_io/sequential/shift_register/netlist.table', 'examples/component_file_io/sequential/shift_register/testbench.tb', 50, ''),
            # FSM examples
            ('fsm', 'Traffic Light FSM', 'fsm', 'tests/netlists/fsm/traffic_light/traffic_light_fsm.net', 'examples/component_file_io/fsm/traffic_light/testbench.tb', 60, 'Green → Yellow → Red → Green.'),
            ('fsm', 'Vending Machine FSM', 'fsm', 'examples/vending_machine/vending_machine.net', 'examples/component_file_io/fsm/vending_machine/testbench.tb', 46, '10-unit item; accepts 5-unit and 10-unit coins.'),
        ]
        catalog = []
        for category, name, circuit_type, netlist, tb_path, max_time, description in entries:
            net_full = os.path.join(PROJECT_ROOT, netlist)
            tb_full = os.path.join(PROJECT_ROOT, tb_path)
            if not os.path.isfile(net_full) or not os.path.isfile(tb_full):
                continue
            with open(tb_full, encoding='utf-8') as fh:
                source = fh.read()
            directives = []
            clock_period = ''
            for raw in source.splitlines():
                line = raw.strip()
                if not line or line.startswith('#') or line.startswith(('NETLIST ', 'FSM ', 'MAX_TIME ')):
                    continue
                if line.startswith('CLOCK '):
                    parts = line.split()
                    if len(parts) >= 3:
                        clock_period = parts[2]
                    continue
                directives.append(line)
            catalog.append({
                'id': category + ':' + name.lower().replace(' ', '-').replace('(', '').replace(')', ''),
                'category': category, 'name': name, 'circuitType': circuit_type,
                'netlist': netlist, 'testbench': tb_path, 'maxTime': max_time,
                'clockPeriod': clock_period, 'description': description,
                'stimuli': '\n'.join(directives)
            })
        self.send_json({'success': True, 'circuits': catalog,
                        'categories': ['combinational', 'sequential', 'fsm']})

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
        try:
            n = int(self.headers.get('Content-Length', '0'))
            if not 0 < n <= 1_000_000:
                raise ValueError('Invalid testbench size.')
            req = json.loads(self.rfile.read(n).decode('utf-8'))
            script = req.get('testbench', '')
            if not isinstance(script,str) or not script.strip():
                raise ValueError('Testbench is empty.')
            res,wave = self.run_generated_testbench(script)
            self.send_json({'success':res.returncode == 0,'stdout':res.stdout,'stderr':res.stderr,'waveform':wave},
                           200 if res.returncode == 0 else 400)
        except Exception as exc:
            self.send_json({'success':False,'error':str(exc)},400)

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

    def handle_file_simulate(self):
        """Run uploaded .tb script against an uploaded .table/.net circuit."""
        try:
            n = int(self.headers.get('Content-Length', '0'))
            if not 0 < n <= 3_000_000:
                raise ValueError('Input files exceed 3 MB request limit.')
            req = json.loads(self.rfile.read(n).decode('utf-8'))
            tb = req.get('testbench', '')
            net = req.get('netlistContent', '')
            net_name = os.path.basename(str(req.get('netlistName', '')))
            if not isinstance(tb, str) or not isinstance(net, str) or not tb.strip() or not net.strip():
                raise ValueError('Choose a nonempty netlist and testbench input file.')
            if len(tb) > 1_000_000 or len(net) > 1_000_000 or not net_name.lower().endswith(('.table', '.tbl', '.net')):
                raise ValueError('Netlist must be .table, .tbl or .net; each file must be under 1 MB.')
            # Keep the original directives, replacing only the selected circuit source.
            build_dir = os.path.join(PROJECT_ROOT, 'build')
            os.makedirs(build_dir, exist_ok=True)
            with tempfile.TemporaryDirectory(prefix='slms_file_', dir=build_dir) as td:
                circuit = os.path.join(td, 'uploaded' + os.path.splitext(net_name)[1].lower())
                with open(circuit, 'w', encoding='utf-8') as f:
                    f.write(net)
                kind = 'FSM' if net_name.lower().endswith('.net') else 'NETLIST'
                lines = tb.splitlines()
                found = False
                for i,line in enumerate(lines):
                    if re.match(r'^\s*(?:CIRCUIT|FSM|NETLIST)\s+', line, re.I):
                        lines[i] = kind + ' ' + os.path.relpath(circuit, PROJECT_ROOT)
                        found = True
                        break
                if not found:
                    lines.insert(0, kind + ' ' + os.path.relpath(circuit, PROJECT_ROOT))
                script = '\n'.join(lines) + '\n'
                res, wave = self.run_generated_testbench(script, timeout=35)
            self.send_json({'success': res.returncode == 0, 'stdout':res.stdout,
                            'stderr':res.stderr,'waveform':wave},
                           200 if res.returncode == 0 and wave.get('times') else 400)
        except Exception as exc:
            self.send_json({'success':False,'error':str(exc)},400)

    def handle_generic_simulate(self):
        """Run a user-selected .table combinational netlist or .net FSM with timed inputs."""
        try:
            n = int(self.headers.get('Content-Length', '0'))
            if n <= 0 or n > 1_000_000:
                raise ValueError('Invalid request size.')
            req = json.loads(self.rfile.read(n).decode('utf-8'))
            circuit_type = str(req.get('circuitType', 'netlist')).lower()
            circuit_path = resolve_project_path(req.get('netlist', ''))
            full_path = os.path.join(PROJECT_ROOT, circuit_path)
            if circuit_type == 'fsm' and not circuit_path.endswith('.net'):
                raise ValueError('FSM simulation expects a .net file.')
            if circuit_type != 'fsm' and not circuit_path.endswith(('.table', '.tbl')):
                raise ValueError('Combinational netlist simulation expects a .table file.')
            max_time = int(req.get('maxTime', 40))
            if max_time < 1 or max_time > 100000:
                raise ValueError('Maximum time must be between 1 and 100000.')
            lines = [('FSM ' if circuit_type == 'fsm' else 'NETLIST ') + circuit_path]
            clock = str(req.get('clockPeriod', '')).strip()
            if clock:
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
                # Reference testbenches use the simulator's native ASSERT and INVARIANT syntax.
                if raw.startswith('ASSERT '):
                    assertion = re.fullmatch(r'ASSERT\s+@(\d+)\s+(.+)', raw)
                    if not assertion:
                        raise ValueError(f'Invalid assertion directive: {raw!r}')
                    if int(assertion.group(1)) > max_time:
                        raise ValueError(f'Assertion time {assertion.group(1)} exceeds MAX_TIME {max_time}.')
                    lines.append(raw)
                    continue
                if raw.startswith('INVARIANT '):
                    if not re.fullmatch(r'INVARIANT\s+[A-Za-z_][A-Za-z0-9_]*\s+.+', raw):
                        raise ValueError(f'Invalid invariant directive: {raw!r}')
                    lines.append(raw)
                    continue
                # Accept `0: a=0 b=1 | y=0` or `@0 a=0 b=1 | y=0`.
                match = re.fullmatch(r'@?(\d+)\s*:\s*(.+)|@(\d+)\s+(.+)', raw)
                if not match:
                    raise ValueError(f'Invalid input/assertion line: {raw!r}. Use timed assignments, ASSERT, or INVARIANT.')
                time = int(match.group(1) or match.group(3))
                event_text = match.group(2) or match.group(4)
                if time > max_time:
                    raise ValueError(f'Input time {time} exceeds MAX_TIME {max_time}.')
                pieces = event_text.split('|')
                if len(pieces) > 2:
                    raise ValueError(f'Only one `|` separator is allowed at time {time}.')
                assignments = pieces[0].strip()
                expected = pieces[1].strip() if len(pieces) == 2 else ''
                assignment_pattern = r'[A-Za-z_][A-Za-z0-9_]*(?:\[-?\d+\])?\s*=\s*[01]'
                if not re.fullmatch(r'\s*' + assignment_pattern + r'(?:\s+' + assignment_pattern + r')*\s*', assignments):
                    raise ValueError(f'Invalid input assignments at time {time}: use signal=0/1 pairs.')
                lines.append(f'@{time} {assignments}')
                if expected:
                    if not re.fullmatch(r'\s*' + assignment_pattern + r'(?:\s+' + assignment_pattern + r')*\s*', expected):
                        raise ValueError(f'Invalid expected outputs at time {time}: use `| output=0` pairs.')
                    for item in expected.split():
                        signal, bit = item.split('=', 1)
                        lines.append(f'ASSERT @{time} {signal}={bit} "GUI expected output"')
                events += 1
                if events > MAX_GENERIC_EVENTS:
                    raise ValueError(f'At most {MAX_GENERIC_EVENTS} input events are allowed.')
            if not events:
                raise ValueError('Add at least one input event.')
            generated = '\n'.join(lines) + '\n'
            res, wave = self.run_generated_testbench(generated)
            summary = assertion_summary(res.stdout, res.stderr)
            # A simulation can produce a valid waveform even when an assertion fails.
            # Report execution and verification separately so failures remain visible.
            executed = bool(wave.get('times'))
            self.send_json({'success': executed, 'engineExitCode': res.returncode,
                            'verification': ('NOT_VERIFIED' if summary['total'] == 0 else
                                             'FAILED' if summary['failed'] else 'PASSED'),
                            'stdout': res.stdout, 'stderr': res.stderr, 'waveform': wave,
                            'generatedTestbench': generated, 'netlistPath': circuit_path, 'assertionSummary': summary},
                           200 if executed else 400)
        except Exception as exc:
            self.send_json({'success': False, 'error': str(exc)}, 400)

    def handle_vending(self):
        """Run the bundled vending FSM and assert each coin's expected response."""
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
                'ASSERT @6 vend=0 credit5=0 "Reset returns vending FSM to idle"',
            ]
            state = 'Idle'
            assertion_times = [6]
            for i, coin in enumerate(coins):
                start = 8 + 20 * i
                response_time = start + (8 if state == 'Idle' and coin == 5 else 2)
                # A 5-unit coin from Idle is stored at the next active clock edge.
                # A 10-unit coin, or a coin received while Credit5 is stored,
                # produces a Mealy response while the coin input is asserted.
                lines.append(f'@{start} coin5={int(coin == 5)} coin10={int(coin == 10)}')
                lines.append(f'@{start + 8} coin5=0 coin10=0')
                if state == 'Idle' and coin == 5:
                    vend, credit5, state = 0, 1, 'Credit5'
                elif state == 'Idle' and coin == 10:
                    vend, credit5, state = 1, 0, 'Idle'
                elif state == 'Credit5':
                    # The bundled FSM's truth table asserts both outputs while
                    # the second coin is present, then returns to Idle.
                    vend, credit5, state = 1, 1, 'Idle'
                else:
                    raise ValueError('Unsupported vending FSM state transition.')
                lines.append(f'ASSERT @{response_time} vend={vend} credit5={credit5} "Vending response for coin {i + 1}"')
                assertion_times.append(response_time)
            generated = '\n'.join(lines) + '\n'
            res, wave = self.run_generated_testbench(generated)
            if not wave.get('times'):
                raise RuntimeError((res.stderr + '\n' + res.stdout)[-2500:] or 'Vending simulation returned no waveform.')
            summary = assertion_summary(res.stdout, res.stderr)
            target_time = assertion_times[-1]
            sample = next((i for i, t in enumerate(wave['times']) if t >= target_time), len(wave['times']) - 1)
            signals = {name: int(values[sample]) for name, values in wave.get('data', {}).items() if sample < len(values)}
            response = {'success': True, 'engineExitCode': res.returncode,
                        'verification': ('NOT_VERIFIED' if summary['total'] == 0 else
                                         'FAILED' if summary['failed'] else 'PASSED'),
                        'assertionSummary': summary, 'generatedTestbench': generated,
                        'coins': coins, 'time': wave['times'][sample], 'signals': signals,
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

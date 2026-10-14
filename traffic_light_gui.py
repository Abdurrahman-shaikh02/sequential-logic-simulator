
import re
import subprocess
import tkinter as tk
from pathlib import Path
from tkinter import ttk, messagebox

ROOT = Path(__file__).resolve().parent
EXE = ROOT / "tests" / "worklet3" / "traffic_light_fsm_test.exe"

DEFAULT_CYCLES = [
    (1, 1, 0, 0),
    (0, 0, 1, 0),
    (1, 0, 0, 1),
    (1, 1, 0, 0),
]

COLORS = {
    "RED": "#ef4444",
    "GREEN": "#22c55e",
    "YELLOW": "#facc15",
}


class TrafficLightGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Traffic-Light FSM Simulator | Worklet 3")
        self.root.geometry("1200x850")
        self.root.minsize(850, 600)

        self.rows = []
        self.last_output = ""

        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass

        style.configure("Title.TLabel", font=("Segoe UI", 19, "bold"))
        style.configure("Section.TLabel", font=("Segoe UI", 11, "bold"))
        style.configure("Treeview", rowheight=25)
        style.configure("Treeview.Heading", font=("Segoe UI", 9, "bold"))

        # Scrollable main area
        outer = ttk.Frame(root)
        outer.pack(fill="both", expand=True)

        self.scroll_canvas = tk.Canvas(
            outer, highlightthickness=0, background="#e8e8e8"
        )
        scrollbar = ttk.Scrollbar(
            outer, orient="vertical", command=self.scroll_canvas.yview
        )
        self.scroll_canvas.configure(yscrollcommand=scrollbar.set)

        scrollbar.pack(side="right", fill="y")
        self.scroll_canvas.pack(side="left", fill="both", expand=True)

        self.main = ttk.Frame(self.scroll_canvas, padding=14)
        self.window_id = self.scroll_canvas.create_window(
            (0, 0), window=self.main, anchor="nw"
        )

        self.main.bind(
            "<Configure>",
            lambda event: self.scroll_canvas.configure(
                scrollregion=self.scroll_canvas.bbox("all")
            ),
        )
        self.scroll_canvas.bind(
            "<Configure>",
            lambda event: self.scroll_canvas.itemconfigure(
                self.window_id, width=event.width
            ),
        )

        # Mouse-wheel scrolling
        self.scroll_canvas.bind_all("<MouseWheel>", self.on_mousewheel)

        # Header
        ttk.Label(
            self.main,
            text="Traffic-Light FSM Simulator",
            style="Title.TLabel",
        ).pack(anchor="w")

        ttk.Label(
            self.main,
            text="Worklet 3 | C++ Sequential Logic Simulator",
        ).pack(anchor="w", pady=(2, 12))

        # Traffic lights and cycle input area
        top = ttk.Frame(self.main)
        top.pack(fill="x", pady=5)

        light_box = ttk.LabelFrame(
            top, text="Traffic Lights — R G Y", padding=10
        )
        light_box.pack(side="left", fill="y", padx=(0, 10))

        self.lights = {}
        lamps = ttk.Frame(light_box)
        lamps.pack()

        for name in ("RED", "GREEN", "YELLOW"):
            panel = ttk.Frame(lamps, padding=4)
            panel.pack(side="left")

            canvas = tk.Canvas(
                panel,
                width=66,
                height=66,
                background="#20242b",
                highlightthickness=0,
            )
            canvas.pack()

            canvas.create_oval(
                6, 6, 60, 60,
                fill="#454950",
                outline="#777777",
                width=2,
                tags="lamp",
            )

            ttk.Label(panel, text=name, font=("Segoe UI", 9, "bold")).pack(
                pady=4
            )
            self.lights[name] = (canvas, COLORS[name])

        self.state_label = ttk.Label(
            light_box, text="Current state: RED", style="Section.TLabel"
        )
        self.state_label.pack(pady=5)

        setup = ttk.LabelFrame(top, text="Cycle Inputs", padding=10)
        setup.pack(side="left", fill="both", expand=True)

        controls = ttk.Frame(setup)
        controls.pack(fill="x", pady=(0, 5))

        ttk.Label(controls, text="Number of cycles:").pack(side="left")

        self.cycle_count = tk.StringVar(value="4")
        ttk.Spinbox(
            controls,
            from_=1,
            to=100,
            width=6,
            textvariable=self.cycle_count,
        ).pack(side="left", padx=6)

        ttk.Button(
            controls, text="Update Rows", command=self.rebuild_cycle_rows
        ).pack(side="left", padx=4)

        ttk.Label(
            setup,
            text="Choose 0 or 1 for en and expected outputs in R G Y order.",
        ).pack(anchor="w", pady=(0, 5))

        self.input_table = ttk.Frame(setup)
        self.input_table.pack(anchor="w")

        # Buttons and status
        actions = ttk.Frame(self.main)
        actions.pack(fill="x", pady=10)

        ttk.Button(
            actions, text="Run Simulation", command=self.run_simulation
        ).pack(side="left")

        ttk.Button(
            actions, text="Reset View", command=self.reset_view
        ).pack(side="left", padx=8)

        self.status = ttk.Label(
            actions, text="Ready | Initial state: RED"
        )
        self.status.pack(side="left", padx=10)

        # Results table
        results_box = ttk.LabelFrame(
            self.main, text="Cycle-by-Cycle Results", padding=8
        )
        results_box.pack(fill="x", pady=5)

        columns = (
            "cycle", "previous", "en", "expected", "actual",
            "expected_out", "actual_out", "result",
        )

        table_frame = ttk.Frame(results_box)
        table_frame.pack(fill="x", expand=True)

        self.table = ttk.Treeview(
            table_frame, columns=columns, show="headings", height=6
        )

        headings = [
            ("cycle", "Cycle", 55),
            ("previous", "Previous", 85),
            ("en", "en", 45),
            ("expected", "Expected State", 110),
            ("actual", "Actual State", 110),
            ("expected_out", "Expected R G Y", 115),
            ("actual_out", "Actual R G Y", 115),
            ("result", "Result", 75),
        ]

        for key, title, width in headings:
            self.table.heading(key, text=title)
            self.table.column(key, width=width, minwidth=45, anchor="center")

        table_scroll = ttk.Scrollbar(
            table_frame, orient="horizontal", command=self.table.xview
        )
        self.table.configure(xscrollcommand=table_scroll.set)
        self.table.pack(fill="x", expand=True)
        table_scroll.pack(fill="x")

        self.table.tag_configure("PASS", foreground="#16803c")
        self.table.tag_configure("FAIL", foreground="#c62828")

        # Waveform
        waveform_box = ttk.LabelFrame(
            self.main, text="Waveform Preview", padding=8
        )
        waveform_box.pack(fill="x", pady=5)

        self.wave = tk.Canvas(
            waveform_box,
            height=190,
            background="#111827",
            highlightthickness=0,
        )
        self.wave.pack(fill="x", expand=True)

        # C++ output
        log_box = ttk.LabelFrame(
            self.main, text="C++ Assertions and Output", padding=8
        )
        log_box.pack(fill="both", expand=True, pady=5)

        self.log = tk.Text(
            log_box,
            height=10,
            wrap="word",
            font=("Consolas", 9),
            background="#111827",
            foreground="#e5e7eb",
        )
        log_scroll = ttk.Scrollbar(
            log_box, orient="vertical", command=self.log.yview
        )
        self.log.configure(yscrollcommand=log_scroll.set)
        log_scroll.pack(side="right", fill="y")
        self.log.pack(fill="both", expand=True)

        self.rebuild_cycle_rows()
        self.draw_lights("RED")
        self.draw_waveform([])

    def on_mousewheel(self, event):
        self.scroll_canvas.yview_scroll(
            int(-1 * (event.delta / 120)), "units"
        )

    def rebuild_cycle_rows(self):
        try:
            count = int(self.cycle_count.get())
        except (ValueError, tk.TclError):
            return

        if not 1 <= count <= 100:
            messagebox.showwarning(
                "Invalid cycle count",
                "Choose a number from 1 to 100.",
            )
            return

        # Preserve current values where possible
        previous_values = []
        for row in self.rows:
            previous_values.append(tuple(var.get() for var in row))

        for widget in self.input_table.winfo_children():
            widget.destroy()

        headings = ["Cycle", "en", "R", "G", "Y"]
        for col, heading in enumerate(headings):
            ttk.Label(
                self.input_table,
                text=heading,
                font=("Segoe UI", 9, "bold"),
            ).grid(row=0, column=col, padx=7, pady=3)

        self.rows = []

        for i in range(count):
            defaults = (
                previous_values[i]
                if i < len(previous_values)
                else tuple(str(v) for v in (
                    DEFAULT_CYCLES[i]
                    if i < len(DEFAULT_CYCLES)
                    else (1, 0, 0, 0)
                ))
            )

            ttk.Label(self.input_table, text=str(i + 1)).grid(
                row=i + 1, column=0, padx=7, pady=3
            )

            variables = []
            for col, value in enumerate(defaults, start=1):
                var = tk.StringVar(value=str(value))
                ttk.Combobox(
                    self.input_table,
                    textvariable=var,
                    values=("0", "1"),
                    width=5,
                    state="readonly",
                ).grid(row=i + 1, column=col, padx=7, pady=3)
                variables.append(var)

            self.rows.append(variables)

        self.scroll_canvas.configure(
            scrollregion=self.scroll_canvas.bbox("all")
        )

    def draw_lights(self, active_state):
        self.state_label.config(text=f"Current state: {active_state}")

        for name, (canvas, color) in self.lights.items():
            canvas.delete("lamp")
            canvas.create_oval(
                6, 6, 60, 60,
                fill=color if name == active_state else "#454950",
                outline="#ffffff" if name == active_state else "#777777",
                width=3 if name == active_state else 2,
                tags="lamp",
            )

    def reset_view(self):
        for item in self.table.get_children():
            self.table.delete(item)

        self.log.delete("1.0", "end")
        self.status.config(text="Ready | Initial state: RED")
        self.draw_lights("RED")
        self.draw_waveform([])
        self.last_output = ""

    def run_simulation(self):
        if not EXE.exists():
            messagebox.showerror(
                "Executable not found",
                "Build the C++ testbench first in MSYS2 UCRT64:\n\n"
                "make tests/worklet3/traffic_light_fsm_test.exe",
            )
            return

        try:
            input_lines = [str(len(self.rows))]
            test_inputs = []

            for row in self.rows:
                values = [int(var.get()) for var in row]
                en, red, green, yellow = values
                input_lines.append(str(en))
                input_lines.append(f"{red} {green} {yellow}")
                test_inputs.append(values)

            result = subprocess.run(
                [str(EXE)],
                input="\n".join(input_lines) + "\n",
                text=True,
                capture_output=True,
                cwd=str(ROOT),
                timeout=20,
            )

            output = result.stdout
            if result.stderr:
                output += "\n" + result.stderr

            self.last_output = output
            self.log.delete("1.0", "end")
            self.log.insert("end", output)
            self.log.see("end")

            self.update_results(output)

            states = re.findall(
                r"Actual next state:\s*(RED|GREEN|YELLOW)", output
            )
            self.draw_lights(states[-1] if states else "RED")
            self.draw_waveform(test_inputs)

            passed = re.search(r"Passed assertions:\s*(\d+)", output)
            failed = re.search(r"Failed assertions:\s*(\d+)", output)

            p = passed.group(1) if passed else "?"
            f = failed.group(1) if failed else "?"

            self.status.config(
                text=f"Finished | {p} passed | {f} failed"
            )

            if result.returncode != 0:
                messagebox.showwarning(
                    "Testbench warning",
                    f"C++ testbench exit code: {result.returncode}\n"
                    "Check the C++ output panel.",
                )

        except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
            messagebox.showerror("Simulation error", str(exc))

    def update_results(self, output):
        for item in self.table.get_children():
            self.table.delete(item)

        pattern = re.compile(
            r"Cycle\s+(\d+).*?"
            r"Previous state:\s*(RED|GREEN|YELLOW).*?"
            r"Input en:\s*([01]).*?"
            r"Expected next state:\s*(RED|GREEN|YELLOW).*?"
            r"Actual next state:\s*(RED|GREEN|YELLOW).*?"
            r"Expected outputs R G Y:\s*([01])\s+([01])\s+([01]).*?"
            r"Actual outputs\s+R G Y:\s*([01])\s+([01])\s+([01]).*?"
            r"\[(PASS|FAIL)\]\s*State transition",
            re.S,
        )

        for match in pattern.finditer(output):
            (
                cycle, previous, en, expected, actual,
                er, eg, ey, ar, ag, ay, assertion_result,
            ) = match.groups()

            expected_out = f"{er} {eg} {ey}"
            actual_out = f"{ar} {ag} {ay}"

            passed = (
                expected == actual
                and expected_out == actual_out
                and assertion_result == "PASS"
            )
            status = "PASS" if passed else "FAIL"

            self.table.insert(
                "",
                "end",
                values=(
                    cycle, previous, en, expected, actual,
                    expected_out, actual_out, status,
                ),
                tags=(status,),
            )

    def draw_waveform(self, rows):
        self.wave.delete("all")

        width = max(self.wave.winfo_width(), 650)
        left = 55
        top = 15
        row_gap = 30
        signals = ["CLK", "en", "R", "G", "Y"]

        colors = {
            "CLK": "#93c5fd",
            "en": "#c4b5fd",
            "R": "#f87171",
            "G": "#4ade80",
            "Y": "#facc15",
        }

        for index, name in enumerate(signals):
            y = top + index * row_gap
            self.wave.create_text(
                8, y + 8, text=name, anchor="w",
                fill="white", font=("Consolas", 9, "bold"),
            )
            self.wave.create_line(
                left, y + 16, width - 10, y + 16, fill="#374151"
            )

        if not rows:
            self.wave.create_text(
                width // 2, 165,
                text="Run simulation to view signals",
                fill="#9ca3af",
            )
            return

        slot = (width - left - 20) / (len(rows) * 2)

        for index, name in enumerate(signals):
            ybase = top + index * row_gap + 16
            high = ybase - 12
            low = ybase + 1
            x = left
            previous_y = None

            for en, red, green, yellow in rows:
                values = {
                    "CLK": [0, 1],
                    "en": [en, en],
                    "R": [red, red],
                    "G": [green, green],
                    "Y": [yellow, yellow],
                }

                for value in values[name]:
                    current_y = high if value else low

                    if previous_y is not None:
                        self.wave.create_line(
                            x, previous_y, x, current_y,
                            fill=colors[name], width=2,
                        )

                    self.wave.create_line(
                        x, current_y, x + slot, current_y,
                        fill=colors[name], width=2,
                    )

                    previous_y = current_y
                    x += slot

        for i in range(len(rows)):
            x = left + (i * 2 + 1) * slot
            self.wave.create_text(
                x, top + len(signals) * row_gap + 3,
                text=f"C{i + 1}", fill="#e5e7eb",
                font=("Consolas", 8),
            )


if __name__ == "__main__":
    window = tk.Tk()
    app = TrafficLightGUI(window)
    window.mainloop()

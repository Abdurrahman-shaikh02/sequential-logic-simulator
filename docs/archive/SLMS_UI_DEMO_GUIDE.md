# SLMS GUI demonstration guide

## Start the application

From the project root:

```bash
make
./simulator --gui 8080
```

Open `http://localhost:8080` in a browser. Keep the terminal visible so the
reviewer can see the C++ simulator output/errors when a simulation runs.

## Suggested 5-minute demo

1. **Live traffic-light FSM**
   - Use Enable and Reset.
   - Press Reset, then press Step Clock several times.
   - Show Green -> Yellow -> Red -> Green and the live waveform.
   - Toggle Enable off and step again to show that the state holds.

2. **Generic combinational netlist**
   - In the generic simulation panel choose `Combinational netlist (.table)`.
   - Use `tests/netlists/combinational/and/netlist.table`.
   - Set maximum time to `40`.
   - Use events:

     ```text
     0: a=0 b=0
     10: a=1 b=0
     20: a=0 b=1
     30: a=1 b=1
     ```
   - Click **Run generic simulation**. Explain that the final AND output is 1
     only when both inputs are 1.

3. **Generic FSM testbench**
   - Select `FSM (.net)`.
   - Use `examples/vending_machine/vending_machine.net`.
   - Set clock period to `10` and maximum time to `60`.
   - Use events:

     ```text
     0: reset=1 coin5=0 coin10=0
     5: reset=0
     8: coin5=1 coin10=0
     16: coin5=0 coin10=0
     28: coin5=0 coin10=1
     36: coin5=0 coin10=0
     ```
   - Click **Run generic simulation** and show the generated testbench,
     signal list, samples, and waveform.

4. **Vending-machine controls**
   - Click **Reset session**.
   - Click **Insert 5**, then **Insert 5** again; show stored credit and vend.
   - Reset, then click **Insert 10** to demonstrate the direct vend path.

5. **Automated verification and export**
   - Click **Run All 7 Tests** and show the category results.
   - Open the Advanced section and show the custom testbench editor.
   - Run a custom testbench and use the VCD, TXT, and JSON download buttons.

## What to say about the UI

The GUI is a browser front end served by the Python standard-library HTTP
server. It sends simulation requests to the existing C++ simulator; it is not a
separate logic engine. The live traffic controls, generic netlist/FSM panel,
vending controls, automated test categories, waveform viewer, and waveform
exports demonstrate different ways to exercise the same simulator.

## Demo cautions

- Run `make` successfully before opening the browser.
- Run the server from the project root so relative netlist paths resolve.
- Use the exact input names declared by the selected circuit.
- The generic GUI currently offers clock period but not a separate duty/high
  duration field. To demonstrate an asymmetric clock, run the supplied
  `docs/ASYMMETRIC_CLOCK_DEMO.tb` from the terminal:

  ```bash
  ./simulator --testbench docs/ASYMMETRIC_CLOCK_DEMO.tb
  ```

  Its `CLOCK clk 10 3` line configures 3 units HIGH and 7 units LOW.

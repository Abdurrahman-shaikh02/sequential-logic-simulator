#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if [[ ! -x ./simulator ]]; then
  echo "Simulator is not built. Run: make -j2" >&2
  exit 1
fi

OUT="output/component_file_io"
mkdir -p "$OUT"

run_assertion_test() {
  local label="$1"
  local tb="$2"
  local log="$OUT/${label}_assertions.log"
  echo "[ASSERT] $label"
  ./simulator --testbench "$tb" > "$log" 2>&1
  if ! grep -Eq 'Total Evaluated : [1-9][0-9]*' "$log" || ! grep -q 'Failed          : 0' "$log"; then
    cat "$log"
    echo "FAIL: $label did not pass assertions" >&2
    exit 1
  fi
}

for gate in and andnot mux nand nor not or ornot xor; do
  echo "[FILE INPUT] $gate"
  log="$OUT/${gate}_file_input.log"
  printf '3\nexamples/component_file_io/combinational/%s/netlist.table\nexamples/component_file_io/combinational/%s/input.txt\n8\n' "$gate" "$gate" \
    | ./simulator > "$log" 2>&1
  grep -q 'Loaded .* input row' "$log"
  dest="$OUT/combinational/$gate"
  mkdir -p "$dest"
  cp output/interactive_results.txt output/interactive_waveform.txt \
     output/interactive_waveform.json output/interactive_simulation.vcd "$dest/"
  run_assertion_test "combinational_${gate}" "examples/component_file_io/combinational/$gate/testbench.tb"
done

for cell in dff_negative dff_positive jkff_negative jkff_positive tff_negative tff_positive shift_register; do
  run_assertion_test "sequential_${cell}" "examples/component_file_io/sequential/$cell/testbench.tb"
  dest="$OUT/sequential/$cell"
  mkdir -p "$dest"
  cp output/results.txt output/waveform.txt output/waveform.json output/simulation.vcd "$dest/"
done

for fsm in traffic_light vending_machine; do
  run_assertion_test "fsm_${fsm}" "examples/component_file_io/fsm/$fsm/testbench.tb"
  dest="$OUT/fsm/$fsm"
  mkdir -p "$dest"
  cp output/results.txt output/waveform.txt output/waveform.json output/simulation.vcd "$dest/"
done

echo
echo "ALL COMPONENT FILE-I/O AND ASSERTION CHECKS PASSED."
echo "Saved per-component outputs under: $OUT/"

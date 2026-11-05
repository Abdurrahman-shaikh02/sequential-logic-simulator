#!/usr/bin/env bash
# Checks, before the review, that every Demo 1 command still works.
# Prints one line per step. It is not part of the demo: type the commands
# from README.md yourself during the review.

cd "$(dirname "${BASH_SOURCE[0]}")"
fail=0
check() {   # check "description" command...
    if "${@:2}" > /dev/null 2>&1; then echo "PASS  $1"; else echo "FAIL  $1"; fail=1; fi
}
has() { "${@:2}" 2>&1 | grep -q -- "$1"; }

check "simulator is here and runs"         test -x ./simulator
check "1. dead states T V H"               has "  H  (1/2" ./simulator --fsm dead_unreachable.net
check "1. unreachable U V H"               has "unreachable: U V H" ./simulator --fsm dead_unreachable.net
check "2. synthesize the traffic light"    ./simulator --fsm traffic_light.net --synth -o traffic_light.table
check "3. parser accepts the netlist"      ./simulator --check traffic_light.table
check "4. testbench passes (exit 0)"       ./simulator --testbench traffic_light.tb
check "4. 31 of 31 assertions"             has "Passed          : 31 (100%)" ./simulator --testbench traffic_light.tb
check "5. output files written"            test -s output/simulation.vcd -a -s output/results.txt
check "6. inputs.txt: 30 of 30 expected"   has "Passed          : 30 (100%)" sh -c "printf 'traffic_light.table\ninputs.txt\n' | ./simulator --interactive"

rm -rf output dead_unreachable.table traffic_light.table   # leave the folder as it was
(( fail )) && echo "Something failed: run that step's command by hand to see why." || echo "All good."
exit $fail

#!/usr/bin/env bash
# Runs the archived (pre-refactor) standalone tests.
#
#   run.sh [filter]
#
# Every tests/archive/**/<name>.cpp was built to <name> in the same directory.
# Each test runs from its own source directory (they open their netlists by
# relative path), with input.txt on stdin when there is one. If the directory
# has an expected.txt, or <name>_expected.txt for test <name>t (analysist ->
# analysis_expected.txt), the output must match it, ignoring a missing final
# newline; otherwise the test only has to exit with status 0.

set -u

FILTER=${1:-}
ARCHIVE=$(cd "$(dirname "$0")" && pwd)

pass=0 fail=0 ran=0
failed=()

while IFS= read -r src; do
    rel=${src#"$ARCHIVE"/}
    rel=${rel%.cpp}
    [[ -n $FILTER && $rel != *$FILTER* ]] && continue

    dir=$(dirname "$src")
    bin=${src%.cpp}

    input=/dev/null
    [[ -f $dir/input.txt ]] && input=$dir/input.txt

    name=$(basename "$rel")
    expected=
    if [[ -f $dir/expected.txt ]]; then
        expected=$dir/expected.txt
    elif [[ -f $dir/${name%t}_expected.txt ]]; then
        expected=$dir/${name%t}_expected.txt
    fi

    out=$(cd "$dir" && timeout 10 "$bin" < "$input" 2>&1)
    status=$?

    if [[ $status -ne 0 ]]; then
        echo "FAIL  $rel (exit $status)"
        fail=$((fail + 1)); failed+=("$rel")
    elif [[ -n $expected ]]; then
        # $(...) already strips trailing newlines, so strip them from the file too
        if [[ $out == "$(cat "$expected")" ]]; then
            echo "PASS  $rel"
            pass=$((pass + 1))
        else
            echo "FAIL  $rel (output differs from ${expected#"$ARCHIVE"/})"
            diff <(printf '%s\n' "$out") <(printf '%s\n' "$(cat "$expected")") | head -n 10 | sed 's/^/      /'
            fail=$((fail + 1)); failed+=("$rel")
        fi
    else
        echo "RAN   $rel (no expected output to compare)"
        ran=$((ran + 1))
    fi
done < <(find "$ARCHIVE" -type f -name '*.cpp' | sort)

echo
echo "$pass passed, $fail failed, $ran ran without a check"
[[ $fail -eq 0 ]]

#!/bin/sh
# Command-line tests: exit codes, --check output and --json.
# Usage: tests/cli_test.sh <path-to-pdr> <sample-project>
PDR=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")  # absolute, so tests can cd
SAMPLE=$2
FAILED=0
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# expect <name> <exit-code> <text-in-output or ""> <pdr arguments...>
# Runs in the folder $IN when it is set.
expect() {
    name=$1 code=$2 text=$3
    shift 3
    (cd "${IN:-.}" && "$PDR" "$@") >"$TMP/out" 2>&1
    got=$?
    if [ "$got" -ne "$code" ]; then
        echo "FAIL  $name: exit $got, expected $code"
        sed 's/^/        /' "$TMP/out"
        FAILED=$((FAILED + 1))
    elif [ -n "$text" ] && ! grep -qF -- "$text" "$TMP/out"; then
        echo "FAIL  $name: output does not contain: $text"
        sed 's/^/        /' "$TMP/out"
        FAILED=$((FAILED + 1))
    else
        echo "PASS  $name"
    fi
}

mkdir -p "$TMP/clean/pkg"
printf 'from pkg import util\n' >"$TMP/clean/main.py"
printf '' >"$TMP/clean/pkg/__init__.py"
printf 'import os\n' >"$TMP/clean/pkg/util.py"

expect "check fails on the sample's cycle" 1 \
    "fad/actions/__init__.py:2: error: circular import: fad/actions/__init__.py -> fad/actions/move.py -> fad/rules.py -> fad/actions/__init__.py" \
    "$SAMPLE" --check
expect "check fails on the sample's broken import" 1 "fad/actions/archive.py:3: error: broken import '.compress'" "$SAMPLE" --check
expect "check passes when both are allowed" 0 "-> OK" "$SAMPLE" --check --allow-cycles --allow-broken
expect "check still fails on what is not allowed" 1 "broken import '.compress'" "$SAMPLE" --allow-cycles --check
expect "check passes on a clean project" 0 "pdr: 3 files, 0 circular imports, 0 broken imports -> OK" "$TMP/clean" --check
expect "check suggests which import to remove" 1 "fix: remove fad/rules.py:4:  from .actions import run_action" "$SAMPLE" --check
expect "check on this program's own C++ source" 0 "-> OK" "$(dirname "$0")/.." --check
expect "json output" 0 '"cycles": 1' "$SAMPLE" --json
expect "dot output" 0 "digraph dependencies {" "$SAMPLE" --dot
expect "why with a file name" 0 "fad/main.py needs fad/utils/paths.py" "$SAMPLE" --why main.py paths.py
expect "help" 0 "Exit codes" --help
# no folder given: the current folder is analysed
IN=$TMP/clean expect "current folder by default" 0 "pdr: 3 files, 0 circular imports, 0 broken imports -> OK" --check
IN=$SAMPLE expect "current folder with a command and files" 0 "fad/main.py needs fad/utils/paths.py" --why main.py paths.py
unset IN
expect "folder that does not exist" 2 "not a folder" "$TMP/nope" --check
mkdir -p "$TMP/empty"
expect "folder without code" 2 "no Python, JavaScript or C/C++ files" "$TMP/empty" --check
expect "unknown command" 2 "unknown command '--frobnicate'" "$SAMPLE" --frobnicate
expect "wrong argument count" 2 "--why takes 2 arguments, got 1" "$SAMPLE" --why main.py
expect "allow flag without --check" 2 "only work with --check" "$SAMPLE" --report --allow-cycles
expect "ambiguous file name" 2 "matches several files" "$SAMPLE" --impact __init__.py

# --json must be valid JSON (checked with whichever tool is installed)
"$PDR" "$SAMPLE" --json >"$TMP/out.json"
if command -v python3 >/dev/null 2>&1; then
    python3 -m json.tool "$TMP/out.json" >/dev/null && echo "PASS  json is valid (python3)" ||
        { echo "FAIL  json is not valid"; FAILED=$((FAILED + 1)); }
elif command -v node >/dev/null 2>&1; then
    node -e 'JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"))' "$TMP/out.json" &&
        echo "PASS  json is valid (node)" || { echo "FAIL  json is not valid"; FAILED=$((FAILED + 1)); }
else
    echo "SKIP  json validity (no python3 or node)"
fi

# --dot must be valid Graphviz
if command -v dot >/dev/null 2>&1; then
    "$PDR" "$SAMPLE" --dot | dot -Tsvg -o "$TMP/graph.svg" && echo "PASS  dot output renders (graphviz)" ||
        { echo "FAIL  dot output does not render"; FAILED=$((FAILED + 1)); }
else
    echo "SKIP  dot rendering (graphviz not installed)"
fi

echo
if [ "$FAILED" -eq 0 ]; then echo "all command-line tests passed"; else echo "$FAILED command-line test(s) failed"; fi
[ "$FAILED" -eq 0 ]

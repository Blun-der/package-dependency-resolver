#!/bin/sh
# Builds every figure of the report into report/build/fig/.
#   sh report/make_figures.sh        (needs: graphviz, ImageMagick, rsvg-convert; make -C cpp bench once)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
OUT=$HERE/build/fig
TMP=$HERE/build/tmp
PDR=$ROOT/cpp/build/pdr
SAMPLE=$ROOT/sample/fad_demo
mkdir -p "$OUT" "$TMP"
make -s -C "$ROOT/cpp" build/pdr

# diagrams
dot -Tpng -Gdpi=200 "$HERE/figures/architecture.dot" -o "$OUT/architecture.png"
dot -Tpng -Gdpi=200 "$HERE/figures/flowchart.dot" -o "$OUT/flowchart.png"

# graphs drawn by the program itself
(cd "$ROOT/cpp" && "$PDR" src --dot) | dot -Tpng -Gdpi=150 -o "$OUT/self-graph.png"
"$PDR" "$SAMPLE" --dot | dot -Tpng -Gdpi=150 -o "$OUT/sample-graph.png"
"$PDR" "$SAMPLE" --dot --rules "$SAMPLE.rules" | dot -Tpng -Gdpi=150 -o "$OUT/sample-rules.png"

# terminal output as images: a light terminal, so the pages print well
shot() {  # shot <name> <text file>
    fold -w 98 "$2" | sed 's/[[:space:]]*$//' >"$TMP/$1.folded"
    magick -background '#fbfbfa' -fill '#1d1d1b' -font DejaVu-Sans-Mono -pointsize 26 \
        "label:@$TMP/$1.folded" -bordercolor '#fbfbfa' -border 28 \
        -bordercolor '#b9b8b3' -border 2 "$OUT/$1.png"
}
run() {  # run <name> <shell command shown after the prompt>, run inside the sample folder
    name=$1
    shift
    { echo "\$ $*"; (cd "$SAMPLE" && PATH="$(dirname "$PDR"):$PATH" sh -c "$*") 2>&1 || true; } >"$TMP/$name.txt"
    shot "$name" "$TMP/$name.txt"
}

run menu "printf '0\\n' | pdr"
run summary "pdr --report --rules ../fad_demo.rules | head -17"
run check "pdr --check"
run why "pdr --why main.py paths.py"
run impact "pdr --impact paths.py"
run rules "pdr --check --rules ../fad_demo.rules --allow-cycles --allow-broken"
run unused "pdr --unused"

# Python's own error on the sample (in a copy with the missing file added) next to pdr's answer
COPY=$TMP/sample-copy
rm -rf "$COPY"
cp -r "$SAMPLE" "$COPY"
echo 'def zip_folder(*a): pass' >"$COPY/fad/actions/compress.py"
{
    echo '$ python3 -c "import fad.rules"'
    (cd "$COPY" && python3 -I -c "import sys; sys.path.insert(0, '.'); import fad.rules" 2>&1 | tail -6 |
        sed "s|$COPY/||g") || true
    echo
    echo '$ pdr --check'
    (cd "$COPY" && "$PDR" --check 2>&1) || true
} >"$TMP/python-vs-pdr.txt"
shot python-vs-pdr "$TMP/python-vs-pdr.txt"

# benchmark charts, redrawn by the C++ benchmark from the last make bench run
make -s -C "$ROOT/cpp" build/pdr_bench
"$ROOT/cpp/build/pdr_bench" --charts "$ROOT/cpp/build/bench.csv" "$OUT"
for svg in "$OUT"/bench-*.svg; do rsvg-convert -z 2 -o "${svg%.svg}.png" "$svg"; done
echo "figures in $OUT"

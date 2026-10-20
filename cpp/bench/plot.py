"""Draws the benchmark charts for the report from build/bench.csv.

    python3 bench/plot.py build/bench.csv build

Writes bench-graph.svg, bench-hash-find.svg, bench-hash-insert.svg and
bench-analyze.svg (and a .png of each when rsvg-convert is installed).
Plain SVG, no plotting library needed.
"""
import csv
import math
import shutil
import subprocess
import sys
from pathlib import Path

# Colors: the first three slots of a palette checked for color-blind readers;
# every series also has its own marker shape, so the charts work in grayscale.
SURFACE = "#ffffff"
TEXT = "#0b0b0b"
TEXT_2 = "#52514e"
GRID = "#e4e3df"
AXIS = "#9b9a95"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a"]
MARKERS = ["circle", "square", "triangle"]
FONT = "Helvetica, Arial, sans-serif"

W, H = 720, 430
LEFT, RIGHT, TOP, BOTTOM = 78, 150, 70, 62


def nice_step(span, count=5):
    raw = span / count
    mag = 10 ** math.floor(math.log10(raw))
    for m in (1, 2, 2.5, 5, 10):
        if raw <= m * mag:
            return m * mag
    return 10 * mag


def fmt(v):
    if v == 0:
        return "0"
    if abs(v) >= 1000:
        return f"{v:,.0f}"
    return f"{v:g}"


def marker(shape, x, y, color, r=4.5):
    ring = f'stroke="{SURFACE}" stroke-width="2"'
    if shape == "circle":
        return f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{color}" {ring}/>'
    if shape == "square":
        return f'<rect x="{x - r:.1f}" y="{y - r:.1f}" width="{2 * r}" height="{2 * r}" rx="1.5" fill="{color}" {ring}/>'
    h = r * 1.2
    pts = f"{x:.1f},{y - h:.1f} {x + h:.1f},{y + h * 0.8:.1f} {x - h:.1f},{y + h * 0.8:.1f}"
    return f'<polygon points="{pts}" fill="{color}" {ring}/>'


def line_chart(path, title, subtitle, x_label, y_label, series):
    """series: list of (name, [(x, y), ...]). Linear axes from zero."""
    x_max = max(x for _, pts in series for x, _ in pts)
    y_max = max(y for _, pts in series for _, y in pts)
    xs, ys = nice_step(x_max), nice_step(y_max)
    x_top, y_top = math.ceil(x_max / xs) * xs, math.ceil(y_max / ys) * ys
    pw, ph = W - LEFT - RIGHT, H - TOP - BOTTOM

    def px(x):
        return LEFT + x / x_top * pw

    def py(y):
        return TOP + ph - y / y_top * ph

    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" '
        f'font-family="{FONT}">',
        f'<rect width="{W}" height="{H}" fill="{SURFACE}"/>',
        f'<text x="{LEFT}" y="28" font-size="17" font-weight="bold" fill="{TEXT}">{title}</text>',
        f'<text x="{LEFT}" y="48" font-size="12.5" fill="{TEXT_2}">{subtitle}</text>',
    ]
    k = 0
    while k * ys <= y_top + 1e-9:
        y = py(k * ys)
        out.append(f'<line x1="{LEFT}" x2="{LEFT + pw}" y1="{y:.1f}" y2="{y:.1f}" stroke="{GRID}" stroke-width="1"/>')
        out.append(f'<text x="{LEFT - 8}" y="{y + 4:.1f}" font-size="12" fill="{TEXT_2}" text-anchor="end">{fmt(k * ys)}</text>')
        k += 1
    k = 0
    while k * xs <= x_top + 1e-9:
        x = px(k * xs)
        out.append(f'<line x1="{x:.1f}" x2="{x:.1f}" y1="{TOP + ph}" y2="{TOP + ph + 5}" stroke="{AXIS}"/>')
        out.append(f'<text x="{x:.1f}" y="{TOP + ph + 20}" font-size="12" fill="{TEXT_2}" text-anchor="middle">{fmt(k * xs)}</text>')
        k += 1
    out.append(f'<line x1="{LEFT}" x2="{LEFT + pw}" y1="{TOP + ph}" y2="{TOP + ph}" stroke="{AXIS}" stroke-width="1"/>')
    out.append(f'<text x="{LEFT + pw / 2}" y="{H - 16}" font-size="13" fill="{TEXT}" text-anchor="middle">{x_label}</text>')
    out.append(f'<text transform="translate(20 {TOP + ph / 2}) rotate(-90)" font-size="13" fill="{TEXT}" '
               f'text-anchor="middle">{y_label}</text>')

    # direct labels at the line ends, nudged apart so they never overlap
    # (one series needs none: the title names it)
    ends = []
    for i, (name, pts) in enumerate(series):
        color, shape = SERIES[i], MARKERS[i]
        d = " ".join(f"{'M' if j == 0 else 'L'}{px(x):.1f},{py(y):.1f}" for j, (x, y) in enumerate(pts))
        out.append(f'<path d="{d}" fill="none" stroke="{color}" stroke-width="2" stroke-linejoin="round"/>')
        out.extend(marker(shape, px(x), py(y), color) for x, y in pts)
        ends.append([py(pts[-1][1]), name, i])
    ends.sort()
    if len(series) == 1:
        ends = []
    for j in range(1, len(ends)):
        ends[j][0] = max(ends[j][0], ends[j - 1][0] + 17)
    for y, name, i in ends:
        out.append(f'<text x="{LEFT + pw + 12}" y="{y + 4:.1f}" font-size="12.5" fill="{TEXT}">{name}</text>')

    out.append("</svg>")
    Path(path).write_text("\n".join(out) + "\n")
    if shutil.which("rsvg-convert"):
        subprocess.run(["rsvg-convert", "-z", "2", "-o", str(Path(path).with_suffix(".png")), str(path)], check=True)


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: plot.py <bench.csv> <output-folder>")
    rows = {"graph": [], "hashmap": [], "analyze": []}
    with open(sys.argv[1]) as f:
        for row in csv.reader(line for line in f if not line.startswith("#")):
            rows[row[0]].append([float(v) for v in row[1:]])
    out = Path(sys.argv[2])

    g = rows["graph"]
    line_chart(out / "bench-graph.svg",
               "Graph algorithms stay close to linear as projects grow",
               "Time against files + import links (V + E), generated projects, median of 5 runs",
               "Files + import links (millions)", "Time (ms)",
               [("Kahn (build order)", [((v + e) / 1e6, k) for v, e, _, k, _ in g]),
                ("Tarjan (cycles)", [((v + e) / 1e6, t) for v, e, t, _, _ in g]),
                ("BFS (impact)", [((v + e) / 1e6, b) for v, e, _, _, b in g])])

    h = rows["hashmap"]
    line_chart(out / "bench-hash-find.svg",
               "Looking up n keys and n missing keys",
               "Our chained HashMap against std::unordered_map, median of 5 runs",
               "Keys in the table (millions)", "Time (ms)",
               [("Our HashMap", [(n / 1e6, of) for n, _, _, of, _ in h]),
                ("std::unordered_map", [(n / 1e6, sf) for n, _, _, _, sf in h])])
    line_chart(out / "bench-hash-insert.svg",
               "Inserting n keys",
               "Our chained HashMap against std::unordered_map, median of 5 runs",
               "Keys inserted (millions)", "Time (ms)",
               [("Our HashMap", [(n / 1e6, oi) for n, oi, _, _, _ in h]),
                ("std::unordered_map", [(n / 1e6, si) for n, _, si, _, _ in h])])

    a = rows["analyze"]
    line_chart(out / "bench-analyze.svg",
               "Whole analysis grows faster than linearly",
               "The critical-files ranking runs one BFS per file: O(V × (V + E)), median of 3 runs",
               "Files in the project (thousands)", "Time (ms)",
               [("Whole analysis", [(v / 1e3, ms) for v, _, ms in a])])
    print(f"wrote bench-graph, bench-hash-find, bench-hash-insert and bench-analyze (.svg/.png) to {out}")


if __name__ == "__main__":
    main()

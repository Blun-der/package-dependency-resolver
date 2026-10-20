// Package Dependency Resolver - benchmark charts as SVG
//
// Line charts drawn directly as SVG text, so the benchmark needs no plotting
// library. Linear axes from zero; every series has its own color and marker
// shape (circle, square, triangle), so the charts also read in grayscale, and
// its name is written next to the end of its line.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace bench {

struct Series {
    std::string name;
    std::vector<std::pair<double, double>> points;  // (x, y)
};

// The first three slots of a palette checked for color-blind readers.
const char* const COLORS[] = {"#2a78d6", "#eb6834", "#1baf7a"};
const char* const SURFACE = "#ffffff";
const char* const TEXT = "#0b0b0b";
const char* const TEXT_2 = "#52514e";
const char* const GRID = "#e4e3df";
const char* const AXIS = "#9b9a95";
const char* const FONT = "Helvetica, Arial, sans-serif";
const int W = 720, H = 430, LEFT = 78, RIGHT = 150, TOP = 70, BOTTOM = 62;

// A round step that splits `span` into about five parts: 1, 2, 2.5 or 5 times a power of ten.
inline double niceStep(double span) {
    const double raw = span / 5;
    const double mag = std::pow(10, std::floor(std::log10(raw)));
    for (double m : {1.0, 2.0, 2.5, 5.0, 10.0})
        if (raw <= m * mag) return m * mag;
    return 10 * mag;
}

// 0, 0.5, 2.5, 400, "2,500"
inline std::string number(double v) {
    char buf[64];
    if (std::fabs(v) < 1000) {
        std::snprintf(buf, sizeof buf, "%g", v);
        return buf;
    }
    std::snprintf(buf, sizeof buf, "%.0f", v);
    std::string digits = buf, out;
    for (size_t i = 0; i < digits.size(); i++) {
        if (i > 0 && (digits.size() - i) % 3 == 0) out += ',';
        out += digits[i];
    }
    return out;
}

inline std::string fixed1(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", v);
    return buf;
}

inline std::string marker(int shape, double x, double y, const char* color, double r = 4.5) {
    const std::string ring = std::string("fill=\"") + color + "\" stroke=\"" + SURFACE + "\" stroke-width=\"2\"";
    if (shape == 0)
        return "<circle cx=\"" + fixed1(x) + "\" cy=\"" + fixed1(y) + "\" r=\"" + fixed1(r) + "\" " + ring + "/>";
    if (shape == 1)
        return "<rect x=\"" + fixed1(x - r) + "\" y=\"" + fixed1(y - r) + "\" width=\"" + fixed1(2 * r) + "\" height=\"" +
               fixed1(2 * r) + "\" rx=\"1.5\" " + ring + "/>";
    const double h = r * 1.2;
    return "<polygon points=\"" + fixed1(x) + "," + fixed1(y - h) + " " + fixed1(x + h) + "," + fixed1(y + h * 0.8) + " " +
           fixed1(x - h) + "," + fixed1(y + h * 0.8) + "\" " + ring + "/>";
}

inline void lineChart(const std::string& path, const std::string& title, const std::string& subtitle,
                      const std::string& xLabel, const std::string& yLabel, const std::vector<Series>& series) {
    double xMax = 0, yMax = 0;
    for (const Series& s : series)
        for (const auto& [x, y] : s.points) {
            xMax = std::max(xMax, x);
            yMax = std::max(yMax, y);
        }
    const double xs = niceStep(xMax), ys = niceStep(yMax);
    const double xTop = std::ceil(xMax / xs) * xs, yTop = std::ceil(yMax / ys) * ys;
    const double pw = W - LEFT - RIGHT, ph = H - TOP - BOTTOM;
    auto px = [&](double x) { return LEFT + x / xTop * pw; };
    auto py = [&](double y) { return TOP + ph - y / yTop * ph; };

    std::ostringstream o;
    o << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << W << "\" height=\"" << H << "\" viewBox=\"0 0 " << W << " "
      << H << "\" font-family=\"" << FONT << "\">\n"
      << "<rect width=\"" << W << "\" height=\"" << H << "\" fill=\"" << SURFACE << "\"/>\n"
      << "<text x=\"" << LEFT << "\" y=\"28\" font-size=\"17\" font-weight=\"bold\" fill=\"" << TEXT << "\">" << title << "</text>\n"
      << "<text x=\"" << LEFT << "\" y=\"48\" font-size=\"12.5\" fill=\"" << TEXT_2 << "\">" << subtitle << "</text>\n";
    for (int k = 0; k * ys <= yTop + 1e-9; k++) {
        const std::string y = fixed1(py(k * ys));
        o << "<line x1=\"" << LEFT << "\" x2=\"" << LEFT + pw << "\" y1=\"" << y << "\" y2=\"" << y << "\" stroke=\"" << GRID
          << "\" stroke-width=\"1\"/>\n"
          << "<text x=\"" << LEFT - 8 << "\" y=\"" << fixed1(py(k * ys) + 4) << "\" font-size=\"12\" fill=\"" << TEXT_2
          << "\" text-anchor=\"end\">" << number(k * ys) << "</text>\n";
    }
    for (int k = 0; k * xs <= xTop + 1e-9; k++) {
        const std::string x = fixed1(px(k * xs));
        o << "<line x1=\"" << x << "\" x2=\"" << x << "\" y1=\"" << TOP + ph << "\" y2=\"" << TOP + ph + 5 << "\" stroke=\"" << AXIS
          << "\"/>\n"
          << "<text x=\"" << x << "\" y=\"" << TOP + ph + 20 << "\" font-size=\"12\" fill=\"" << TEXT_2
          << "\" text-anchor=\"middle\">" << number(k * xs) << "</text>\n";
    }
    o << "<line x1=\"" << LEFT << "\" x2=\"" << LEFT + pw << "\" y1=\"" << TOP + ph << "\" y2=\"" << TOP + ph << "\" stroke=\"" << AXIS
      << "\" stroke-width=\"1\"/>\n"
      << "<text x=\"" << LEFT + pw / 2 << "\" y=\"" << H - 16 << "\" font-size=\"13\" fill=\"" << TEXT << "\" text-anchor=\"middle\">"
      << xLabel << "</text>\n"
      << "<text transform=\"translate(20 " << TOP + ph / 2 << ") rotate(-90)\" font-size=\"13\" fill=\"" << TEXT
      << "\" text-anchor=\"middle\">" << yLabel << "</text>\n";

    struct End {
        double y;
        std::string name;
        double x;
    };
    std::vector<End> ends;
    for (size_t i = 0; i < series.size(); i++) {
        const Series& s = series[i];
        std::string d;
        for (size_t j = 0; j < s.points.size(); j++)
            d += (j ? " L" : "M") + fixed1(px(s.points[j].first)) + "," + fixed1(py(s.points[j].second));
        o << "<path d=\"" << d << "\" fill=\"none\" stroke=\"" << COLORS[i] << "\" stroke-width=\"2\" stroke-linejoin=\"round\"/>\n";
        for (const auto& [x, y] : s.points) o << marker(static_cast<int>(i), px(x), py(y), COLORS[i]) << "\n";
        if (!s.points.empty()) ends.push_back({py(s.points.back().second), s.name, px(s.points.back().first)});
    }
    // names at the line ends, nudged apart so they never overlap (one series needs none: the title names it)
    if (series.size() > 1) {
        std::sort(ends.begin(), ends.end(), [](const End& a, const End& b) { return a.y < b.y; });
        for (size_t j = 1; j < ends.size(); j++) ends[j].y = std::max(ends[j].y, ends[j - 1].y + 17);
        for (const End& e : ends)
            o << "<text x=\"" << fixed1(e.x + 12) << "\" y=\"" << fixed1(e.y + 4) << "\" font-size=\"12.5\" fill=\"" << TEXT << "\">"
              << e.name << "</text>\n";
    }
    o << "</svg>\n";
    std::ofstream(path) << o.str();
}

// Reads the benchmark CSV ("kind,v1,v2,...", "#" lines are comments) and draws
// bench-graph, bench-hash-find, bench-hash-insert, bench-ranking and bench-analyze.svg.
inline bool drawCharts(const std::string& csvPath, const std::string& outDir) {
    std::ifstream in(csvPath);
    if (!in) return false;
    std::vector<std::vector<double>> graph, hash, ranking, analyze;
    for (std::string line; std::getline(in, line);) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream cells(line);
        std::string kind, cell;
        std::getline(cells, kind, ',');
        std::vector<double> v;
        while (std::getline(cells, cell, ',')) v.push_back(std::stod(cell));
        if (kind == "graph" && v.size() == 5) graph.push_back(v);
        else if (kind == "hashmap" && v.size() == 5) hash.push_back(v);
        else if (kind == "ranking" && v.size() == 4) ranking.push_back(v);
        else if (kind == "analyze" && v.size() == 3) analyze.push_back(v);
    }
    auto series = [](const std::string& name, const std::vector<std::vector<double>>& rows, auto x, auto y) {
        Series s{name, {}};
        for (const auto& r : rows)
            if (y(r) >= 0) s.points.push_back({x(r), y(r)});  // -1 = not measured
        return s;
    };
    auto vePlusE = [](const std::vector<double>& r) { return (r[0] + r[1]) / 1e6; };
    auto millions = [](const std::vector<double>& r) { return r[0] / 1e6; };
    auto thousands = [](const std::vector<double>& r) { return r[0] / 1e3; };
    auto col = [](size_t i) { return [i](const std::vector<double>& r) { return r[i]; }; };
    const std::string dir = outDir + "/";

    lineChart(dir + "bench-graph.svg", "Graph algorithms stay close to linear as projects grow",
              "Time against files + import links (V + E), generated projects, median of 5 runs",
              "Files + import links (millions)", "Time (ms)",
              {series("Kahn (build order)", graph, vePlusE, col(3)), series("Tarjan (cycles)", graph, vePlusE, col(2)),
               series("BFS (impact)", graph, vePlusE, col(4))});
    lineChart(dir + "bench-hash-find.svg", "Looking up n keys and n missing keys",
              "Our chained HashMap against std::unordered_map, median of 5 runs", "Keys in the table (millions)", "Time (ms)",
              {series("Our HashMap", hash, millions, col(3)), series("std::unordered_map", hash, millions, col(4))});
    lineChart(dir + "bench-hash-insert.svg", "Inserting n keys",
              "Our chained HashMap against std::unordered_map, median of 5 runs", "Keys inserted (millions)", "Time (ms)",
              {series("Our HashMap", hash, millions, col(1)), series("std::unordered_map", hash, millions, col(2))});
    lineChart(dir + "bench-ranking.svg", "Critical-files ranking: bitsets against one BFS per file",
              "Counting what every file needs and is needed by, median of 3 runs (BFS stopped at 20,000 files)",
              "Files in the project (thousands)", "Time (ms)",
              {series("Bitsets", ranking, thousands, col(3)), series("One BFS per file", ranking, thousands, col(2))});
    lineChart(dir + "bench-analyze.svg", "Whole analysis of generated Python projects",
              "Reading imports, resolving them and every algorithm, median of 3 runs", "Files in the project (thousands)",
              "Time (ms)", {series("Whole analysis", analyze, thousands, col(2))});
    return true;
}

}  // namespace bench

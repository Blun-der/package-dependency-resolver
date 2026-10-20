// Package Dependency Resolver - tests
// Build and run with:  make test     (or: ./build/pdr_tests ../sample/fad_demo)
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../src/algorithms.h"
#include "../src/analyzer.h"
#include "../src/dot_export.h"
#include "../src/hash_map.h"
#include "../src/min_heap.h"
#include "../src/parser.h"

using namespace pdr;
using Strings = std::vector<std::string>;

namespace {

int failures = 0;
std::string sampleDir = "../sample/fad_demo";

std::string show(const std::string& s) { return "\"" + s + "\""; }
std::string show(int v) { return std::to_string(v); }
std::string show(size_t v) { return std::to_string(v); }
template <typename T>
std::string show(const std::vector<T>& v) {
    std::string out = "[";
    for (size_t i = 0; i < v.size(); i++) out += (i ? ", " : "") + show(v[i]);
    return out + "]";
}

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            failures++;                                                             \
            std::cerr << "    line " << __LINE__ << ": expected " #cond "\n";       \
        }                                                                           \
    } while (0)

#define CHECK_EQ(actual, expected)                                                                \
    do {                                                                                          \
        const auto a_ = (actual);                                                                 \
        const auto e_ = (expected);                                                               \
        if (!(a_ == e_)) {                                                                        \
            failures++;                                                                           \
            std::cerr << "    line " << __LINE__ << ": " #actual "\n      got      " << show(a_) \
                      << "\n      expected " << show(e_) << "\n";                                 \
        }                                                                                         \
    } while (0)

Strings sorted(Strings v) {
    std::sort(v.begin(), v.end());
    return v;
}

std::string lines(std::initializer_list<const char*> parts) {
    std::string out;
    for (const char* p : parts) out += (out.empty() ? "" : "\n") + std::string(p);
    return out;
}

Graph graphOf(const std::vector<std::pair<std::string, std::string>>& edges) {
    Graph g;
    for (const auto& [a, b] : edges) g.addEdge(a, b, {1, ""});
    return g;
}

// ---------- parsing ----------

void pythonImportForms() {
    const auto imps = parsePython(lines({
        "import os, sys as system",
        "from a.b import (c,",
        "    d)",
        "from . import x",
        "from ..pkg import y as z",
        "# import commented",
        "\"\"\"import inside docstring\"\"\"",
        "def f():",
        "    import lazy",
    }));
    Strings specs;
    std::vector<int> levels, lineNos;
    for (const Import& i : imps) {
        specs.push_back(i.spec);
        levels.push_back(i.level);
        lineNos.push_back(i.line);
    }
    CHECK_EQ(specs, (Strings{"os", "sys", "a.b", "", "pkg", "lazy"}));
    CHECK_EQ(levels, (std::vector<int>{0, 0, 0, 1, 2, 0}));
    CHECK_EQ(lineNos, (std::vector<int>{1, 1, 2, 4, 5, 9}));
    if (imps.size() == 6) {
        CHECK_EQ(imps[2].names, (Strings{"c", "d"}));
        CHECK_EQ(imps[4].names, (Strings{"y"}));
    }
}

Strings specsOf(const std::vector<Import>& imps) {
    Strings out;
    for (const Import& i : imps) out.push_back(i.spec);
    return out;
}

std::vector<When> timingOf(const std::vector<Import>& imps) {
    std::vector<When> out;
    for (const Import& i : imps) out.push_back(i.when);
    return out;
}

void pythonImportTiming() {
    const auto imps = parsePython(lines({
        "import a",
        "from typing import TYPE_CHECKING",
        "if TYPE_CHECKING:",
        "    from b import B",
        "else:",
        "    import c",
        "try:",
        "    import d",
        "except ImportError:",
        "    pass",
        "class K:",
        "    import e",
        "    def method(self,",
        "               x=(1,",
        "  2)):",
        "        import f",
        "    import g",
        "async def run():",
        "    if True:",
        "        import h",
        "if typing.TYPE_CHECKING:",
        "    import i",
        "if __name__ == '__main__':",
        "    import j",
        "import k",
    }));
    CHECK_EQ(specsOf(imps), (Strings{"a", "typing", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k"}));
    const When L = When::Load, D = When::Deferred, T = When::TypeChecking;
    CHECK(timingOf(imps) == (std::vector<When>{L, L, T, L, L, L, D, L, D, T, D, L}));
}

void jsImportTiming() {
    const auto imps = parseJs(lines({
        "import a from './a';",
        "import type { B } from './b';",
        "export type { C } from './c';",
        "import type from './d';",  // a default import that happens to be called "type"
        "const e = () => import('./e');",
    }));
    CHECK_EQ(specsOf(imps), (Strings{"./a", "./b", "./c", "./d", "./e"}));
    const When L = When::Load, D = When::Deferred, T = When::TypeChecking;
    CHECK(timingOf(imps) == (std::vector<When>{L, T, T, L, D}));
}

void jsImportForms() {
    const auto imps = parseJs(lines({
        "import React, { useState } from 'react';",
        "import './styles.css';",
        "export { a } from './a';",
        "const fs = require('fs');",
        "// import x from './commented';",
        "const lazy = () => import('./lazy');",
        "import type { T } from './types';",
    }));
    CHECK_EQ(specsOf(imps), (Strings{"react", "./styles.css", "./a", "fs", "./lazy", "./types"}));
}

void cppIncludes() {
    const auto imps = parseCpp(lines({
        "#include <vector>",
        "  #  include \"graph.h\"  // the graph",
        "// #include \"commented.h\"",
        "/* #include \"block.h\" */",
        "const char* s = \"#include \\\"in_string.h\\\"\";",
        "#include_next <stdio.h>",
        "#include <sys/stat.h>",
    }));
    CHECK_EQ(specsOf(imps), (Strings{"vector", "graph.h", "sys/stat.h"}));
    if (imps.size() == 3) {
        CHECK(imps[0].kind == ImportKind::SystemInclude);
        CHECK(imps[1].kind == ImportKind::Include);
        CHECK_EQ(imps[1].line, 2);
        CHECK_EQ(imps[2].line, 7);
    }
}

void jsStringLiteralsIgnored() {
    const auto imps = parseJs(lines({
        "const example = \"import x from './not-real'\";",
        "const tpl = `require('also-not-real')`;",
        "import real from './real';",
    }));
    CHECK_EQ(specsOf(imps), (Strings{"./real"}));
}

// ---------- data structures ----------

void minHeapOrder() {
    MinHeap<int> h;
    for (int x : {5, 1, 4, 2, 3, 0}) h.push(x);
    std::vector<int> out;
    while (!h.empty()) out.push_back(h.pop());
    CHECK_EQ(out, (std::vector<int>{0, 1, 2, 3, 4, 5}));
}

void triePrefixSearch() {
    Trie t;
    t.insert("rules.py", "fad/rules.py");
    t.insert("rename.py", "fad/actions/rename.py");
    t.insert("daemon.py", "fad/daemon.py");
    CHECK_EQ(sorted(t.search("r")), (Strings{"fad/actions/rename.py", "fad/rules.py"}));
    CHECK_EQ(t.search("ru"), (Strings{"fad/rules.py"}));
    CHECK_EQ(t.search("zzz"), Strings{});
}

void hashMapBasics() {
    HashMap<std::string, int> m;
    CHECK(m.insert("a", 1));
    CHECK(!m.insert("a", 2));  // already there: keeps the old value
    CHECK_EQ(*m.find("a"), 1);
    m.set("a", 3);
    CHECK_EQ(*m.find("a"), 3);
    CHECK(m.find("b") == nullptr);

    // grows from 16 buckets and still finds everything
    for (int i = 0; i < 5000; i++) m.insert("file" + std::to_string(i), i);
    CHECK_EQ(m.size(), size_t{5001});
    bool allFound = true;
    for (int i = 0; i < 5000; i++) {
        const int* v = m.find("file" + std::to_string(i));
        allFound = allFound && v && *v == i;
    }
    CHECK(allFound);
    CHECK(m.size() <= m.bucketCount() * 3 / 4);  // load factor stays under 0.75
    CHECK(m.longestChain() <= 8);                // a good hash keeps chains short

    CHECK(m.erase("file42"));
    CHECK(!m.erase("file42"));
    CHECK(!m.contains("file42"));
    CHECK_EQ(m.size(), size_t{5000});

    HashMap<std::string, int> copy = m;  // deep copy: changing it leaves m alone
    copy.set("a", 99);
    CHECK_EQ(*m.find("a"), 3);
    CHECK_EQ(*copy.find("a"), 99);
}

// Every key in one bucket: the chain (linked list) must still work.
struct AlwaysZero {
    size_t operator()(const std::string&) const { return 0; }
};

void hashMapCollisions() {
    HashMap<std::string, int, AlwaysZero> m;
    for (int i = 0; i < 100; i++) m.insert("k" + std::to_string(i), i);
    CHECK_EQ(m.longestChain(), size_t{100});
    CHECK_EQ(*m.find("k0"), 0);
    CHECK_EQ(*m.find("k99"), 99);
    CHECK(m.erase("k50"));  // unlink from the middle of the chain
    CHECK(!m.contains("k50"));
    CHECK_EQ(*m.find("k51"), 51);
    CHECK_EQ(m.size(), size_t{99});
}

// ---------- graph algorithms ----------

void tarjanCycles() {
    const Graph g = graphOf({{"a", "b"}, {"b", "c"}, {"c", "a"}, {"c", "d"}, {"e", "d"}});
    Strings sccs;
    for (const Component& c : tarjanSCC(g)) {
        std::string joined;
        for (const std::string& n : g.names(c)) joined += (joined.empty() ? "" : ",") + n;
        sccs.push_back(joined);
    }
    CHECK_EQ(sorted(sccs), (Strings{"a,b,c", "d", "e"}));
}

void buildOrderCollapsesCycles() {
    const Graph g = graphOf({{"app", "web"}, {"app", "db"}, {"web", "json"}, {"db", "json"},
                             {"x", "y"}, {"y", "x"}, {"app", "x"}});
    const BuildOrder bo = buildOrder(g, tarjanSCC(g));
    Strings flat;
    for (const BuildUnit& u : bo.order) {
        std::string joined;
        for (const std::string& n : g.names(u.files)) joined += (joined.empty() ? "" : "+") + n;
        flat.push_back(joined);
        if (u.cyclic) CHECK_EQ(g.names(u.files), (Strings{"x", "y"}));
    }
    CHECK_EQ(flat, (Strings{"json", "db", "web", "x+y", "app"}));
    CHECK_EQ(bo.layerOf[g.id("json")], 0);
    CHECK_EQ(bo.layerOf[g.id("app")], 2);
}

void buildOrderLongChain() {
    std::vector<std::pair<std::string, std::string>> edges;
    for (int i = 0; i < 5000; i++) edges.push_back({"n" + std::to_string(i), "n" + std::to_string(i + 1)});
    const Graph g = graphOf(edges);
    const auto sccs = tarjanSCC(g);
    CHECK_EQ(sccs.size(), size_t{5001});
    const BuildOrder bo = buildOrder(g, sccs);
    CHECK_EQ(bo.order.size(), size_t{5001});
    if (bo.order.size() == 5001) {
        CHECK_EQ(g.name(bo.order[0].files[0]), std::string("n5000"));
        CHECK_EQ(g.name(bo.order[5000].files[0]), std::string("n0"));
    }
}

void shortestPathAndReachability() {
    const Graph g = graphOf({{"a", "b"}, {"b", "c"}, {"a", "c"}, {"c", "d"}});
    CHECK_EQ(g.names(shortestPath(g, g.id("a"), g.id("d"))), (Strings{"a", "c", "d"}));
    CHECK(shortestPath(g, g.id("d"), g.id("a")).empty());
    CHECK_EQ(sorted(g.names(reachable(g, g.id("d"), true))), (Strings{"a", "b", "c"}));
}

// ---------- resolution ----------

void pythonResolution() {
    const Result r = analyze({
        {"proj/src/app/__init__.py", ""},
        {"proj/src/app/main.py", "from app.core import engine\nimport requests\nfrom .missing import x"},
        {"proj/src/app/core/__init__.py", "from . import engine"},
        {"proj/src/app/core/engine.py", "from ..util import helper\nimport os.path"},
        {"proj/src/app/util.py", ""},
        {"proj/scripts/run.py", "import helpers\nfrom app.main import *"},
        {"proj/scripts/helpers.py", ""},
    }, {true});
    CHECK_EQ(r.rootName, std::string("proj"));
    CHECK_EQ(r.graph.outNames("src/app/main.py"), (Strings{"src/app/core/engine.py"}));
    CHECK_EQ(r.graph.outNames("src/app/core/__init__.py"), (Strings{"src/app/core/engine.py"}));
    CHECK_EQ(r.graph.outNames("src/app/core/engine.py"), (Strings{"src/app/util.py"}));
    CHECK_EQ(r.graph.outNames("scripts/run.py"), (Strings{"scripts/helpers.py", "src/app/main.py"}));
    Strings ext;
    for (const ExternalPackage& e : r.externals) ext.push_back(e.name);
    CHECK_EQ(sorted(ext), (Strings{"os", "requests"}));
    CHECK_EQ(r.broken.size(), size_t{1});
    if (!r.broken.empty()) {
        CHECK_EQ(r.broken[0].spec, std::string(".missing"));
        CHECK_EQ(r.broken[0].line, 3);
    }
}

void loadTimeCycles() {
    const Result r = analyze({
        {"pkg/__init__.py", ""},
        {"pkg/a.py", "from .b import B\nimport types"},
        {"pkg/b.py", "from typing import TYPE_CHECKING\nif TYPE_CHECKING:\n    from .a import A\n"
                     "def f():\n    from .c import C"},
        {"pkg/c.py", "from .b import B"},
        {"pkg/types.py", ""},
    });
    // every import is still a dependency ...
    CHECK_EQ(r.graph.outNames("pkg/b.py"), (Strings{"pkg/a.py", "pkg/c.py"}));
    // ... but only load-time ones can make a circular import fail
    CHECK_EQ(r.loadGraph.outNames("pkg/b.py"), Strings{});
    CHECK_EQ(r.cycles.size(), size_t{0});
    CHECK_EQ(r.stats.deferredImports, 1);
    CHECK_EQ(r.stats.typeCheckingImports, 1);
    // inside a package, "import types" is the standard library, not pkg/types.py
    CHECK_EQ(r.graph.outNames("pkg/a.py"), (Strings{"pkg/b.py"}));
}

void jsResolution() {
    const Result r = analyze({
        {"src/index.ts", "import App from './App'\nimport { api } from '@/lib/api'\n"
                         "import x from 'lodash/get'\nimport y from '@scope/pkg/sub'"},
        {"src/App.tsx", "import './components'\nimport z from './nope'"},
        {"src/components/index.js", ""},
        {"src/lib/api.js", "const fs = require('node:fs')"},
    });
    CHECK_EQ(r.graph.outNames("src/index.ts"), (Strings{"src/App.tsx", "src/lib/api.js"}));
    CHECK_EQ(r.graph.outNames("src/App.tsx"), (Strings{"src/components/index.js"}));
    Strings ext;
    for (const ExternalPackage& e : r.externals) ext.push_back(e.name);
    CHECK_EQ(sorted(ext), (Strings{"@scope/pkg", "fs", "lodash"}));
    CHECK(!r.broken.empty() && r.broken[0].spec == "./nope");
}

void cppResolution() {
    const Result r = analyze({
        {"src/main.cpp", "#include \"graph.h\"\n#include \"util/str.h\"\n#include <vector>\n"
                         "#include <boost/asio.hpp>\n#include \"missing.h\"\n#include \"../include/api.h\"\n"
                         "#include \"gtest/gtest.h\""},
        {"src/graph.h", "#include <api.h>\n#include <string.h>"},
        {"src/util/str.h", ""},
        {"include/api.h", ""},
        {"tests/t.cpp", "#include \"graph.h\""},
        {"lib/a/config.h", ""},
        {"lib/b/config.h", ""},
        {"lib/a/sub/y.c", "#include \"config.h\""},
    });
    CHECK_EQ(r.graph.outNames("src/main.cpp"), (Strings{"include/api.h", "src/graph.h", "src/util/str.h"}));
    CHECK_EQ(r.graph.outNames("src/graph.h"), (Strings{"include/api.h"}));  // <api.h> found in the project
    CHECK_EQ(r.graph.outNames("tests/t.cpp"), (Strings{"src/graph.h"}));    // by the end of the path
    CHECK_EQ(r.graph.outNames("lib/a/sub/y.c"), (Strings{"lib/a/config.h"}));  // the closer of two
    Strings outside, install;
    for (const ExternalPackage& e : r.externals) {
        outside.push_back(e.name);
        if (!e.builtin) install.push_back(e.name);
    }
    CHECK_EQ(sorted(outside), (Strings{"boost", "gtest", "string.h", "vector"}));
    CHECK_EQ(sorted(install), (Strings{"boost", "gtest"}));
    CHECK_EQ(r.broken.size(), size_t{1});
    if (!r.broken.empty()) CHECK_EQ(r.broken[0].spec, std::string("missing.h"));
    CHECK_EQ(r.stats.cppFiles, 8);
}

Strings planOf(const Graph& g) {
    Strings out;
    for (const Component& c : tarjanSCC(g)) {
        if (c.size() < 2) continue;
        const auto plan = cycleBreakingPlan(g, c);  // keep it alive while looping over it
        for (const Link& l : *plan) out.push_back(g.name(l.first) + ">" + g.name(l.second));
    }
    return out;
}

void cycleFixPlans() {
    // b -> c is in both loops (a>b>c>a and b>c>b): removing it alone fixes everything
    CHECK_EQ(planOf(graphOf({{"a", "b"}, {"b", "c"}, {"c", "a"}, {"c", "b"}})), (Strings{"b>c"}));
    // two loops through "hub" that share no link need two removals
    const Strings two = planOf(graphOf({{"hub", "x"}, {"x", "hub"}, {"hub", "y"}, {"y", "hub"}}));
    CHECK_EQ(two.size(), size_t{2});
    // a tie broken by stability, not by name: "zcore" is imported by many files
    // (stable) and imports "plugin" (unstable), so zcore -> plugin is the import
    // to cut, although plugin -> zcore comes first alphabetically
    CHECK_EQ(planOf(graphOf({{"zcore", "plugin"}, {"plugin", "zcore"},
                             {"u1", "zcore"}, {"u2", "zcore"}, {"u3", "zcore"}, {"plugin", "z"}})),
             (Strings{"zcore>plugin"}));
    // the limit on links
    const Graph g = graphOf({{"a", "b"}, {"b", "a"}});
    CHECK(!cycleBreakingPlan(g, tarjanSCC(g)[0], 1).has_value());
}

void dotExport() {
    const Result r = analyze(loadProject(sampleDir));
    std::ostringstream out;
    writeDot(out, r);
    const std::string dot = out.str();
    CHECK(startsWith(dot, "digraph dependencies {"));
    CHECK(dot.find("label=\"fad/actions/\"") != std::string::npos);
    CHECK(dot.find("\"fad/rules.py\" -> \"fad/actions/__init__.py\"") != std::string::npos);
    CHECK(dot.find("label=\"remove\"") != std::string::npos);
    CHECK(dot.find("label=\".compress\\nnot found\"") != std::string::npos);
    CHECK(endsWith(dot, "}\n"));
}

void ignoredFoldersSkipped() {
    const Result r = analyze({
        {"a.py", "import b"}, {"b.py", ""},
        {"venv/lib/x.py", ""}, {"node_modules/m/i.js", ""}, {"README.md", "# hi"},
    });
    CHECK_EQ(sorted(r.graph.names(r.graph.nodesByName())), (Strings{"a.py", "b.py"}));
}

// ---------- end to end on the bundled sample ----------

void sampleProject() {
    const Result r = analyze(loadProject(sampleDir));
    CHECK_EQ(r.stats.files, 17);

    // rules -> actions/__init__ -> move -> rules
    CHECK_EQ(r.cycles.size(), size_t{1});
    if (!r.cycles.empty()) {
        const Cycle& c = r.cycles[0];
        CHECK_EQ(c.files, (Strings{"fad/actions/__init__.py", "fad/actions/move.py", "fad/rules.py"}));
        CHECK(!c.loop.empty() && c.loop.front() == c.loop.back());
        CHECK(std::all_of(c.steps.begin(), c.steps.end(), [](const Step& s) { return s.line > 0; }));
    }

    // archive.py imports a module that does not exist
    CHECK_EQ(r.broken.size(), size_t{1});
    if (!r.broken.empty()) {
        CHECK_EQ(r.broken[0].file, std::string("fad/actions/archive.py"));
        CHECK_EQ(r.broken[0].spec, std::string(".compress"));
    }

    // every unit appears after all of its dependencies
    std::vector<int> pos(r.graph.size(), -1);
    for (size_t i = 0; i < r.order.size(); i++)
        for (const std::string& f : r.order[i].files) pos[r.graph.id(f)] = static_cast<int>(i);
    for (int a = 0; a < r.graph.size(); a++)
        for (const Edge& e : r.graph.out(a)) CHECK(pos[e.to] <= pos[a]);

    CHECK(!r.critical.empty() && r.critical[0] == "fad/config.py");
    const auto why = explainPath(r, "fad/main.py", "fad/utils/paths.py");
    CHECK(why.has_value());
    if (why) {
        CHECK_EQ(why->path.front(), std::string("fad/main.py"));
        CHECK_EQ(why->path.back(), std::string("fad/utils/paths.py"));
    }
    Strings thirdParty;
    bool osBuiltin = false;
    for (const ExternalPackage& e : r.externals) {
        if (!e.builtin) thirdParty.push_back(e.name);
        if (e.name == "os") osBuiltin = e.builtin;
    }
    CHECK_EQ(sorted(thirdParty), (Strings{"pytest", "requests", "watchdog", "yaml"}));
    CHECK(osBuiltin);
    CHECK(std::find(r.entryPoints.begin(), r.entryPoints.end(), "fad/main.py") != r.entryPoints.end());

    // the core file importing the plugin package is the import to cut
    if (!r.cycles.empty() && r.cycles[0].fix.size() == 1) {
        CHECK_EQ(r.cycles[0].fix[0].from, std::string("fad/rules.py"));
        CHECK_EQ(r.cycles[0].fix[0].line, 4);
    } else {
        CHECK(false);
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) sampleDir = argv[1];
    const std::vector<std::pair<const char*, void (*)()>> tests = {
        {"python: import forms, aliases and line numbers", pythonImportForms},
        {"js: import, export-from, require, dynamic import, comments ignored", jsImportForms},
        {"js: import text inside string literals is ignored", jsStringLiteralsIgnored},
        {"python: when imports run (function, TYPE_CHECKING, __main__, class, try)", pythonImportTiming},
        {"js: type-only and dynamic imports", jsImportTiming},
        {"c/c++: #include forms, comments and strings ignored", cppIncludes},
        {"min-heap returns items in order", minHeapOrder},
        {"trie prefix search", triePrefixSearch},
        {"hash map: insert, find, grow, erase, copy", hashMapBasics},
        {"hash map: every key in one bucket (chaining)", hashMapCollisions},
        {"tarjan finds cycles and singletons", tarjanCycles},
        {"build order puts dependencies first and collapses cycles", buildOrderCollapsesCycles},
        {"build order is valid on a long chain (no recursion limit)", buildOrderLongChain},
        {"shortest path and reachability", shortestPathAndReachability},
        {"cycle fix: shared link, two loops, stability tie-break, limit", cycleFixPlans},
        {"python resolution: packages, relative, src layout, externals, broken", pythonResolution},
        {"cycles use load-time imports only; package-local names do not shadow the stdlib", loadTimeCycles},
        {"js resolution: extensions, index files, aliases, packages", jsResolution},
        {"c/c++ resolution: same folder, path ending, closest, system, broken", cppResolution},
        {"ignored folders and non-code files are skipped", ignoredFoldersSkipped},
        {"sample daemon project: cycle, order, critical files, path, fix", sampleProject},
        {"graphviz export of the sample", dotExport},
    };
    int failed = 0;
    for (const auto& [name, fn] : tests) {
        const int before = failures;
        try {
            fn();
        } catch (const std::exception& e) {
            failures++;
            std::cerr << "    exception: " << e.what() << "\n";
        }
        const bool ok = failures == before;
        failed += ok ? 0 : 1;
        std::cout << (ok ? "PASS  " : "FAIL  ") << name << "\n";
    }
    std::cout << "\n" << tests.size() - failed << " of " << tests.size() << " tests passed\n";
    return failed ? 1 : 0;
}

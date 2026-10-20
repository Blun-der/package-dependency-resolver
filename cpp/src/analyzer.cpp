#include "analyzer.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

#include "algorithms.h"
#include "parser.h"
#include "resolver.h"

namespace pdr {

namespace {

std::vector<Step> stepsAlong(const Graph& graph, const std::vector<int>& path) {
    std::vector<Step> steps;
    for (size_t i = 0; i + 1 < path.size(); i++) {
        const Evidence* ev = graph.edge(path[i], path[i + 1]);
        steps.push_back({graph.name(path[i]), graph.name(path[i + 1]), ev->line, ev->text});
    }
    return steps;
}

int countLines(const std::string& content) {
    if (content.empty()) return 0;
    return static_cast<int>(std::count(content.begin(), content.end(), '\n')) + 1;
}

}  // namespace

std::vector<SourceFile> loadProject(const std::string& folder) {
    namespace fs = std::filesystem;
    const fs::path root(folder);
    if (!fs::is_directory(root)) throw std::runtime_error("not a folder: " + folder);

    std::vector<SourceFile> files;
    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    if (ec) throw std::runtime_error("cannot read " + folder + ": " + ec.message());
    for (const fs::recursive_directory_iterator end; it != end; it.increment(ec)) {
        if (ec) throw std::runtime_error("cannot read " + folder + ": " + ec.message());
        const std::string rel = it->path().lexically_relative(root).generic_string();
        if (it->is_directory(ec)) {
            if (isIgnored(rel)) it.disable_recursion_pending();  // never walk into node_modules etc.
            continue;
        }
        if (!it->is_regular_file(ec) || langOf(rel) == Lang::None || isIgnored(rel)) continue;
        std::ifstream in(it->path(), std::ios::binary);
        std::ostringstream content;
        content << in.rdbuf();
        files.push_back({rel, content.str()});
    }
    std::sort(files.begin(), files.end(),
              [](const SourceFile& a, const SourceFile& b) { return a.path < b.path; });
    return files;
}

Result analyze(std::vector<SourceFile> files, const AnalyzeOptions& options) {
    const auto t0 = std::chrono::steady_clock::now();
    Result r;

    std::vector<SourceFile> clean;
    for (SourceFile& f : files) {
        std::string p = f.path;
        std::replace(p.begin(), p.end(), '\\', '/');
        if (startsWith(p, "./")) p = p.substr(2);
        if (langOf(p) != Lang::None && !isIgnored(p)) clean.push_back({p, std::move(f.content)});
    }

    if (options.stripRoot && !clean.empty()) {
        const std::string first = clean[0].path.substr(0, clean[0].path.find('/'));
        const bool shared = std::all_of(clean.begin(), clean.end(), [&](const SourceFile& f) {
            return startsWith(f.path, first + "/");
        });
        if (shared) {
            r.rootName = first;
            for (SourceFile& f : clean) f.path = f.path.substr(first.size() + 1);
        }
    }

    std::vector<std::string> paths;
    for (const SourceFile& f : clean) paths.push_back(f.path);
    std::sort(paths.begin(), paths.end());
    const ResolveContext ctx = makeContext(paths);

    Graph& graph = r.graph;
    Graph& loadGraph = r.loadGraph;
    for (const SourceFile& f : clean) {
        graph.addNode(f.path);
        loadGraph.addNode(f.path);
    }

    struct ExternalUse {
        std::set<std::string> files;
        bool builtin;
        Lang lang;
    };
    std::map<std::string, ExternalUse> externals;  // package -> files using it

    for (const SourceFile& f : clean) {
        const Lang lang = langOf(f.path);
        const std::vector<Import> imports = parseImports(f.path, f.content);
        r.stats.importStatements += static_cast<int>(imports.size());
        for (const Import& imp : imports) {
            if (imp.when == When::Deferred) r.stats.deferredImports++;
            if (imp.when == When::TypeChecking) r.stats.typeCheckingImports++;
        }
        FileInfo info;
        info.path = f.path;
        info.lang = lang;
        info.lines = countLines(f.content);

        for (const Import& imp : imports) {
            const std::vector<Resolution> res = lang == Lang::Python ? resolvePython(imp, f.path, ctx)
                                                : lang == Lang::Cpp  ? resolveCpp(imp, f.path, ctx)
                                                                     : resolveJs(imp, f.path, ctx);
            for (const Resolution& x : res) {
                switch (x.kind) {
                    case Resolution::Kind::File:
                        if (x.value == f.path) break;  // package importing itself
                        graph.addEdge(f.path, x.value, {imp.line, imp.text});
                        if (imp.when == When::Load) loadGraph.addEdge(f.path, x.value, {imp.line, imp.text});
                        info.imports.push_back({ImportRef::Type::File, x.value, imp.line, imp.text});
                        break;
                    case Resolution::Kind::External: {
                        auto it = externals.find(x.value);
                        if (it == externals.end())
                            it = externals.emplace(x.value, ExternalUse{{}, isBuiltin(lang, x.value), lang}).first;
                        it->second.files.insert(f.path);
                        info.externals.insert(x.value);
                        info.imports.push_back({ImportRef::Type::External, x.value, imp.line, imp.text});
                        break;
                    }
                    case Resolution::Kind::Broken:
                        r.broken.push_back({f.path, x.value, imp.line, imp.text});
                        info.imports.push_back({ImportRef::Type::Broken, x.value, imp.line, imp.text});
                        break;
                }
            }
        }
        r.fileInfo.push_back(std::move(info));
        (lang == Lang::Python ? r.stats.pythonFiles : lang == Lang::Cpp ? r.stats.cppFiles : r.stats.jsFiles)++;
    }

    // Circular imports: only an import that runs while its module is loading
    // can find the other module half-loaded, so deferred and type-checking
    // imports are left out.
    const std::vector<Component> sccs = tarjanSCC(loadGraph);
    for (const Component& comp : sccs) {
        if (comp.size() < 2) continue;
        const std::vector<int> loop = cycleIn(loadGraph, comp);
        Cycle cycle{graph.names(comp), graph.names(loop), stepsAlong(loadGraph, loop), {}, false};
        if (auto plan = cycleBreakingPlan(loadGraph, comp)) {
            for (const Link& link : *plan) {
                const Evidence* ev = loadGraph.edge(link.first, link.second);
                cycle.fix.push_back({graph.name(link.first), graph.name(link.second), ev->line, ev->text});
            }
        } else {
            cycle.fixTooLarge = true;
        }
        r.cycles.push_back(std::move(cycle));
    }
    std::sort(r.cycles.begin(), r.cycles.end(), [](const Cycle& a, const Cycle& b) {
        if (a.files.size() != b.files.size()) return a.files.size() > b.files.size();
        return a.files[0] < b.files[0];
    });

    // Build order and layers (load order: a deferred import does not have to load first)
    const BuildOrder bo = buildOrder(loadGraph, sccs);
    int maxLayer = 0;
    for (const BuildUnit& u : bo.order) {
        r.order.push_back({graph.names(u.files), u.cyclic, u.layer});
        maxLayer = std::max(maxLayer, u.layer);
    }

    // Per-file metrics: one BFS each way per file -> O(V * (V + E))
    for (int id = 0; id < graph.size(); id++) {
        Metrics m;
        m.fanOut = static_cast<int>(graph.out(id).size());
        m.fanIn = static_cast<int>(graph.in(id).size());
        m.dependents = static_cast<int>(reachable(graph, id, true).size());
        m.dependencies = static_cast<int>(reachable(graph, id, false).size());
        m.layer = bo.layerOf[id];
        m.inCycle = sccs[bo.compOf[id]].size() > 1;
        r.metrics.push_back(m);
    }

    std::vector<int> byName = graph.nodesByName();
    for (int id : byName) {
        if (r.metrics[id].dependents > 0) r.critical.push_back(graph.name(id));
        if (r.metrics[id].fanIn == 0) r.entryPoints.push_back(graph.name(id));
    }
    std::stable_sort(r.critical.begin(), r.critical.end(), [&](const std::string& a, const std::string& b) {
        const Metrics& ma = r.metricsOf(a);
        const Metrics& mb = r.metricsOf(b);
        if (ma.dependents != mb.dependents) return ma.dependents > mb.dependents;
        return ma.fanIn > mb.fanIn;
    });

    // File search: by file name and by full path
    for (int id = 0; id < graph.size(); id++) {
        r.trie.insert(baseName(graph.name(id)), graph.name(id));
        r.trie.insert(graph.name(id), graph.name(id));
    }

    // Outside packages: ones to install first, then the most used
    for (auto& [name, use] : externals)
        r.externals.push_back({name, {use.files.begin(), use.files.end()}, use.builtin, use.lang});
    std::stable_sort(r.externals.begin(), r.externals.end(), [](const ExternalPackage& a, const ExternalPackage& b) {
        if (a.builtin != b.builtin) return !a.builtin;
        return a.files.size() > b.files.size();
    });

    Stats& st = r.stats;
    st.files = graph.size();
    st.edges = graph.edgeCount();
    st.cycles = static_cast<int>(r.cycles.size());
    for (const Cycle& c : r.cycles) st.filesInCycles += static_cast<int>(c.files.size());
    st.externals = static_cast<int>(r.externals.size());
    for (const ExternalPackage& e : r.externals) st.thirdParty += e.builtin ? 0 : 1;
    st.broken = static_cast<int>(r.broken.size());
    st.layers = graph.size() ? maxLayer + 1 : 0;
    st.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

std::optional<PathExplanation> explainPath(const Result& result, const std::string& from, const std::string& to) {
    const std::vector<int> path = shortestPath(result.graph, result.graph.id(from), result.graph.id(to));
    if (path.empty()) return std::nullopt;
    return PathExplanation{result.graph.names(path), stepsAlong(result.graph, path)};
}

std::vector<std::string> impactOf(const Result& result, const std::string& file) {
    const int id = result.graph.id(file);
    if (id < 0) return {};
    std::vector<std::string> out = result.graph.names(reachable(result.graph, id, true));
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace pdr

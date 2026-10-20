#include "resolver.h"

#include <algorithm>
#include <optional>
#include <set>

#include "util.h"

namespace pdr {

namespace {

using Kind = Resolution::Kind;

std::string dotsToSlashes(std::string s) {
    std::replace(s.begin(), s.end(), '.', '/');
    return s;
}

// A "source root" is a folder Python would put on sys.path: the project root
// plus any folder with .py files that is NOT a package (no __init__.py).
HashMap<std::string, std::string> buildPythonIndex(const std::vector<std::string>& paths,
                                                   const HashSet<std::string>& fileSet) {
    std::vector<std::string> pyFiles;
    for (const std::string& p : paths)
        if (langOf(p) == Lang::Python) pyFiles.push_back(p);

    std::set<std::string> roots{""};
    for (const std::string& p : pyFiles) {
        const std::string d = dirOf(p);
        if (!d.empty() && !fileSet.contains(d + "/__init__.py")) roots.insert(d);
        // The folder that holds a top-level package is also a root ("src/" layout).
        if (endsWith(p, "__init__.py")) {
            const std::string parent = dirOf(d);
            if (!d.empty() && !fileSet.contains(joinPath(parent, "__init__.py"))) roots.insert(parent);
        }
    }
    // Shallow roots first, so "a.b" from the project root wins over a deeper match.
    std::vector<std::string> sorted(roots.begin(), roots.end());
    std::stable_sort(sorted.begin(), sorted.end(), [](const std::string& a, const std::string& b) {
        return split(a, '/').size() < split(b, '/').size();
    });

    HashMap<std::string, std::string> index;
    for (const std::string& r : sorted) {
        for (const std::string& p : pyFiles) {
            if (!r.empty() && !startsWith(p, r + "/")) continue;
            std::string rel = r.empty() ? p : p.substr(r.size() + 1);
            rel = rel.substr(0, rel.size() - 3);  // drop ".py"
            if (endsWith(rel, "/__init__")) rel = rel.substr(0, rel.size() - 9);
            else if (rel == "__init__") rel.clear();
            if (rel.empty()) continue;
            std::replace(rel.begin(), rel.end(), '/', '.');
            index.insert(rel, p);  // keeps the first (shallowest) match
        }
    }
    return index;
}

// dir + "a.b" -> "dir/a/b.py" or "dir/a/b/__init__.py", if it exists
std::optional<std::string> pyFileAt(const std::string& dir, const std::string& dotted,
                                    const ResolveContext& ctx) {
    const std::string base = joinPath(dir, dotsToSlashes(dotted));
    if (!base.empty() && ctx.fileSet.contains(base + ".py")) return base + ".py";
    const std::string init = joinPath(base, "__init__.py");
    if (ctx.fileSet.contains(init)) return init;
    return std::nullopt;
}

// Script folder first (Python puts the running script's folder on sys.path),
// then the project-wide index. A file inside a package (a folder with
// __init__.py) is not a script: there "import types" means the standard
// library module, never the package's own types.py.
std::optional<std::string> lookupPyModule(const std::string& dotted, const std::string& fromFile,
                                          const ResolveContext& ctx) {
    const std::string dir = dirOf(fromFile);
    const bool inPackage = ctx.fileSet.contains(dir.empty() ? "__init__.py" : dir + "/__init__.py");
    if (!inPackage) {
        if (auto hit = pyFileAt(dir, dotted, ctx)) return hit;
    }
    if (const std::string* file = ctx.pyIndex.find(dotted)) return *file;
    return std::nullopt;
}

}  // namespace

ResolveContext makeContext(const std::vector<std::string>& sortedPaths) {
    ResolveContext ctx;
    for (const std::string& p : sortedPaths) ctx.fileSet.insert(p);
    ctx.pyIndex = buildPythonIndex(sortedPaths, ctx.fileSet);
    for (const std::string& p : sortedPaths) {
        if (langOf(p) != Lang::Cpp) continue;
        // "a/b/c.h" is reachable as "a/b/c.h", "b/c.h" and "c.h"
        for (size_t start = 0;;) {
            const std::string suffix = p.substr(start);
            if (std::vector<std::string>* files = ctx.cSuffixes.find(suffix)) files->push_back(p);
            else ctx.cSuffixes.insert(suffix, {p});
            const size_t slash = p.find('/', start);
            if (slash == std::string::npos) break;
            start = slash + 1;
        }
    }
    return ctx;
}

std::vector<Resolution> resolvePython(const Import& imp, const std::string& fromFile, const ResolveContext& ctx) {
    std::vector<Resolution> results;
    if (imp.level > 0) {
        // Relative import: climb (level - 1) folders from the file's own package.
        std::optional<std::string> base = dirOf(fromFile);
        for (int k = 1; k < imp.level; k++) {
            if (base->empty()) {
                base.reset();
                break;
            }
            base = dirOf(*base);
        }
        const std::string dots(imp.level, '.');
        if (!base) return {{Kind::Broken, dots + imp.spec}};
        if (!imp.spec.empty()) {
            bool hitSub = false;  // "from .pkg import mod" where mod is a file
            for (const std::string& name : imp.names) {
                if (auto sub = pyFileAt(*base, imp.spec + "." + name, ctx)) {
                    results.push_back({Kind::File, *sub});
                    hitSub = true;
                }
            }
            if (!hitSub) {
                auto mod = pyFileAt(*base, imp.spec, ctx);
                results.push_back(mod ? Resolution{Kind::File, *mod} : Resolution{Kind::Broken, dots + imp.spec});
            }
        } else {
            // from . import a, b
            for (const std::string& name : imp.names) {
                if (auto sub = pyFileAt(*base, name, ctx)) {
                    results.push_back({Kind::File, *sub});
                } else {
                    auto init = pyFileAt(*base, "", ctx);
                    results.push_back(init ? Resolution{Kind::File, *init} : Resolution{Kind::Broken, dots + name});
                }
            }
        }
        return results;
    }

    // Absolute import
    if (imp.spec.empty()) return results;
    const std::vector<std::string> parts = split(imp.spec, '.');
    if (imp.kind == ImportKind::From) {
        bool hitSub = false;
        for (const std::string& name : imp.names) {
            if (auto sub = lookupPyModule(imp.spec + "." + name, fromFile, ctx)) {
                results.push_back({Kind::File, *sub});
                hitSub = true;
            }
        }
        if (hitSub) return results;
    }
    // "import a.b.c" -> try a.b.c, then a.b, then a
    for (size_t k = parts.size(); k > 0; k--) {
        if (auto hit = lookupPyModule(join(parts, '.', k), fromFile, ctx)) return {{Kind::File, *hit}};
    }
    return {{Kind::External, parts[0]}};
}

std::vector<Resolution> resolveJs(const Import& imp, const std::string& fromFile, const ResolveContext& ctx) {
    const std::string& spec = imp.spec;

    // "./x" -> x, x.js, x.ts ..., x/index.js ...
    auto tryPaths = [&](const std::optional<std::string>& base) -> std::optional<std::string> {
        if (!base) return std::nullopt;
        std::vector<std::string> candidates{*base};
        for (const std::string& e : JS_EXTENSIONS) candidates.push_back(*base + e);
        for (const std::string& e : JS_EXTENSIONS) candidates.push_back(joinPath(*base, "index" + e));
        // "./x.js" written in TypeScript source but the file is x.ts
        for (const char* js : {".mjs", ".cjs", ".js"}) {
            if (endsWith(*base, js)) {
                const std::string stripped = base->substr(0, base->size() - std::char_traits<char>::length(js));
                for (const std::string& e : JS_EXTENSIONS) candidates.push_back(stripped + e);
                break;
            }
        }
        for (const std::string& c : candidates)
            if (ctx.fileSet.contains(c)) return c;
        return std::nullopt;
    };

    if (startsWith(spec, ".") || startsWith(spec, "/")) {
        auto hit = tryPaths(startsWith(spec, "/") ? normalize(spec) : normalize(joinPath(dirOf(fromFile), spec)));
        return {hit ? Resolution{Kind::File, *hit} : Resolution{Kind::Broken, spec}};
    }
    if (startsWith(spec, "@/") || startsWith(spec, "~/")) {
        const std::string rest = spec.substr(2);
        auto hit = tryPaths(normalize("src/" + rest));
        if (!hit) hit = tryPaths(normalize(rest));
        return {hit ? Resolution{Kind::File, *hit} : Resolution{Kind::Broken, spec}};
    }
    // Bare name: an outside package. "@scope/pkg/sub" -> "@scope/pkg", "lodash/get" -> "lodash"
    const std::string pkg = startsWith(spec, "node:") ? spec.substr(5) : spec;
    const std::vector<std::string> parts = split(pkg, '/');
    return {{Kind::External, startsWith(pkg, "@") ? join(parts, '/', 2) : parts[0]}};
}

// Compilers look next to the including file first, then in the include
// folders given with -I. We do not know those folders, so we match the end of
// the path against every project file and take the one closest to the
// including file.
std::vector<Resolution> resolveCpp(const Import& imp, const std::string& fromFile, const ResolveContext& ctx) {
    const std::string& spec = imp.spec;
    const bool quoted = imp.kind == ImportKind::Include;
    // <sys/stat.h> -> "sys", <boost/asio.hpp> -> "boost", <vector> -> "vector"
    const std::string package = spec.substr(0, spec.find('/'));

    if (!quoted && isBuiltin(Lang::Cpp, package)) return {{Kind::External, package}};
    if (quoted) {
        if (auto local = normalize(joinPath(dirOf(fromFile), spec)); local && ctx.fileSet.contains(*local))
            return {{Kind::File, *local}};
    }
    if (auto clean = normalize(spec); clean && !startsWith(spec, "..")) {
        if (const std::vector<std::string>* files = ctx.cSuffixes.find(*clean)) {
            // closest = most leading folders in common; files are in path order, so ties go to the first
            const std::vector<std::string> from = split(dirOf(fromFile), '/');
            const std::string* best = nullptr;
            size_t bestShared = 0;
            for (const std::string& f : *files) {
                const std::vector<std::string> dirs = split(dirOf(f), '/');
                size_t shared = 0;
                while (shared < dirs.size() && shared < from.size() && dirs[shared] == from[shared]) shared++;
                if (!best || shared > bestShared) {
                    best = &f;
                    bestShared = shared;
                }
            }
            return {{Kind::File, *best}};
        }
    }
    // Not in the project. "x.h" or "../x.h" should have been there;
    // "gtest/gtest.h" and <SDL2/SDL.h> belong to a library.
    if (quoted && (spec.find('/') == std::string::npos || startsWith(spec, ".")) && !isBuiltin(Lang::Cpp, package))
        return {{Kind::Broken, spec}};
    return {{Kind::External, package}};
}

}  // namespace pdr

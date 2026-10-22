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

// A module built from Cython or C/C++ (scipy: transform/_rotation_cy.pyx,
// _duccfft/pyduccfft.cxx) has no .py file. Without this check "import
// pkg._rotation_cy" would fall back to pkg/__init__.py and invent a cycle.
bool isCompiledModule(const std::string& dir, const std::string& name, const ResolveContext& ctx) {
    for (const char* ext : {".pyx", ".pxd", ".pyi", ".c", ".cc", ".cpp", ".cxx"}) {
        const std::string p = joinPath(dir, name + ext);
        if (ctx.otherFiles.contains(p) || ctx.fileSet.contains(p)) return true;
    }
    return false;
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

// An import that points at a project path but no code file: is that path a
// non-code file, or inside a build folder (dist/, build/) that is made later?
// "./template/main.js?raw" (Vite) reads the file before the "?" as text, so
// even a code file is not imported as code there.
bool isNotCode(const std::optional<std::string>& path, const ResolveContext& ctx) {
    if (!path) return false;
    const size_t query = path->find('?');
    const std::string file = path->substr(0, query);
    return ctx.otherFiles.contains(file) || isIgnored(file) ||
           (query != std::string::npos && ctx.fileSet.contains(file));
}

}  // namespace

ResolveContext makeContext(const std::vector<std::string>& sortedPaths, const std::vector<std::string>& otherPaths) {
    ResolveContext ctx;
    for (const std::string& p : sortedPaths) ctx.fileSet.insert(p);
    for (const std::string& p : otherPaths) ctx.otherFiles.insert(p);
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

// The modules an import names, without the packages Python runs first.
static std::vector<Resolution> resolvePythonModules(const Import& imp, const std::string& fromFile, const ResolveContext& ctx) {
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
                } else if (isCompiledModule(*base, name, ctx)) {
                    results.push_back({Kind::NotCode, dots + name});
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
    // The package itself is loading when fromFile is inside it.
    auto compiledIn = [&](const std::string& pkgInit) -> std::vector<Resolution> {
        std::vector<Resolution> out{{Kind::NotCode, imp.spec}};
        if (!startsWith(fromFile, dirOf(pkgInit) + "/")) out.push_back({Kind::File, pkgInit});
        return out;
    };
    if (imp.kind == ImportKind::From) {
        bool hitSub = false;
        const auto pkg = lookupPyModule(imp.spec, fromFile, ctx);
        const bool isPackage = pkg && endsWith(*pkg, "__init__.py");
        bool compiled = false;
        for (const std::string& name : imp.names) {
            if (auto sub = lookupPyModule(imp.spec + "." + name, fromFile, ctx)) {
                results.push_back({Kind::File, *sub});
                hitSub = true;
            } else if (isPackage && isCompiledModule(dirOf(*pkg), name, ctx)) {
                compiled = true;
            }
        }
        if (hitSub) return results;
        if (compiled) return compiledIn(*pkg);
    }
    // "import a.b.c" -> try a.b.c, then a.b, then a
    for (size_t k = parts.size(); k > 0; k--) {
        if (auto hit = lookupPyModule(join(parts, '.', k), fromFile, ctx)) {
            if (k < parts.size() && endsWith(*hit, "__init__.py") && isCompiledModule(dirOf(*hit), parts[k], ctx))
                return compiledIn(*hit);
            return {{Kind::File, *hit}};
        }
    }
    return {{Kind::External, parts[0]}};
}

// Python runs a module's packages before the module: "from jesse.models.Route
// import Route" runs jesse/models/__init__.py first, and that file's imports can
// close a circle (jesse: models/__init__ -> Exchange -> routes). A package the
// importing file is itself inside is already loading, so it adds nothing.
std::vector<Resolution> resolvePython(const Import& imp, const std::string& fromFile, const ResolveContext& ctx) {
    std::vector<Resolution> results = resolvePythonModules(imp, fromFile, ctx);
    const size_t modules = results.size();
    for (size_t i = 0; i < modules; i++) {
        if (results[i].kind != Kind::File) continue;
        const std::string& file = results[i].value;
        std::string dir = endsWith(file, "__init__.py") ? dirOf(dirOf(file)) : dirOf(file);
        for (; !dir.empty() && ctx.fileSet.contains(dir + "/__init__.py"); dir = dirOf(dir)) {
            if (startsWith(fromFile, dir + "/")) break;  // this package and the ones above it are loading
            const std::string init = dir + "/__init__.py";
            const bool seen = std::any_of(results.begin(), results.end(),
                                          [&](const Resolution& r) { return r.kind == Kind::File && r.value == init; });
            if (!seen) results.push_back({Kind::File, init});
        }
    }
    return results;
}

std::vector<Resolution> resolveJs(const Import& imp, const std::string& fromFile, const ResolveContext& ctx) {
    const std::string& spec = imp.spec;

    // "./x" -> x, x.js, x.ts ..., x/index.js ...
    auto tryPaths = [&](const std::optional<std::string>& base) -> std::optional<std::string> {
        if (!base) return std::nullopt;
        std::vector<std::string> candidates{*base};
        for (const std::string& e : JS_EXTENSIONS) candidates.push_back(*base + e);
        candidates.push_back(*base + ".d.ts");  // TypeScript: "./utils" can be utils.d.ts (types only)
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
        const auto base = startsWith(spec, "/") ? normalize(spec) : normalize(joinPath(dirOf(fromFile), spec));
        if (auto hit = tryPaths(base)) return {{Kind::File, *hit}};
        return {{isNotCode(base, ctx) ? Kind::NotCode : Kind::Broken, spec}};
    }
    if (startsWith(spec, "@/") || startsWith(spec, "~/")) {
        const std::string rest = spec.substr(2);
        const auto inSrc = normalize("src/" + rest), atRoot = normalize(rest);
        auto hit = tryPaths(inSrc);
        if (!hit) hit = tryPaths(atRoot);
        if (hit) return {{Kind::File, *hit}};
        return {{isNotCode(inSrc, ctx) || isNotCode(atRoot, ctx) ? Kind::NotCode : Kind::Broken, spec}};
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
        const auto local = normalize(joinPath(dirOf(fromFile), spec));
        if (local && ctx.fileSet.contains(*local)) return {{Kind::File, *local}};
        if (isNotCode(local, ctx)) return {{Kind::NotCode, spec}};  // #include "commands.def"
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

// Package Dependency Resolver - console program
//
//   pdr [folder]                 interactive menu
//   pdr [folder] --report        print the whole analysis
//   pdr [folder] --why A B       why does file A need file B?
//   pdr [folder] --impact FILE   what breaks if FILE changes?
//   pdr [folder] --search TEXT   find files by name prefix
//   pdr [folder] --check         for CI: exit 1 on cycles or broken imports
//   pdr [folder] --json          the whole analysis as JSON
//   pdr [folder] --dot           the graph for Graphviz (dot -Tsvg)
//
// Without a folder, the current folder is analysed.
//
// Exit codes: 0 = OK, 1 = --check found problems, 2 = bad usage or unreadable folder.
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "analyzer.h"
#include "dot_export.h"
#include "json_report.h"

using namespace pdr;

namespace {

std::string plural(size_t n, const std::string& one, const std::string& many = "") {
    return std::to_string(n) + " " + (n == 1 ? one : (many.empty() ? one + "s" : many));
}

void heading(const std::string& title) {
    std::cout << "\n== " << title << " " << std::string(title.size() < 60 ? 60 - title.size() : 0, '=') << "\n\n";
}

void printSummary(const Result& r, const std::string& folder) {
    const Stats& s = r.stats;
    heading("Summary");
    std::cout << "  Project              " << folder << "\n"
              << "  Files                " << s.files << "  (" << s.pythonFiles << " Python, " << s.jsFiles
              << " JavaScript/TypeScript, " << s.cppFiles << " C/C++)\n"
              << "  Import statements    " << s.importStatements
              << (s.deferredImports + s.typeCheckingImports
                      ? "  (" + std::to_string(s.deferredImports) + " inside functions, " +
                            std::to_string(s.typeCheckingImports) + " type-checking only)"
                      : "")
              << "\n"
              << "  Links between files  " << s.edges << "\n"
              << "  Circular imports     " << s.cycles << (s.cycles ? "  (" + plural(s.filesInCycles, "file") + ")" : "")
              << "\n"
              << "  Broken imports       " << s.broken << "\n"
              << "  Outside packages     " << s.externals << "  (" << s.thirdParty << " to install)\n"
              << "  Layers               " << s.layers << "\n"
              << "  Analysed in          " << std::fixed << std::setprecision(1) << s.ms << " ms\n";
}

void printSteps(const std::vector<Step>& steps) {
    for (const Step& st : steps) {
        std::cout << "    " << st.from << "  imports  " << st.to << "\n"
                  << "        line " << st.line << ":  " << st.text << "\n";
    }
}

void printCycles(const Result& r) {
    heading("Circular imports (Tarjan's strongly connected components)");
    const int later = r.stats.deferredImports + r.stats.typeCheckingImports;
    if (later) {
        std::cout << "  Only imports that run while a module loads are counted. The " << plural(later, "import")
                  << " inside functions or\n  for type checking only run later or never, so they cannot cause a circular import error.\n\n";
    }
    if (r.cycles.empty()) {
        std::cout << "  None. No files import each other in a loop.\n";
        return;
    }
    for (size_t i = 0; i < r.cycles.size(); i++) {
        const Cycle& c = r.cycles[i];
        std::cout << "  " << i + 1 << ". " << plural(c.files.size(), "file") << " import each other:\n";
        std::cout << "    ";
        for (size_t k = 0; k < c.loop.size(); k++) std::cout << (k ? " -> " : "") << c.loop[k];
        std::cout << "\n\n  Lines that make the loop:\n";
        printSteps(c.steps);
        if (c.fixTooLarge) {
            std::cout << "\n  Too many links in this group to search for a fix.\n";
        } else {
            std::cout << "\n  To break it, remove " << (c.fix.size() == 1 ? "this import" : "these imports")
                      << " (greedy search, fewest files left in a loop):\n";
            for (const Step& st : c.fix)
                std::cout << "    " << st.from << "  line " << st.line << ":  " << st.text << "\n";
        }
        std::cout << "\n";
    }
}

void printBroken(const Result& r) {
    heading("Broken imports (point to files that do not exist)");
    if (r.broken.empty()) {
        std::cout << "  None. Every local import points to a real file.\n";
        return;
    }
    for (const BrokenImport& b : r.broken) {
        std::cout << "  " << b.file << "  line " << b.line << "\n"
                  << "      " << b.text << "\n"
                  << "      '" << b.spec << "' was not found\n";
    }
}

void printOrder(const Result& r) {
    heading("Build order (Kahn's topological sort, dependencies first)");
    int step = 1;
    for (const OrderUnit& u : r.order) {
        std::cout << "  " << std::setw(3) << step++ << ".  [layer " << u.layer << "]  ";
        if (u.cyclic) {
            std::cout << "loaded together (cycle):\n";
            for (const std::string& f : u.files) std::cout << "                      " << f << "\n";
        } else {
            std::cout << u.files[0] << "\n";
        }
    }
}

void printCritical(const Result& r, size_t limit = 10) {
    heading("Most critical files (BFS on the reverse graph)");
    if (r.critical.empty()) {
        std::cout << "  No file is imported by another file.\n";
        return;
    }
    std::cout << "  " << std::left << std::setw(40) << "File" << std::right << std::setw(12) << "Affects"
              << std::setw(14) << "Imported by" << "\n";
    for (size_t i = 0; i < r.critical.size() && i < limit; i++) {
        const Metrics& m = r.metricsOf(r.critical[i]);
        std::cout << "  " << std::left << std::setw(40) << r.critical[i] << std::right << std::setw(12)
                  << plural(m.dependents, "file") << std::setw(14) << plural(m.fanIn, "file") << "\n";
    }
}

void printExternals(const Result& r) {
    heading("Outside packages");
    if (r.externals.empty()) {
        std::cout << "  None.\n";
        return;
    }
    bool shownHeader[2] = {false, false};
    for (const ExternalPackage& e : r.externals) {
        if (!shownHeader[e.builtin]) {
            std::cout << (e.builtin ? "\n  Built into the language:\n" : "  Must be installed:\n");
            shownHeader[e.builtin] = true;
        }
        std::cout << "    " << std::left << std::setw(20) << e.name << std::right << "used by "
                  << plural(e.files.size(), "file") << "\n";
    }
}

void printSearch(const Result& r, const std::string& prefix) {
    const std::vector<std::string> hits = r.trie.search(prefix, 20);
    std::cout << "\n  " << plural(hits.size(), "match", "matches") << " for '" << prefix << "' (trie prefix search):\n";
    for (const std::string& h : hits) std::cout << "    " << h << "\n";
}

// Exact path, or a unique prefix match of a file name / path.
std::optional<std::string> findFile(const Result& r, const std::string& input) {
    if (r.graph.has(input)) return input;
    std::vector<std::string> hits = r.trie.search(input, 10);
    if (hits.empty()) {
        // the end of a path, like "actions/__init__.py"
        for (int id : r.graph.nodesByName())
            if (endsWith(r.graph.name(id), "/" + input)) hits.push_back(r.graph.name(id));
    }
    if (hits.size() == 1) return hits[0];
    // "move.py" should pick fad/actions/move.py even if move_utils.py exists,
    // but "__init__.py" in three folders is still ambiguous.
    std::vector<std::string> exact;
    for (const std::string& h : hits)
        if (baseName(h) == input) exact.push_back(h);
    if (exact.size() == 1) return exact[0];
    if (hits.empty()) std::cout << "  No file matches '" << input << "'.\n";
    else {
        std::cout << "  '" << input << "' matches several files, be more specific:\n";
        for (const std::string& h : hits) std::cout << "    " << h << "\n";
    }
    return std::nullopt;
}

void printWhy(const Result& r, const std::string& a, const std::string& b) {
    heading("Why does " + baseName(a) + " need " + baseName(b) + "?");
    if (a == b) {
        std::cout << "  Pick two different files.\n";
        return;
    }
    if (auto why = explainPath(r, a, b)) {
        std::cout << "  " << a << " needs " << b << " through " << plural(why->steps.size(), "import")
                  << " (shortest chain, BFS):\n\n";
        printSteps(why->steps);
        return;
    }
    std::cout << "  " << a << " does not depend on " << b << ".\n";
    if (auto back = explainPath(r, b, a)) {
        std::cout << "  It is the other way round: " << b << " depends on " << a << " through "
                  << plural(back->steps.size(), "step") << ".\n";
    } else {
        std::cout << "  There is no chain of imports between them in either direction.\n";
    }
}

void printImpact(const Result& r, const std::string& file) {
    const std::vector<std::string> hit = impactOf(r, file);
    heading("If " + baseName(file) + " changes");
    if (hit.empty()) {
        std::cout << "  No other file depends on " << file << ".\n";
        return;
    }
    std::cout << "  " << plural(hit.size(), "file") << " may break (directly or through other imports):\n";
    for (const std::string& f : hit) std::cout << "    " << f << "\n";
}

void printFile(const Result& r, const std::string& file) {
    const FileInfo& info = r.infoOf(file);
    const Metrics& m = r.metricsOf(file);
    heading(file);
    std::cout << "  " << langName(info.lang) << ", " << plural(info.lines, "line") << ", layer " << m.layer
              << (m.inCycle ? ", part of a circular import" : "") << "\n"
              << "  Needs " << plural(m.dependencies, "file") << ", affects " << plural(m.dependents, "file") << "\n\n"
              << "  Imports:\n";
    if (info.imports.empty()) std::cout << "    (nothing)\n";
    for (const ImportRef& imp : info.imports) {
        const char* kind = imp.type == ImportRef::Type::File       ? "file    "
                           : imp.type == ImportRef::Type::External ? "package "
                                                                   : "BROKEN  ";
        std::cout << "    line " << std::setw(4) << std::left << imp.line << std::right << " " << kind << imp.target
                  << "\n";
    }
    std::cout << "\n  Imported by:\n";
    const auto& in = r.graph.in(r.graph.id(file));
    if (in.empty()) std::cout << "    (nobody - this is an entry point)\n";
    for (const Edge& e : in) std::cout << "    " << r.graph.name(e.to) << "  (line " << e.evidence.line << ")\n";
}

void printReport(const Result& r, const std::string& folder) {
    printSummary(r, folder);
    printCycles(r);
    printBroken(r);
    printOrder(r);
    printCritical(r);
    printExternals(r);
}

// One problem per line as "file:line: message", the format CI logs and
// editors turn into links. Returns true if the check failed.
bool printCheck(const Result& r, bool allowCycles, bool allowBroken) {
    int problems = 0;
    if (!allowCycles) {
        for (const Cycle& c : r.cycles) {
            std::string loop;
            for (size_t k = 0; k < c.loop.size(); k++) loop += (k ? " -> " : "") + c.loop[k];
            const Step& first = c.steps.front();
            std::cout << first.from << ":" << first.line << ": error: circular import: " << loop << "\n";
            for (const Step& st : c.steps)
                std::cout << "    " << st.from << ":" << st.line << ":  " << st.text << "\n";
            for (const Step& st : c.fix)
                std::cout << "    fix: remove " << st.from << ":" << st.line << ":  " << st.text << "\n";
            problems++;
        }
    }
    if (!allowBroken) {
        for (const BrokenImport& b : r.broken) {
            std::cout << b.file << ":" << b.line << ": error: broken import '" << b.spec
                      << "' (no such file in the project)\n";
            problems++;
        }
    }
    std::cout << "pdr: " << plural(r.stats.files, "file") << ", " << plural(r.stats.cycles, "circular import")
              << (allowCycles ? " (allowed)" : "") << ", " << plural(r.stats.broken, "broken import")
              << (allowBroken ? " (allowed)" : "") << " -> " << (problems ? "FAILED" : "OK") << "\n";
    return problems > 0;
}

std::string ask(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) return "";
    return trim(line);
}

void menu(const Result& r, const std::string& folder) {
    for (;;) {
        std::cout << "\n==== Package Dependency Resolver ====\n"
                  << "Project: " << folder << "  (" << plural(r.stats.files, "file") << ", "
                  << plural(r.stats.edges, "import link") << ")\n\n"
                  << "  1. Summary\n"
                  << "  2. Circular imports\n"
                  << "  3. Broken imports\n"
                  << "  4. Build order\n"
                  << "  5. Most critical files\n"
                  << "  6. Outside packages\n"
                  << "  7. Why does file A need file B?\n"
                  << "  8. What breaks if a file changes?\n"
                  << "  9. Search files\n"
                  << " 10. File details\n"
                  << "  0. Exit\n";
        const std::string choice = ask("\nChoice: ");
        if (!std::cin || choice == "0") return;
        if (choice == "1") printSummary(r, folder);
        else if (choice == "2") printCycles(r);
        else if (choice == "3") printBroken(r);
        else if (choice == "4") printOrder(r);
        else if (choice == "5") printCritical(r);
        else if (choice == "6") printExternals(r);
        else if (choice == "7") {
            auto a = findFile(r, ask("File A (name or path): "));
            if (!a) continue;
            auto b = findFile(r, ask("File B (name or path): "));
            if (b) printWhy(r, *a, *b);
        } else if (choice == "8") {
            if (auto f = findFile(r, ask("File (name or path): "))) printImpact(r, *f);
        } else if (choice == "9") {
            printSearch(r, ask("Start of a file name or path: "));
        } else if (choice == "10") {
            if (auto f = findFile(r, ask("File (name or path): "))) printFile(r, *f);
        } else {
            std::cout << "  Please type a number from the menu.\n";
        }
    }
}

enum Exit { OK = 0, PROBLEMS = 1, USAGE = 2 };

const char* const HELP =
    "Usage: pdr [project-folder] [command]\n"
    "\n"
    "Without a folder, the current folder is analysed.\n"
    "\n"
    "Commands:\n"
    "  (none)                 interactive menu\n"
    "  --report               print the whole analysis\n"
    "  --why FILE_A FILE_B    why does FILE_A need FILE_B?\n"
    "  --impact FILE          every file that may break if FILE changes\n"
    "  --search TEXT          find files whose name or path starts with TEXT\n"
    "  --check                for CI: list circular and broken imports, exit 1 if any\n"
    "      --allow-cycles     do not fail on circular imports\n"
    "      --allow-broken     do not fail on broken imports\n"
    "  --json                 the whole analysis as JSON\n"
    "  --dot                  the graph in Graphviz format; draw it with: dot -Tsvg -o graph.svg\n"
    "  --help                 show this help\n"
    "\n"
    "Files can be given as a path, a file name, or the end of a path.\n"
    "Exit codes: 0 OK, 1 --check found problems, 2 bad usage or unreadable folder.\n";

int usage(const std::string& problem) {
    std::cerr << "pdr: " << problem << "\n\n" << HELP;
    return USAGE;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    for (const std::string& a : args) {
        if (a == "--help" || a == "-h") {
            std::cout << HELP;
            return OK;
        }
    }
    // Flags may appear anywhere; everything else is the folder, a command and its arguments.
    bool allowCycles = false, allowBroken = false;
    std::vector<std::string> words;
    for (const std::string& a : args) {
        if (a == "--allow-cycles") allowCycles = true;
        else if (a == "--allow-broken") allowBroken = true;
        else words.push_back(a);
    }
    // The folder comes first and is optional: "pdr --check" checks the current folder.
    if (words.empty() || startsWith(words[0], "-")) words.insert(words.begin(), ".");
    const std::string folder = words[0];
    const std::string command = words.size() > 1 ? words[1] : "";
    const std::vector<std::string> params(words.begin() + std::min<size_t>(2, words.size()), words.end());

    const struct {
        const char* name;
        size_t params;
    } commands[] = {{"", 0},         {"--report", 0}, {"--why", 2},  {"--impact", 1},
                    {"--search", 1}, {"--check", 0},  {"--json", 0}, {"--dot", 0}};
    bool known = false;
    for (const auto& c : commands) {
        if (command != c.name) continue;
        known = true;
        if (params.size() != c.params)
            return usage(command + " takes " + plural(c.params, "argument") + ", got " + std::to_string(params.size()));
    }
    if (!known) return usage("unknown command '" + command + "'");
    if ((allowCycles || allowBroken) && command != "--check")
        return usage("--allow-cycles and --allow-broken only work with --check");

    Result r;
    try {
        r = analyze(loadProject(folder));
    } catch (const std::exception& e) {
        std::cerr << "pdr: " << e.what() << "\n";
        return USAGE;
    }
    if (r.stats.files == 0) {
        std::cerr << "pdr: no Python, JavaScript or C/C++ files were found in " << folder << "\n";
        return USAGE;
    }

    if (command.empty()) {
        menu(r, folder);
    } else if (command == "--report") {
        printReport(r, folder);
    } else if (command == "--why") {
        auto a = findFile(r, params[0]);
        auto b = findFile(r, params[1]);
        if (!a || !b) return USAGE;
        printWhy(r, *a, *b);
    } else if (command == "--impact") {
        auto f = findFile(r, params[0]);
        if (!f) return USAGE;
        printImpact(r, *f);
    } else if (command == "--search") {
        printSearch(r, params[0]);
    } else if (command == "--check") {
        return printCheck(r, allowCycles, allowBroken) ? PROBLEMS : OK;
    } else if (command == "--json") {
        writeJson(std::cout, r, folder);
    } else if (command == "--dot") {
        writeDot(std::cout, r);
    }
    return OK;
}

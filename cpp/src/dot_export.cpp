#include "dot_export.h"

#include <map>
#include <string>
#include <vector>

#include "hash_map.h"

namespace pdr {

namespace {

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '\n') {
            out += "\\n";  // a line break inside a label
            continue;
        }
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

}  // namespace

void writeDot(std::ostream& out, const Result& r) {
    const Graph& g = r.graph;

    HashMap<std::string, int> cycleOf;  // file -> index of its cycle
    HashSet<std::uint64_t> fixLinks;    // imports the fix plan removes
    for (size_t i = 0; i < r.cycles.size(); i++) {
        for (const std::string& f : r.cycles[i].files) cycleOf.insert(f, static_cast<int>(i));
        for (const Step& st : r.cycles[i].fix) fixLinks.insert(linkKey(g.id(st.from), g.id(st.to)));
    }
    HashSet<std::uint64_t> ruleLinks;  // imports that break pdr.rules
    for (const RuleViolation& v : r.ruleCheck.violations) ruleLinks.insert(linkKey(g.id(v.file), g.id(v.target)));
    HashSet<std::string> unused;
    for (const std::string& f : r.unused) unused.insert(f);

    out << "digraph dependencies {\n"
        << "  // A -> B means \"A imports B\". Made by pdr --dot.\n"
        << "  rankdir=LR;\n"
        << "  graph [fontname=\"Helvetica\", fontsize=11, nodesep=0.25, ranksep=0.6];\n"
        << "  node [shape=box, style=\"rounded,filled\", fillcolor=\"#f4f4f4\", color=\"#9a9a9a\", "
           "fontname=\"Helvetica\", fontsize=10];\n"
        << "  edge [color=\"#8a8a8a\", arrowsize=0.6, fontname=\"Helvetica\"];\n";

    // One box per folder, in path order.
    std::map<std::string, std::vector<int>> byFolder;
    for (int id : g.nodesByName()) byFolder[dirOf(g.name(id))].push_back(id);
    int cluster = 0;
    for (const auto& [folder, ids] : byFolder) {
        const std::string indent = folder.empty() ? "  " : "    ";
        if (!folder.empty())
            out << "  subgraph cluster_" << cluster++ << " {\n"
                << "    label=" << quote(folder + "/") << "; style=\"rounded\"; color=\"#c8c8c8\";\n";
        for (int id : ids) {
            const std::string& name = g.name(id);
            out << indent << quote(name) << " [label=" << quote(baseName(name)) << ", tooltip=" << quote(name);
            if (cycleOf.contains(name)) out << ", fillcolor=\"#fde3e1\", color=\"#d0342c\"";
            else if (unused.contains(name)) out << ", style=\"rounded,dashed\", fontcolor=\"#8a8a8a\", color=\"#b0b0b0\"";
            out << "];\n";
        }
        if (!folder.empty()) out << "  }\n";
    }

    for (int a : g.nodesByName()) {
        for (const Edge& e : g.out(a)) {
            out << "  " << quote(g.name(a)) << " -> " << quote(g.name(e.to))
                << " [tooltip=" << quote("line " + std::to_string(e.evidence.line) + ": " + e.evidence.text);
            const int* ca = cycleOf.find(g.name(a));
            const int* cb = cycleOf.find(g.name(e.to));
            const bool atLoad = r.loadGraph.edge(a, e.to) != nullptr;
            if (!atLoad) out << ", style=dotted";  // inside a function or type-checking only
            else if (ca && cb && *ca == *cb) out << ", color=\"#d0342c\", penwidth=1.6";
            if (fixLinks.contains(linkKey(a, e.to))) out << ", style=dashed, label=\"remove\", fontcolor=\"#d0342c\", fontsize=9";
            else if (ruleLinks.contains(linkKey(a, e.to)))
                out << ", color=\"#e07b00\", penwidth=1.6, label=\"breaks rule\", fontcolor=\"#e07b00\", fontsize=9";
            out << "];\n";
        }
    }

    for (size_t i = 0; i < r.broken.size(); i++) {
        const BrokenImport& b = r.broken[i];
        const std::string missing = "missing " + std::to_string(i);
        out << "  " << quote(missing) << " [label=" << quote(b.spec + "\nnot found") << ", shape=note, "
            << "style=\"dashed\", color=\"#d0342c\", fontcolor=\"#d0342c\"];\n"
            << "  " << quote(b.file) << " -> " << quote(missing) << " [style=dashed, color=\"#d0342c\", tooltip="
            << quote("line " + std::to_string(b.line) + ": " + b.text) << "];\n";
    }
    out << "}\n";
}

}  // namespace pdr

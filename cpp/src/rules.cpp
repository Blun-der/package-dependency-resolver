#include "rules.h"

#include <sstream>
#include <stdexcept>

#include "path_trie.h"
#include "util.h"

namespace pdr {

namespace {

// "./fad/utils/" -> "fad/utils"
std::string cleanPattern(std::string p) {
    while (startsWith(p, "./")) p = p.substr(2);
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    return p;
}

// One stored pattern: which rule it belongs to and what part it plays.
struct Pattern {
    enum class Kind { ForbidFrom, ForbidTo, Layer, Entry } kind;
    int rule;   // index into forbids / layers / entries
    int layer;  // position in the layer list, top = 0
    std::string text;
    int line;
};

}  // namespace

Rules parseRules(const std::string& text, const std::string& source) {
    Rules rules;
    rules.source = source;
    int lineNo = 0;
    for (const std::string& raw : split(text, '\n')) {
        lineNo++;
        std::istringstream words(raw.substr(0, raw.find('#')));
        std::vector<std::string> w;
        for (std::string word; words >> word;) w.push_back(word);
        if (w.empty()) continue;
        auto fail = [&](const std::string& why) {
            throw std::runtime_error(source + ":" + std::to_string(lineNo) + ": " + why);
        };

        if (w[0] == "layers") {
            if (w.size() < 3) fail("'layers' needs at least two paths, top layer first");
            Rules::Layers stack{{}, lineNo};
            for (size_t i = 1; i < w.size(); i++) {
                std::vector<std::string> level;
                for (const std::string& p : split(w[i], '|'))
                    if (!p.empty()) level.push_back(cleanPattern(p));
                if (level.empty()) fail("empty layer in 'layers'");
                stack.levels.push_back(level);
            }
            rules.layers.push_back(stack);
        } else if (w[0] == "forbid") {
            if (w.size() != 4 || w[2] != "->") fail("write 'forbid FROM -> TO'");
            rules.forbids.push_back({cleanPattern(w[1]), cleanPattern(w[3]), lineNo});
        } else if (w[0] == "entry") {
            if (w.size() < 2) fail("'entry' needs at least one path");
            for (size_t i = 1; i < w.size(); i++) {
                rules.entries.push_back(cleanPattern(w[i]));
                rules.entryLines.push_back(lineNo);
            }
        } else {
            fail("unknown rule '" + w[0] + "' (expected layers, forbid or entry)");
        }
    }
    return rules;
}

RuleCheck checkRules(const Graph& graph, const Rules& rules) {
    std::vector<Pattern> patterns;
    for (size_t r = 0; r < rules.forbids.size(); r++) {
        const Rules::Forbid& f = rules.forbids[r];
        patterns.push_back({Pattern::Kind::ForbidFrom, static_cast<int>(r), 0, f.from, f.line});
        patterns.push_back({Pattern::Kind::ForbidTo, static_cast<int>(r), 0, f.to, f.line});
    }
    for (size_t r = 0; r < rules.layers.size(); r++) {
        const Rules::Layers& l = rules.layers[r];
        for (size_t k = 0; k < l.levels.size(); k++)
            for (const std::string& p : l.levels[k])
                patterns.push_back({Pattern::Kind::Layer, static_cast<int>(r), static_cast<int>(k), p, l.line});
    }
    for (size_t r = 0; r < rules.entries.size(); r++)
        patterns.push_back({Pattern::Kind::Entry, static_cast<int>(r), 0, rules.entries[r], rules.entryLines[r]});

    PathTrie trie;
    for (size_t i = 0; i < patterns.size(); i++) trie.add(patterns[i].text, static_cast<int>(i));

    // One walk of the trie per file: the patterns it falls under, and its
    // layer pattern in every layer list (the deepest match wins, so a rule
    // can name both "app" and "app/core").
    const int n = graph.size();
    std::vector<std::vector<int>> matched(n);
    std::vector<std::vector<int>> layerOf(rules.layers.size(), std::vector<int>(n, -1));  // pattern index
    std::vector<int> uses(patterns.size(), 0);
    for (int f = 0; f < n; f++) {
        matched[f] = trie.matches(graph.name(f));
        for (int p : matched[f]) {
            uses[p]++;
            if (patterns[p].kind == Pattern::Kind::Layer) layerOf[patterns[p].rule][f] = p;
        }
    }

    RuleCheck out;
    out.source = rules.source;
    for (size_t i = 0; i < patterns.size(); i++) {
        if (uses[i] == 0)
            out.warnings.push_back(rules.source + ":" + std::to_string(patterns[i].line) + ": warning: '" +
                                   patterns[i].text + "' matches no file");
    }

    for (int a : graph.nodesByName()) {
        for (const Edge& e : graph.out(a)) {
            const int b = e.to;
            auto report = [&](const std::string& rule, int ruleLine) {
                out.violations.push_back({graph.name(a), e.evidence.line, e.evidence.text, graph.name(b), rule, ruleLine});
            };
            for (int p : matched[a]) {
                if (patterns[p].kind != Pattern::Kind::ForbidFrom) continue;
                const Rules::Forbid& f = rules.forbids[patterns[p].rule];
                for (int q : matched[b]) {
                    if (patterns[q].kind == Pattern::Kind::ForbidTo && patterns[q].rule == patterns[p].rule) {
                        report(f.from + " must not import " + f.to, f.line);
                        break;
                    }
                }
            }
            for (size_t s = 0; s < rules.layers.size(); s++) {
                const int pa = layerOf[s][a], pb = layerOf[s][b];
                if (pa < 0 || pb < 0 || patterns[pa].layer <= patterns[pb].layer) continue;  // same layer or lower: allowed
                report(patterns[pa].text + " must not import " + patterns[pb].text + ", a layer above it",
                       rules.layers[s].line);
            }
        }
    }
    return out;
}

}  // namespace pdr

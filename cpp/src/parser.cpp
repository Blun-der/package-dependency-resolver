#include "parser.h"

#include <algorithm>
#include "hash_map.h"
#include "util.h"

namespace pdr {

namespace {

// Start offset of every line, so "which line is offset i on?" is a binary
// search instead of counting newlines from the top each time.
class LineIndex {
public:
    explicit LineIndex(const std::string& text) {
        starts_.push_back(0);
        for (size_t i = 0; i < text.size(); i++)
            if (text[i] == '\n') starts_.push_back(i + 1);
    }
    int lineAt(size_t offset) const {
        return static_cast<int>(std::upper_bound(starts_.begin(), starts_.end(), offset) - starts_.begin());
    }

private:
    std::vector<size_t> starts_;
};

bool isBlank(char c) { return c == ' ' || c == '\t'; }
bool isQuote(char c) { return c == '\'' || c == '"'; }
bool isDottedChar(char c) { return isWordChar(c) || c == '.'; }

bool wordAt(const std::string& s, size_t pos, const char* word) {
    return s.compare(pos, std::char_traits<char>::length(word), word) == 0;
}

size_t skipBlanks(const std::string& s, size_t pos, size_t end) {
    while (pos < end && isBlank(s[pos])) pos++;
    return pos;
}

size_t skipSpaces(const std::string& s, size_t pos) {
    while (pos < s.size() && isSpace(s[pos])) pos++;
    return pos;
}

std::string lineText(const std::vector<std::string>& lines, int line) {
    return line >= 1 && line <= static_cast<int>(lines.size()) ? trim(lines[line - 1]) : "";
}

// Names after "from x import": "(a, b as c,\n d)" -> [a, b, d]
std::vector<std::string> importedNames(std::string body) {
    for (char& c : body)
        if (c == '(' || c == ')' || c == '\\') c = ' ';
    std::vector<std::string> names;
    for (const std::string& part : split(body, ',')) {
        std::string name = trim(part);
        size_t ws = 0;
        while (ws < name.size() && !isSpace(name[ws])) ws++;
        name = name.substr(0, ws);
        if (!name.empty() && name != "*") names.push_back(name);
    }
    return names;
}

// "if TYPE_CHECKING:" or "if typing.TYPE_CHECKING:" starting at p
bool isTypeCheckingIf(const std::string& s, size_t p, size_t end) {
    if (!wordAt(s, p, "if") || p + 2 >= end || !isBlank(s[p + 2])) return false;
    const size_t colon = s.find(':', p);
    if (colon == std::string::npos || colon > end) return false;
    const std::string cond = trim(s.substr(p + 2, colon - p - 2));
    const std::string name = "TYPE_CHECKING";
    if (cond == name) return true;
    return cond.size() > name.size() + 1 && cond.compare(cond.size() - name.size() - 1, std::string::npos, "." + name) == 0 &&
           std::all_of(cond.begin(), cond.end(), isDottedChar);
}

// if __name__ == "__main__":   (src is the original text: the mask blanks strings)
bool isMainGuard(const std::string& src, size_t p, size_t end) {
    if (!wordAt(src, p, "if") || p + 2 >= end || !isBlank(src[p + 2])) return false;
    std::string cond;
    for (size_t i = p + 2; i < end && src[i] != ':'; i++)
        if (!isSpace(src[i])) cond += src[i] == '\'' ? '"' : src[i];
    return cond == "__name__==\"__main__\"" || cond == "\"__main__\"==__name__";
}

// When each line of a Python file runs, indexed by line number - 1 (`text` is
// the masked source, `src` the original). The blocks around a line are the
// nearest earlier statements with less indentation: inside a "def" or an
// "if __name__ == '__main__':" it runs later, inside "if TYPE_CHECKING:" never.
// Lines that continue a statement (open brackets, a trailing backslash)
// belong to the statement they continue.
std::vector<When> pythonTiming(const std::string& text, const std::string& src) {
    struct Block {
        size_t indent;
        When inside;  // when the lines indented below this statement run
    };
    std::vector<Block> open;  // innermost last
    std::vector<When> timing;
    When current = When::Load;
    int depth = 0;
    bool continued = false;
    for (size_t start = 0;;) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        const size_t p = skipBlanks(text, start, end);

        if (p < end && text[p] != '\r' && depth == 0 && !continued) {
            const size_t indent = p - start;
            while (!open.empty() && open.back().indent >= indent) open.pop_back();
            current = open.empty() ? When::Load : open.back().inside;
            When inside = current;
            if (current == When::Load) {
                const size_t d = wordAt(text, p, "async") ? skipBlanks(text, p + 5, end) : p;
                if (wordAt(text, d, "def") && d + 3 < end && isBlank(text[d + 3])) inside = When::Deferred;
                else if (isMainGuard(src, p, end)) inside = When::Deferred;
                else if (isTypeCheckingIf(text, p, end)) inside = When::TypeChecking;
            }
            open.push_back({indent, inside});
        }
        timing.push_back(current);

        size_t last = end;
        for (size_t i = start; i < end; i++) {
            const char c = text[i];
            if (c == '(' || c == '[' || c == '{') depth++;
            else if ((c == ')' || c == ']' || c == '}') && depth > 0) depth--;
            if (!isSpace(c)) last = i;
        }
        continued = last < end && text[last] == '\\';

        if (end == text.size()) break;
        start = end + 1;
    }
    return timing;
}

}  // namespace

// ---------------------------------------------------------------------------
// Python
// ---------------------------------------------------------------------------

std::string maskPython(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    const size_t n = src.size();
    size_t i = 0;
    while (i < n) {
        const char c = src[i];
        if (c == '#') {
            while (i < n && src[i] != '\n') { out += ' '; i++; }
            continue;
        }
        if (c == '"' || c == '\'') {
            const bool triple = i + 2 < n && src[i + 1] == c && src[i + 2] == c;
            const size_t q = triple ? 3 : 1;
            out.append(q, ' ');
            i += q;
            while (i < n) {
                if (src[i] == '\\') {
                    out += ' ';
                    if (i + 1 < n) out += src[i + 1] == '\n' ? '\n' : ' ';
                    i += 2;
                    continue;
                }
                if (src[i] == c && (!triple || (i + 2 < n && src[i + 1] == c && src[i + 2] == c))) {
                    out.append(q, ' ');
                    i += q;
                    break;
                }
                if (!triple && src[i] == '\n') break;  // unterminated single-line string
                out += src[i] == '\n' ? '\n' : ' ';
                i++;
            }
            continue;
        }
        out += c;
        i++;
    }
    return out;
}

std::vector<Import> parsePython(const std::string& src) {
    const std::string text = maskPython(src);
    const std::vector<std::string> lines = split(src, '\n');
    const std::vector<When> timing = pythonTiming(text, src);
    std::vector<Import> imports;

    size_t start = 0;
    for (int lineNo = 1;; lineNo++) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        const size_t p = skipBlanks(text, start, end);

        if (wordAt(text, p, "import") && p + 6 < end && isBlank(text[p + 6])) {
            // import a.b.c, d as e
            size_t q = skipBlanks(text, p + 6, end);
            for (;;) {
                const size_t s = q;
                while (q < end && isDottedChar(text[q])) q++;
                if (q == s) break;
                Import imp;
                imp.spec = text.substr(s, q - s);
                imp.when = timing[lineNo - 1];
                imp.line = lineNo;
                imp.text = lineText(lines, lineNo);
                imports.push_back(imp);

                size_t r = skipBlanks(text, q, end);
                if (r > q && wordAt(text, r, "as")) {
                    const size_t u = skipBlanks(text, r + 2, end);
                    size_t v = u;
                    while (v < end && isWordChar(text[v])) v++;
                    if (u > r + 2 && v > u) q = v;
                }
                r = skipBlanks(text, q, end);
                if (r < end && text[r] == ',') {
                    q = skipBlanks(text, r + 1, end);
                    continue;
                }
                break;
            }
        } else if (wordAt(text, p, "from")) {
            // from .a.b import (x, y)   /   from . import x
            const size_t afterFrom = skipBlanks(text, p + 4, end);
            size_t d = afterFrom;
            while (d < end && text[d] == '.') d++;
            size_t s = d;
            while (s < end && isDottedChar(text[s])) s++;
            const size_t kw = skipBlanks(text, s, end);
            const size_t body = kw + 6 < end ? skipBlanks(text, kw + 6, end) : end;
            if (afterFrom > p + 4 && kw > s && wordAt(text, kw, "import") && body > kw + 6) {
                Import imp;
                imp.kind = ImportKind::From;
                imp.level = static_cast<int>(d - afterFrom);
                imp.spec = text.substr(d, s - d);
                imp.when = timing[lineNo - 1];
                imp.line = lineNo;
                imp.text = lineText(lines, lineNo);
                const size_t close = text[body] == '(' ? text.find(')', body) : std::string::npos;
                imp.names = importedNames(close != std::string::npos
                                              ? text.substr(body, close - body + 1)
                                              : text.substr(body, end - body));
                imports.push_back(imp);
            }
        }

        if (end == text.size()) break;
        start = end + 1;
    }
    return imports;
}

// ---------------------------------------------------------------------------
// JavaScript / TypeScript
// ---------------------------------------------------------------------------

// Remove comments (keeps newlines and length). With blankStrings, string
// contents are blanked too, which tells us whether a keyword sits in real code.
std::string maskJs(const std::string& src, bool blankStrings) {
    std::string out;
    out.reserve(src.size());
    const size_t n = src.size();
    size_t i = 0;
    while (i < n) {
        const char c = src[i];
        const char d = i + 1 < n ? src[i + 1] : '\0';
        if (c == '/' && d == '/') {
            while (i < n && src[i] != '\n') { out += ' '; i++; }
            continue;
        }
        if (c == '/' && d == '*') {
            out += "  ";
            i += 2;
            while (i < n && !(src[i] == '*' && i + 1 < n && src[i + 1] == '/')) {
                out += src[i] == '\n' ? '\n' : ' ';
                i++;
            }
            if (i < n) { out += "  "; i += 2; }
            continue;
        }
        if (c == '"' || c == '\'' || c == '`') {
            out += c;
            i++;
            while (i < n && src[i] != c) {
                if (src[i] == '\\') {
                    if (blankStrings) {
                        out += ' ';
                        if (i + 1 < n) out += src[i + 1] == '\n' ? '\n' : ' ';
                    } else {
                        out += src[i];
                        if (i + 1 < n) out += src[i + 1];
                    }
                    i += 2;
                    continue;
                }
                if (src[i] == '\n' && c != '`') break;
                out += blankStrings && src[i] != '\n' ? ' ' : src[i];
                i++;
            }
            if (i < n) { out += src[i]; i++; }
            continue;
        }
        out += c;
        i++;
    }
    return out;
}

namespace {

struct Span {
    size_t start = 0;
    size_t length = 0;
};

// 'spec' or "spec" starting at a quote. Returns the end offset (after the
// closing quote), or npos.
size_t readQuoted(const std::string& t, size_t q, Span& spec) {
    if (q >= t.size() || !isQuote(t[q])) return std::string::npos;
    size_t r = q + 1;
    while (r < t.size() && !isQuote(t[r]) && t[r] != '\n') r++;
    if (r == q + 1 || r >= t.size() || t[r] == '\n') return std::string::npos;
    spec = {q + 1, r - q - 1};
    return r + 1;
}

bool inClause(char c) {
    return isWordChar(c) || isSpace(c) || c == '$' || c == '*' || c == '{' || c == '}' || c == ',';
}

// import x, { y } from 'spec'   /   import 'spec'   /   import type { T } from 'spec'
bool matchStaticImport(const std::string& t, size_t k, Span& spec) {
    const size_t q = skipSpaces(t, k + 6);
    if (q == k + 6 || q >= t.size()) return false;
    if (isQuote(t[q])) return readQuoted(t, q, spec) != std::string::npos;
    if (wordAt(t, q, "type")) {
        const size_t after = skipSpaces(t, q + 4);
        if (after > q + 4 && after < t.size() && isQuote(t[after]))
            return readQuoted(t, after, spec) != std::string::npos;
    }
    size_t j = q;
    while (j < t.size() && inClause(t[j])) j++;
    if (j >= t.size() || !isQuote(t[j])) return false;
    size_t e = j;
    while (e > q && isSpace(t[e - 1])) e--;
    if (e == j || e < q + 4 || !wordAt(t, e - 4, "from")) return false;
    const size_t f = e - 4;
    if (f < q + 2 || !isSpace(t[f - 1])) return false;
    return readQuoted(t, j, spec) != std::string::npos;
}

// export * from 'spec'   /   export * as ns from 'spec'   /   export { a } from 'spec'
bool matchExportFrom(const std::string& t, size_t k, Span& spec) {
    size_t q = skipSpaces(t, k + 6);
    if (q == k + 6 || q >= t.size()) return false;
    if (wordAt(t, q, "type")) {
        const size_t after = skipSpaces(t, q + 4);
        if (after > q + 4) q = after;
    }
    size_t r;
    if (t[q] == '*') {
        r = q + 1;
        const size_t a = skipSpaces(t, r);
        if (a > r && wordAt(t, a, "as")) {
            const size_t b = skipSpaces(t, a + 2);
            size_t c = b;
            while (c < t.size() && (isWordChar(t[c]) || t[c] == '$')) c++;
            if (b > a + 2 && c > b) r = c;
        }
    } else if (t[q] == '{') {
        const size_t close = t.find('}', q);
        if (close == std::string::npos) return false;
        r = close + 1;
    } else {
        return false;
    }
    const size_t s = skipSpaces(t, r);
    if (s == r || !wordAt(t, s, "from")) return false;
    const size_t f = skipSpaces(t, s + 4);
    if (f == s + 4) return false;
    return readQuoted(t, f, spec) != std::string::npos;
}

// require('spec')   /   import('spec')
bool matchCall(const std::string& t, size_t k, size_t keywordLength, Span& spec) {
    size_t q = skipSpaces(t, k + keywordLength);
    if (q >= t.size() || t[q] != '(') return false;
    q = skipSpaces(t, q + 1);
    const size_t end = readQuoted(t, q, spec);
    if (end == std::string::npos) return false;
    const size_t r = skipSpaces(t, end);
    return r < t.size() && t[r] == ')';
}

}  // namespace

std::vector<Import> parseJs(const std::string& src) {
    const std::string text = maskJs(src, false);  // comments removed
    const std::string code = maskJs(src, true);   // comments and string contents removed
    const std::vector<std::string> lines = split(src, '\n');
    const LineIndex index(text);

    struct Hit {
        int line;
        int pattern;
        Span spec;
        bool typeOnly;  // TypeScript "import type" / "export type"
    };
    std::vector<Hit> hits;

    // keyword must be real code, not text inside a string literal
    auto keyword = [&](size_t k, const char* word) {
        return wordAt(text, k, word) && wordAt(code, k, word);
    };
    // "import type {A} from" and "export type {A} from", but not "import type from" (a default import named type)
    auto typeOnly = [&](size_t k, size_t keywordLength) {
        const size_t t = skipSpaces(text, k + keywordLength);
        if (!wordAt(text, t, "type") || t + 4 >= text.size() || !isSpace(text[t + 4])) return false;
        return !wordAt(text, skipSpaces(text, t + 4), "from");
    };
    auto add = [&](int pattern, const Span& spec, bool typeOnly) {
        hits.push_back({index.lineAt(spec.start), pattern, spec, typeOnly});
    };

    for (size_t k = 0; k < text.size(); k++) {
        if (k > 0 && isWordChar(text[k - 1])) continue;  // word boundary
        Span spec;
        if (keyword(k, "import")) {
            if (matchStaticImport(text, k, spec)) add(0, spec, typeOnly(k, 6));
            else if (matchCall(text, k, 6, spec)) add(3, spec, false);
        } else if (keyword(k, "export")) {
            if (matchExportFrom(text, k, spec)) add(1, spec, typeOnly(k, 6));
        } else if (keyword(k, "require")) {
            if (matchCall(text, k, 7, spec)) add(2, spec, false);
        }
    }

    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        if (a.line != b.line) return a.line < b.line;
        if (a.pattern != b.pattern) return a.pattern < b.pattern;
        return a.spec.start < b.spec.start;
    });

    std::vector<Import> imports;
    HashSet<std::uint64_t> seen;
    for (const Hit& h : hits) {
        if (!seen.insert(h.spec.start)) continue;
        Import imp;
        imp.spec = text.substr(h.spec.start, h.spec.length);
        imp.when = h.pattern == 3 ? When::Deferred : h.typeOnly ? When::TypeChecking : When::Load;
        imp.line = h.line;
        imp.text = lineText(lines, h.line);
        imports.push_back(imp);
    }
    return imports;
}

// ---------------------------------------------------------------------------
// C / C++
// ---------------------------------------------------------------------------

// #include "x.h"  /  #  include <vector>
// C comments and strings look like JavaScript's, so the same masking works.
std::vector<Import> parseCpp(const std::string& src) {
    const std::string text = maskJs(src, false);  // comments removed
    const std::string code = maskJs(src, true);   // and string contents removed
    const std::vector<std::string> lines = split(src, '\n');
    std::vector<Import> imports;

    size_t start = 0;
    for (int lineNo = 1;; lineNo++) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        const size_t hash = skipBlanks(text, start, end);
        if (hash < end && text[hash] == '#' && code[hash] == '#') {
            const size_t kw = skipBlanks(text, hash + 1, end);
            if (wordAt(text, kw, "include") && (kw + 7 >= end || !isWordChar(text[kw + 7]))) {
                const size_t open = skipBlanks(text, kw + 7, end);
                const char close = open < end && text[open] == '"' ? '"' : open < end && text[open] == '<' ? '>' : 0;
                const size_t stop = close ? text.find(close, open + 1) : std::string::npos;
                if (stop != std::string::npos && stop < end && stop > open + 1) {
                    Import imp;
                    imp.kind = close == '"' ? ImportKind::Include : ImportKind::SystemInclude;
                    imp.spec = text.substr(open + 1, stop - open - 1);
                    imp.line = lineNo;
                    imp.text = lineText(lines, lineNo);
                    imports.push_back(imp);
                }
            }
        }
        if (end == text.size()) break;
        start = end + 1;
    }
    return imports;
}

std::vector<Import> parseImports(const std::string& path, const std::string& src) {
    switch (langOf(path)) {
        case Lang::Python: return parsePython(src);
        case Lang::JavaScript: return parseJs(src);
        case Lang::Cpp: return parseCpp(src);
        default: return {};
    }
}

}  // namespace pdr

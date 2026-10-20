#include "json_report.h"

#include <cstdio>
#include <vector>

namespace pdr {

namespace {

std::string str(const std::string& s) { return "\"" + jsonEscape(s) + "\""; }

std::string strList(const std::vector<std::string>& items) {
    std::string out = "[";
    for (size_t i = 0; i < items.size(); i++) out += (i ? ", " : "") + str(items[i]);
    return out + "]";
}

std::string steps(const std::vector<Step>& list) {
    std::string out = "[";
    for (size_t i = 0; i < list.size(); i++) {
        const Step& s = list[i];
        out += std::string(i ? ", " : "") + "{\"from\": " + str(s.from) + ", \"to\": " + str(s.to) +
               ", \"line\": " + std::to_string(s.line) + ", \"text\": " + str(s.text) + "}";
    }
    return out + "]";
}

// Writes "[\n    item,\n    item\n  ]" for a list of pre-rendered items.
void array(std::ostream& out, const std::vector<std::string>& items) {
    if (items.empty()) {
        out << "[]";
        return;
    }
    out << "[\n";
    for (size_t i = 0; i < items.size(); i++) out << "    " << items[i] << (i + 1 < items.size() ? ",\n" : "\n");
    out << "  ]";
}

}  // namespace

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

void writeJson(std::ostream& out, const Result& r, const std::string& folder) {
    const Stats& s = r.stats;
    out << "{\n"
        << "  \"project\": " << str(folder) << ",\n"
        << "  \"stats\": {\"files\": " << s.files << ", \"pythonFiles\": " << s.pythonFiles
        << ", \"jsFiles\": " << s.jsFiles << ", \"cppFiles\": " << s.cppFiles << ", \"importStatements\": " << s.importStatements
        << ", \"deferredImports\": " << s.deferredImports << ", \"typeCheckingImports\": " << s.typeCheckingImports
        << ", \"edges\": " << s.edges << ", \"cycles\": " << s.cycles << ", \"filesInCycles\": " << s.filesInCycles
        << ", \"broken\": " << s.broken << ", \"externals\": " << s.externals << ", \"thirdParty\": " << s.thirdParty
        << ", \"layers\": " << s.layers << "},\n";

    std::vector<std::string> items;
    for (const Cycle& c : r.cycles)
        items.push_back("{\"files\": " + strList(c.files) + ", \"loop\": " + strList(c.loop) +
                        ", \"steps\": " + steps(c.steps) +
                        ", \"fix\": " + (c.fixTooLarge ? std::string("null") : steps(c.fix)) + "}");
    out << "  \"cycles\": ";
    array(out, items);

    items.clear();
    for (const BrokenImport& b : r.broken)
        items.push_back("{\"file\": " + str(b.file) + ", \"line\": " + std::to_string(b.line) +
                        ", \"spec\": " + str(b.spec) + ", \"text\": " + str(b.text) + "}");
    out << ",\n  \"broken\": ";
    array(out, items);

    items.clear();
    for (const OrderUnit& u : r.order)
        items.push_back("{\"layer\": " + std::to_string(u.layer) + ", \"cyclic\": " + (u.cyclic ? "true" : "false") +
                        ", \"files\": " + strList(u.files) + "}");
    out << ",\n  \"buildOrder\": ";
    array(out, items);

    items.clear();
    for (const std::string& f : r.critical) {
        const Metrics& m = r.metricsOf(f);
        items.push_back("{\"file\": " + str(f) + ", \"dependents\": " + std::to_string(m.dependents) +
                        ", \"importedBy\": " + std::to_string(m.fanIn) + "}");
    }
    out << ",\n  \"critical\": ";
    array(out, items);

    items.clear();
    for (const ExternalPackage& e : r.externals)
        items.push_back("{\"name\": " + str(e.name) + ", \"builtin\": " + (e.builtin ? "true" : "false") +
                        ", \"files\": " + strList(e.files) + "}");
    out << ",\n  \"externals\": ";
    array(out, items);

    out << ",\n  \"entryPoints\": " << strList(r.entryPoints) << "\n}\n";
}

}  // namespace pdr

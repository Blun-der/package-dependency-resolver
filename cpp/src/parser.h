// Package Dependency Resolver - import parsing
//
// Reads the import statements of one source file. Comments and string
// literals are blanked out first (newlines kept), so an "import" inside a
// docstring or a comment is never counted and line numbers stay correct.
#pragma once

#include <string>
#include <vector>

namespace pdr {

enum class ImportKind {
    Import,         // Python "import x", any JavaScript import
    From,           // Python "from x import y"
    Include,        // C/C++ #include "x.h"
    SystemInclude,  // C/C++ #include <x.h>
};

// When an import runs. Only imports that run while a module is loading can
// make a circular import fail, so cycles are searched among those alone.
enum class When {
    Load,          // top level, or inside if / try / class: runs as the module loads
    Deferred,      // inside a Python function or "if __name__ == '__main__':", or a JavaScript import()
    TypeChecking,  // Python "if TYPE_CHECKING:", TypeScript "import type": never runs
};

struct Import {
    ImportKind kind = ImportKind::Import;
    When when = When::Load;
    std::string spec;                // "a.b" in Python, "./x" or "react" in JS, "x.h" in C
    std::vector<std::string> names;  // names after "from a import ..." (Python only)
    int level = 0;                   // leading dots of a relative Python import
    int line = 0;                    // 1-based line number
    std::string text;                // the source line, trimmed
};

std::string maskPython(const std::string& src);
std::string maskJs(const std::string& src, bool blankStrings);

std::vector<Import> parsePython(const std::string& src);
std::vector<Import> parseJs(const std::string& src);
std::vector<Import> parseCpp(const std::string& src);
std::vector<Import> parseImports(const std::string& path, const std::string& src);

}  // namespace pdr

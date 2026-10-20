// Package Dependency Resolver - Graphviz export
//
//   pdr <folder> --dot > deps.dot
//   dot -Tsvg deps.dot -o deps.svg
//
// Files are grouped in a box per folder. Files and imports that form a loop
// are red; the imports the cycle fix suggests removing are dashed; imports
// that do not run while the module loads (inside a function, type-checking
// only) are dotted; imports that break pdr.rules are orange; unused files are
// gray with a dashed outline; broken imports point to a dashed "missing" note.
#pragma once

#include <ostream>

#include "analyzer.h"

namespace pdr {

void writeDot(std::ostream& out, const Result& result);

}  // namespace pdr

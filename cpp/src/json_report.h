// Package Dependency Resolver - JSON output for scripts and CI
#pragma once

#include <ostream>
#include <string>

#include "analyzer.h"

namespace pdr {

std::string jsonEscape(const std::string& s);
void writeJson(std::ostream& out, const Result& result, const std::string& folder);

}  // namespace pdr

// Package Dependency Resolver - path and text helpers
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace pdr {

enum class Lang { None, Python, JavaScript, Cpp };

extern const std::vector<std::string> JS_EXTENSIONS;
extern const std::vector<std::string> C_EXTENSIONS;

Lang langOf(const std::string& path);         // from the file extension
const char* langName(Lang lang);
bool isIgnored(const std::string& path);      // inside node_modules, venv, .git ...
bool isBuiltin(Lang lang, const std::string& name);  // standard library / Node built-in

std::string dirOf(const std::string& path);   // "a/b/c.py" -> "a/b"
std::string baseName(const std::string& path);  // "a/b/c.py" -> "c.py"
std::string joinPath(const std::string& a, const std::string& b);
// "a/./b/../c" -> "a/c". Empty optional if the path climbs above the root.
std::optional<std::string> normalize(const std::string& path);

std::vector<std::string> split(const std::string& s, char sep);
std::string join(const std::vector<std::string>& parts, char sep, size_t count);
std::string trim(const std::string& s);
std::string toLower(std::string s);
bool startsWith(const std::string& s, const std::string& prefix);
bool endsWith(const std::string& s, const std::string& suffix);
bool isWordChar(char c);   // [A-Za-z0-9_]
bool isSpace(char c);

}  // namespace pdr

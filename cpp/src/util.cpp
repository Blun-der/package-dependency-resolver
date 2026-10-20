#include "util.h"

#include <cctype>

#include "hash_map.h"

namespace pdr {

const std::vector<std::string> JS_EXTENSIONS = {".js", ".jsx", ".ts", ".tsx", ".mjs", ".cjs"};
const std::vector<std::string> C_EXTENSIONS = {".c", ".h", ".cc", ".cpp", ".cxx", ".c++",
                                               ".hh", ".hpp", ".hxx", ".h++", ".ipp", ".inl", ".tpp"};

namespace {

HashSet<std::string> wordSet(const std::string& words) {
    HashSet<std::string> out;
    for (const std::string& w : split(words, ' '))
        if (!w.empty()) out.insert(w);
    return out;
}

const HashSet<std::string>& ignoredDirs() {
    static const auto set = wordSet(
        "node_modules .git __pycache__ venv .venv env .env dist build .next coverage .idea .vscode "
        "site-packages .mypy_cache .pytest_cache .tox out .cache egg-info");
    return set;
}

// Built-in modules, so externals can be split into "standard library"
// and "third-party packages you must install".
const HashSet<std::string>& pythonStdlib() {
    static const auto set = wordSet(
        "__future__ abc argparse array ast asyncio atexit base64 bisect builtins bz2 calendar cmath cmd "
        "codecs collections colorsys concurrent configparser contextlib contextvars copy copyreg csv ctypes "
        "curses dataclasses datetime dbm decimal difflib dis doctest email encodings enum errno faulthandler "
        "fcntl filecmp fileinput fnmatch fractions ftplib functools gc getopt getpass gettext glob graphlib "
        "grp gzip hashlib heapq hmac html http imaplib importlib inspect io ipaddress itertools json keyword "
        "linecache locale logging lzma mailbox marshal math mimetypes mmap multiprocessing netrc numbers "
        "operator optparse os pathlib pdb pickle pkgutil platform plistlib poplib posix pprint profile pstats "
        "pty pwd py_compile queue quopri random re readline reprlib resource rlcompleter runpy sched secrets "
        "select selectors shelve shlex shutil signal site smtplib socket socketserver sqlite3 ssl stat "
        "statistics string stringprep struct subprocess symtable sys sysconfig syslog tabnanny tarfile "
        "tempfile termios textwrap threading time timeit tkinter token tokenize tomllib trace traceback "
        "tracemalloc tty turtle types typing unicodedata unittest urllib uuid venv warnings wave weakref "
        "webbrowser winreg winsound wsgiref xml xmlrpc zipapp zipfile zipimport zlib zoneinfo _thread");
    return set;
}

const HashSet<std::string>& nodeBuiltins() {
    static const auto set = wordSet(
        "assert async_hooks buffer child_process cluster console constants crypto dgram diagnostics_channel "
        "dns domain events fs http http2 https inspector module net os path perf_hooks process punycode "
        "querystring readline repl stream string_decoder sys timers tls trace_events tty url util v8 vm "
        "wasi worker_threads zlib test");
    return set;
}

// C and C++ standard headers, plus the common operating system ones.
// <sys/stat.h> is looked up as "sys", <stdio.h> as "stdio.h", <vector> as "vector".
const HashSet<std::string>& cStandardHeaders() {
    static const auto set = wordSet(
        "algorithm any array atomic barrier bit bitset charconv chrono codecvt compare complex concepts "
        "condition_variable coroutine deque exception execution expected filesystem flat_map flat_set format "
        "forward_list fstream functional future generator initializer_list iomanip ios iosfwd iostream istream "
        "iterator latch limits list locale map mdspan memory memory_resource mutex new numbers numeric optional "
        "ostream print queue random ranges ratio regex scoped_allocator semaphore set shared_mutex "
        "source_location span spanstream sstream stack stacktrace stdexcept stdfloat stop_token streambuf "
        "string string_view syncstream system_error text_encoding thread tuple type_traits typeindex typeinfo "
        "unordered_map unordered_set utility valarray variant vector version "
        "cassert cctype cerrno cfenv cfloat cinttypes climits clocale cmath csetjmp csignal cstdarg cstddef "
        "cstdint cstdio cstdlib cstring ctime cuchar cwchar cwctype "
        "assert.h complex.h ctype.h errno.h fenv.h float.h inttypes.h iso646.h limits.h locale.h math.h "
        "setjmp.h signal.h stdalign.h stdarg.h stdatomic.h stdbool.h stddef.h stdint.h stdio.h stdlib.h "
        "stdnoreturn.h string.h tgmath.h threads.h time.h uchar.h wchar.h wctype.h "
        "unistd.h fcntl.h pthread.h dirent.h dlfcn.h poll.h sched.h semaphore.h termios.h netdb.h strings.h "
        "syslog.h glob.h regex.h pwd.h grp.h spawn.h libgen.h getopt.h malloc.h sys netinet arpa linux "
        "windows.h winsock2.h ws2tcpip.h io.h conio.h direct.h process.h");
    return set;
}

}  // namespace

Lang langOf(const std::string& path) {
    size_t dot = path.find_last_of("./");
    if (dot == std::string::npos || path[dot] != '.' || dot + 1 == path.size()) return Lang::None;
    std::string ext = toLower(path.substr(dot));
    if (ext == ".py") return Lang::Python;
    for (const std::string& e : JS_EXTENSIONS)
        if (ext == e) return Lang::JavaScript;
    for (const std::string& e : C_EXTENSIONS)
        if (ext == e) return Lang::Cpp;
    return Lang::None;
}

const char* langName(Lang lang) {
    switch (lang) {
        case Lang::Python: return "Python";
        case Lang::JavaScript: return "JavaScript";
        case Lang::Cpp: return "C/C++";
        default: return "unknown";
    }
}

bool isIgnored(const std::string& path) {
    for (const std::string& part : split(path, '/'))
        if (ignoredDirs().contains(part) || endsWith(part, ".egg-info")) return true;
    return false;
}

bool isBuiltin(Lang lang, const std::string& name) {
    if (lang == Lang::Python) return pythonStdlib().contains(name);
    if (lang == Lang::Cpp) return cStandardHeaders().contains(name);
    std::string bare = startsWith(name, "node:") ? name.substr(5) : name;
    return nodeBuiltins().contains(bare.substr(0, bare.find('/')));
}

std::string dirOf(const std::string& path) {
    size_t i = path.rfind('/');
    return i == std::string::npos ? "" : path.substr(0, i);
}

std::string baseName(const std::string& path) {
    size_t i = path.rfind('/');
    return i == std::string::npos ? path : path.substr(i + 1);
}

std::string joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    return b.empty() ? a : a + "/" + b;
}

std::optional<std::string> normalize(const std::string& path) {
    std::vector<std::string> out;  // used as a stack of folder names
    for (const std::string& part : split(path, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") {
            if (out.empty()) return std::nullopt;
            out.pop_back();
        } else {
            out.push_back(part);
        }
    }
    return join(out, '/', out.size());
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> parts;
    size_t start = 0;
    for (;;) {
        size_t i = s.find(sep, start);
        if (i == std::string::npos) {
            parts.push_back(s.substr(start));
            return parts;
        }
        parts.push_back(s.substr(start, i - start));
        start = i + 1;
    }
}

std::string join(const std::vector<std::string>& parts, char sep, size_t count) {
    std::string out;
    for (size_t i = 0; i < count && i < parts.size(); i++) {
        if (i) out += sep;
        out += parts[i];
    }
    return out;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isSpace(s[a])) a++;
    while (b > a && isSpace(s[b - 1])) b--;
    return s.substr(a, b - a);
}

std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isWordChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

}  // namespace pdr

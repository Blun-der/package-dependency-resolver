# Package Dependency Resolver

A tool that reads a code project and shows how its files depend on each other.
Open a Python or JavaScript/TypeScript project folder and it will:

- draw the **dependency graph** (which file imports which)
- find **circular imports** and show the exact lines that cause them
- find **broken imports** (imports that point to files that do not exist)
- give a **safe build order** (every file comes after the files it needs)
- rank the **most critical files** (how many files break if one changes)
- answer **"why does file A need file B?"** with the shortest chain of imports
- list **outside packages**, split into "must be installed" and "built into the language"

Everything runs in the browser. Your code is never uploaded.

## C++ version (course submission)

`cpp/` holds the same engine in C++17, using only the standard library. The graph, hash table, min-heap,
tries and bitsets are written by hand. It started as a port of the JavaScript engine and adds things the web
version does not have:

- **C/C++ projects**: `#include "x.h"` and `#include <x.h>` are read, so the tool can analyse its own source
- **only real circular imports**: imports inside functions, under `if TYPE_CHECKING:` or under
  `if __name__ == "__main__":` cannot cause a circular import error, so they are left out of cycle detection
- **cycle fix suggestions**: which imports to remove so a circular import disappears
- **architecture rules**: a `pdr.rules` file of layers and forbidden imports that `--check` enforces
- **unused files**: files that no script, test or entry point reaches
- **faster critical-files ranking** with bitsets
- **graph images**: `--dot` writes the graph for Graphviz
- **a benchmark**: `make bench` measures the algorithms on generated projects of up to a million files

```
cd cpp
make                                   # build/pdr
make test                              # 28 engine tests + 29 command-line tests
make bench                             # timings -> build/bench.csv and charts (about a minute)
make dist                              # the zip to hand in -> build/v5-package-dependency-resolver.zip
make graph                             # draw the sample and this program as SVG (needs graphviz)
make install                           # copy pdr to ~/.local/bin, so it runs from any folder
```

Then `cd` into any project and run `pdr`. Without a folder it analyses the current folder, and files
can be named by file name alone (`main.py`) or by the end of their path (`utils/paths.py`):

```
pdr                                    # interactive menu
pdr --report
pdr --why main.py paths.py
pdr --impact config.py
pdr --search ru
pdr --check                            # for CI, see below
pdr --json > deps.json
pdr --dot | dot -Tsvg -o graph.svg
pdr --unused                           # files nothing reaches
pdr --check --rules my.rules           # rules from another file (default: pdr.rules here)
pdr ~/some/other/project --check       # or name a folder
pdr --help
```

Without installing, use `./build/pdr` from `cpp/` instead of `pdr`. `make uninstall` removes it.

### Terminal and CI use

`--check` prints one line per problem in `file:line: message` form, the format CI logs and editors
turn into links, and sets the exit code:

| Exit code | Meaning |
|---|---|
| 0 | no circular or broken imports, and no rule violations |
| 1 | `--check` found problems |
| 2 | bad usage, or the folder cannot be read / has no code |

```
$ cd sample/fad_demo && pdr --check
fad/actions/__init__.py:2: error: circular import: fad/actions/__init__.py -> fad/actions/move.py -> fad/rules.py -> fad/actions/__init__.py
    fad/actions/__init__.py:2:  from .move import move_file
    fad/actions/move.py:4:  from ..rules import Rule
    fad/rules.py:4:  from .actions import run_action
    fix: remove fad/rules.py:4:  from .actions import run_action
fad/actions/archive.py:3: error: broken import '.compress' (no such file in the project)
pdr: 17 files, 1 circular import, 1 broken import -> FAILED
```

`--allow-cycles` or `--allow-broken` turns off one of the two rules, for a project that cannot fix
everything at once. `--json` prints the whole analysis (stats, cycles, broken imports, build order,
critical files, outside packages, suggested fixes, unused files, rule violations) for other tools or as a saved CI artifact.

This repository's own GitHub Actions workflow, `.github/workflows/check.yml`, runs `make -C cpp test` on
every push, which includes `pdr --check` on the program's own source against `cpp/pdr.rules`. In another
project the step is:

```yaml
- name: Check imports
  run: pdr --check
```

Without `make` (Windows / Code::Blocks): `g++ -std=c++17 -O2 -o pdr cpp/src/*.cpp`

| File | Contents |
|---|---|
| `cpp/src/util.*` | file types, ignored folders, built-in module lists, path helpers |
| `cpp/src/parser.*` | Python, JavaScript and C/C++ import parsing |
| `cpp/src/resolver.*` | import -> project file / outside package / broken |
| `cpp/src/graph.*` | adjacency list + reverse adjacency list |
| `cpp/src/hash_map.h` | hash table with separate chaining (template) |
| `cpp/src/min_heap.h` | binary min-heap (template) |
| `cpp/src/trie.*` | prefix search over file names and paths |
| `cpp/src/algorithms.*` | Tarjan's SCC, Kahn's topological sort, BFS, cycle fix |
| `cpp/src/analyzer.*` | loads a folder and runs the whole pipeline |
| `cpp/src/json_report.*` | `--json` output |
| `cpp/src/dot_export.*` | `--dot` output (Graphviz) |
| `cpp/src/rules.*` | reads `pdr.rules` and checks every import against it |
| `cpp/src/path_trie.h` | trie over folder names, for matching rule paths |
| `cpp/pdr.rules` | this program's own architecture, checked by `make test` |
| `cpp/src/main.cpp` | console menu and command-line options |
| `cpp/tests/engine_tests.cpp` | the 13 tests, ported, plus 15 for the C++-only features |
| `cpp/tests/cli_test.sh` | command-line tests: exit codes, `--check`, `--json`, `--dot` (run by `make test`) |
| `cpp/bench/bench.cpp` | benchmark on generated projects (`make bench`) |
| `cpp/bench/chart.h` | draws the benchmark charts as SVG, in C++ |
| `cpp/packaging/README.md` | the README of the zip made by `make dist` (C++ program only) |

### Hand-written hash table

`HashMap<K, V>` in `hash_map.h` replaces `std::unordered_map` and `std::unordered_set` everywhere in the
engine. Each bucket is a singly linked list of the entries whose hash lands there (separate chaining).
Strings are hashed with FNV-1a and integer keys with the splitmix64 finaliser. The table doubles once it
is more than 75% full, so chains stay short: insert, find and erase are O(1) on average and O(n) only if
every key collides. One test puts every key in the same bucket to check the chaining works.

### Which imports count for circular imports

Python runs a module's code from top to bottom the first time it is imported. If `a.py` imports `b.py`
while it is still loading and `b.py` imports something from `a.py`, `b.py` finds `a.py` half-loaded:

```
ImportError: cannot import name 'Rule' from partially initialized module 'fad.rules'
(most likely due to a circular import)
```

Only imports that run *while a module is loading* can do this, so each import is tagged with when it runs:

| Where the import is | When it runs | Counted for cycles |
|---|---|---|
| top level, or inside `if` / `try` / `with` / `class` | while the module loads | yes |
| inside a `def` (a function or method) | when the function is called, after loading | no |
| inside `if __name__ == "__main__":` | only when the file is started as a script | no |
| inside `if TYPE_CHECKING:`, TypeScript `import type` | never (only type checkers read it) | no |
| JavaScript `import('x')` | later, when that line runs | no |

The enclosing blocks of an import are found from indentation: the nearest earlier statement with less
indentation is the block it sits in (lines that continue a bracket or end in `\` are skipped).

Two graphs are kept with the same nodes. **Every import** drives `--why`, `--impact` and the critical-files
ranking, because a file still depends on what it imports inside a function. **Load-time imports** drive
Tarjan's algorithm, the cycle fix and the build order. In `--dot` output the other imports are dotted.

Tested on five well-known Python projects (shallow clones, October 2026):

| Project | Files | Imports inside functions / type-checking only | Cycles, all imports | Cycles, load-time only |
|---|---|---|---|---|
| requests | 37 | 9 / 34 | 1 | 0 |
| httpx | 60 | 23 / 19 | 2 | 0 |
| rich | 213 | 64 / 41 | 2 | 0 |
| flask | 83 | 52 / 49 | 2 | 1, in the `js_example` example app |
| click | 91 | 73 / 24 | 1 | 1, `_compat.py` ↔ `_winconsole.py`, only on Windows |

These libraries work, so counting every import reports loops that cannot fail. The two left are real
load-time loops that work only because of the order of the lines, the situation the tool is meant to
flag. Flask's example is the well-known Flask pattern: `__init__.py` creates `app` and then imports
`views`, which imports `app` back. Moving that import above `app = Flask(__name__)` breaks it.

### Architecture rules

Teams decide which parts of a program may use which: the screens use the services, the services use the
database, never the other way round. `pdr.rules` writes that down, and `--check` fails when an import breaks it:

```
# top layer first: a layer may import the layers below it, never one above
layers  fad/main  fad/daemon  fad/scheduler|fad/watcher  fad/rules  fad/actions  fad/events|fad/logger  fad/utils|fad/config
forbid  fad/utils -> fad/actions      # nothing under fad/utils may import fad/actions
entry   scripts                       # more start points for --unused
```

- Paths are folders or files of the project; `fad/utils` covers the folder `fad/utils/` and the file
  `fad/utils.py`. Paths joined with `|` share a layer. When paths are nested (`app` and `app/core`), the
  deepest one decides a file's layer.
- `pdr.rules` in the project folder is read automatically; `--rules FILE` reads another file.
- A rule path that matches no file is a warning (`pdr.rules:4: warning: 'fad/utilz' matches no file`), which
  catches typos. A line the tool does not understand stops it with exit code 2 and the line number.
- Every rule path is stored in a trie with one node per folder name. Walking a file's path down it finds
  every rule that applies in O(depth of the path), so each import link is then checked in constant time
  per rule that applies.

`sample/fad_demo.rules` describes the sample daemon's design:

```
$ pdr sample/fad_demo --check --rules sample/fad_demo.rules
...
fad/actions/move.py:4: error: fad/actions must not import fad/rules, a layer above it (sample/fad_demo.rules:7)
    fad/actions/move.py:4:  from ..rules import Rule
```

The rule points at a different import than the cycle fix. The cycle fix removes as few links as possible
(`rules.py:4`); the rule knows which way the design says the dependency should go (`move.py:4`). Both
are right answers to different questions, and `--dot` draws both (dashed red "remove", orange "breaks rule").

`cpp/pdr.rules` holds this program's own layers (command line, reports, analysis, algorithms, data
structures, helpers), and `make test` checks them, so the tool guards its own design.

### Unused files

`--unused` lists files that nothing reaches. It runs one BFS from every start file at once, following
every import (also the ones inside functions). Start files are the files that run on their own:

| Language | Start files |
|---|---|
| Python | scripts (a file in a folder without `__init__.py`), files with `if __name__ == "__main__":`, `__main__.py`, `setup.py`, `conftest.py`, `test_*.py`, files in `tests/`, `examples/`, `benchmarks/`, `scripts/`, `docs/`, `tools/`, `bin/` |
| JavaScript / TypeScript | `*.test.*`, `*.spec.*`, `*.config.*`, `*.d.ts`, files in `__tests__/`, `tests/`, `pages/`, `scripts/`, `bin/`, `examples/`, `benchmarks/`, Next.js `page` / `layout` / `route` files, `index` / `main` / `app` / `server` / `cli` at the top or in `src/` |
| C / C++ | every `.c` / `.cpp` / `.cc` / `.cxx` file (each is compiled by itself), so what is left are headers nobody includes |
| any | paths named by `entry` in `pdr.rules` |

Importing `a.b.c` also runs `a/__init__.py` and `a/b/__init__.py`, so reaching a module reaches its
packages too. On the five projects above it reports nothing for requests, flask, httpx and click, and 21
files for rich: `rich/_unicode_data/unicode*.py`, which rich loads with
`import_module(f".unicode{version}")`, a name built at runtime that no static tool can see. That is why the
list says "check before deleting", and why `entry rich/_unicode_data` exists.

### Critical files with bitsets

"How many files break if this file changes?" is the number of files that reach it. One BFS per file
answers it in O(V · (V + E)). The C++ version gets the same numbers 22 to 33 times faster:

1. Tarjan's algorithm on all imports gives the strongly connected components, dependencies first.
   Collapsed into one node each, they form a graph with no loops.
2. Each component gets a row of bits, one bit per file: the files in the component plus every file it
   reaches. Going through the components dependencies first, a component's row is its own files OR the
   row of every component it imports, and those rows are already finished.
3. A file's count is the number of set bits in its component's row, minus the file itself. The same pass
   in reverse order, following importers, gives the other direction.

One OR of two 64-bit words handles 64 files at once. For big projects the files are done in slices so all
rows together stay under 64 MB. A test checks the result against one BFS per file on 20 random graphs
with loops, in one slice and in slices of 64 files.

### Suggested cycle fixes

For each group of files that import each other in a loop (one strongly connected component), the tool
looks for imports to remove until no loop is left:

1. For every import link inside the group, remove it for a moment and run Tarjan's again on the group alone.
2. Keep the removal that leaves the fewest files still in a loop (ties: alphabetical, so the answer is stable).
3. Repeat until the group has no loop.

Finding the smallest possible set of links is the *minimum feedback arc set* problem, which is NP-hard,
so this greedy method gives a good plan, not always the smallest one. It costs O(k · E · (V + E)) for k
removals inside the group, so very large groups are skipped and reported as too large to search.

### C/C++ includes

- `#include "x.h"` looks next to the including file first, like a compiler does.
- Otherwise the path is matched against the end of every project file's path (`"core/graph.h"` matches
  `src/core/graph.h`). If several files match, the one sharing the most folders with the including file wins.
  This stands in for the `-I` folders, which the tool does not know.
- `<vector>`, `<sys/stat.h>` and other standard headers count as built-in; `<boost/asio.hpp>` counts as the
  outside package `boost`.
- A quoted include with no folder (`"x.h"`) or a relative one (`"../x.h"`) that is not found is **broken**.

### Graph images

`--dot` prints the graph in Graphviz format. Files are grouped in one box per folder, files and imports in a
loop are red, the imports the cycle fix suggests removing are dashed, and broken imports point to a dashed
"missing" note. On Arch: `sudo pacman -S graphviz`, then `pdr --dot | dot -Tsvg -o graph.svg`.

### Benchmark

`make bench` builds `cpp/bench/bench.cpp`, generates projects shaped like real code (packages of 10 files,
each file importing 1 to 5 others, one small circular import per 20 packages, fixed random seed), and
writes the median of several runs to `build/bench.csv`, then draws the charts `build/bench-*.svg` itself
(`bench/chart.h`; `pdr_bench --charts build/bench.csv DIR` redraws them without running again).
Results on an AMD Ryzen AI 7 350, g++ -O2:

| Files | Import links | Tarjan | Kahn + min-heap | One BFS |
|---|---|---|---|---|
| 10,000 | 28,111 | 1 ms | 4 ms | 0.2 ms |
| 100,000 | 281,291 | 24 ms | 136 ms | 4 ms |
| 1,000,000 | 2,812,718 | 323 ms | 2,080 ms | 71 ms |

- Tarjan's algorithm and BFS grow in step with V + E, as O(V + E) predicts.
- Kahn's algorithm grows a little faster: the min-heap adds the log V of O(V log V + E).
- Our `HashMap` finds keys faster than `std::unordered_map` (139 ms against 229 ms for a million present
  and a million missing keys) and inserts a little slower (482 ms against 465 ms).
- The critical-files ranking with bitsets is 22 to 33 times faster than one BFS per file:

  | Files | One BFS per file | Bitsets |
  |---|---|---|
  | 1,000 | 4.6 ms | 0.2 ms |
  | 10,000 | 298 ms | 9.8 ms |
  | 20,000 | 1,111 ms | 50 ms |
  | 100,000 | (not run) | 2,029 ms |

  Both are O(V · (V + E)) in the worst case; bitsets divide it by 64 (one machine word holds 64 files),
  and the rest of the gap to 64 is the work outside the word operations.
- The whole analysis (reading imports, resolving them, every algorithm) takes 67 ms for 11,000 files
  (366 ms with one BFS per file) and 3.5 s for 110,000 files.

Timings change from machine to machine and run to run; the shapes of the curves are what matter.

### Live demo

1. The bundled sample really fails in Python. From a copy of `sample/fad_demo` with the missing
   `fad/actions/compress.py` added (one line: `def zip_folder(*a): pass`):

   ```
   $ python3 -c "import fad.rules"
   ImportError: cannot import name 'Rule' from partially initialized module 'fad.rules'
   (most likely due to a circular import)
   ```

   Python names two files. `pdr --check` names all three, the line of each import, and the fix:
   remove `from .actions import run_action` from `fad/rules.py:4`. Moving that import into
   `RuleEngine.handle`, the only place that uses it, makes `import fad.rules` work and `pdr --check` pass.

2. A real project: `git clone --depth 1 https://github.com/pallets/flask && cd flask && pdr --check`
   finds the loop in `examples/javascript/js_example` and nothing in Flask itself.

## How to run the web version

Open `dist/index.html` in Chrome, Edge or Firefox. No installation needed.
Click **Open project folder** and pick your project (for example the File Automation Daemon folder).
A bundled file automation daemon project loads on start, so the page is never empty.

Developer commands (need Node.js 18 or newer):

```
node --test test/engine.test.js   # run the 13 tests
node build.js                     # rebuild dist/index.html after editing src/
```

## Project structure

```
src/engine.js          all parsing, data structures and algorithms (no libraries)
src/app.html           the user interface
build.js               bundles engine + UI + sample into one HTML file
dist/index.html        the finished app (open this)
test/engine.test.js    unit and end-to-end tests
sample/fad_demo/       a small file automation daemon used as the built-in example
```

## How it works

```
project folder
   │  read every .py / .js / .ts file
   ▼
parse imports ──► resolve each import to a file (hash map)
   │
   ▼
dependency graph (adjacency list + reverse adjacency list)
   │
   ├─► Tarjan's SCC ............ circular imports
   ├─► Kahn's topological sort . build order and layers
   ├─► BFS on reverse graph .... critical files / impact
   └─► BFS with parent links ... "why does A need B"
```

An edge **A → B** means "file A imports file B".

### Data structures

| Structure | Where it is used | Why |
|---|---|---|
| Hash map (`Map`) | module name → file path, file → info | O(1) lookup while resolving imports |
| Adjacency list | `graph.adj`: file → files it imports | dependency graphs are sparse, so O(V + E) space instead of O(V²) for a matrix |
| Reverse adjacency list | `graph.radj`: file → files that import it | answers "who uses this file?" in O(1) per neighbour |
| Binary min-heap | Kahn's algorithm | always picks the alphabetically first ready file, so the order is stable |
| Stack | Tarjan's algorithm (iterative, so no recursion limit on deep projects) | tracks the current DFS path |
| Queue | BFS | shortest chains and reachability |
| Trie | file search box and path finder | prefix search in O(length of what you typed) |
| Path trie (C++) | rule paths, one node per folder name | every rule containing a file in O(depth of its path) |
| Set | visited nodes, deduplication | O(1) membership tests |
| Bitset (C++) | which files each component reaches, 64 files per machine word | one OR handles 64 files |

### Algorithms and complexity (V = files, E = import links)

| Task | Algorithm | Time |
|---|---|---|
| Read imports | single pass over the text, skipping comments and strings | O(total characters) |
| Circular imports | Tarjan's strongly connected components; then BFS inside each component to show one concrete loop | O(V + E) |
| Build order | condense each cycle to one unit, then Kahn's topological sort with a min-heap | O(V log V + E) |
| Layers | layer = 1 + max layer of the files it imports, computed during Kahn's | O(V + E) |
| Impact of a change | BFS on the reverse graph | O(V + E) per file |
| Critical files ranking | bitsets over the components in Tarjan's order (C++); the BFS above for every file (web) | O(V / 64 · (V + E)) |
| Why A needs B | BFS with parent pointers gives the shortest chain | O(V + E) |
| Graph drawing | files placed in columns by layer, rows reordered by barycenter sweeps to reduce crossing lines | O(sweeps · E log V) |

### Why Tarjan before Kahn?

Kahn's algorithm only works on a graph without cycles. If files import each other in a loop, no
file in the loop can ever have "zero remaining dependencies", so Kahn's would stop early. Tarjan's
algorithm finds each loop as a strongly connected component; collapsing each component into a single
unit turns the graph into a DAG, and then Kahn's always produces a complete order. Files in a loop
are shown as one step ("loaded together").

### How imports are matched to files

**Python**
- `import a.b.c` tries `a/b/c.py`, then `a/b/c/__init__.py`, then the shorter names `a.b`, `a`.
- `from a.b import c` first checks whether `c` is a submodule (`a/b/c.py`), otherwise links to `a/b.py`.
- Relative imports (`from . import x`, `from ..pkg import y`) climb folders from the importing file.
- Searched in the importing file's own folder first, then in every source root
  (the project root, a `src/` folder that holds packages, and any folder of scripts without `__init__.py`).
  In the C++ version the own-folder search happens only for scripts: inside a package (a folder with
  `__init__.py`), `import types` is the standard library, never the package's own `types.py`.
- Anything not found is external. Known standard library names (`os`, `json`, `pathlib`…) are marked built-in.
- A relative import that is not found is reported as **broken**.

**JavaScript / TypeScript**
- `import … from`, `export … from`, `require()`, and `import()` are all detected.
- `./x` tries `x`, `x.js/.jsx/.ts/.tsx/.mjs/.cjs`, and `x/index.*`. `@/` and `~/` map to `src/`.
- Bare names (`react`, `@scope/pkg`) are external; Node built-ins (`fs`, `path`) are marked built-in.

Folders such as `node_modules`, `venv`, `.git`, `__pycache__`, `dist` and `build` are skipped.

## Tests

`test/engine.test.js` has 13 tests:

1. Python import forms, aliases, multi-line imports, and line numbers
2. JavaScript import, export-from, require and dynamic import; comments ignored
3. Import text inside string literals is ignored
4. Min-heap ordering
5. Trie prefix search
6. Tarjan's algorithm finds a 3-file cycle and the single files
7. Build order puts dependencies first and collapses cycles
8. Build order on a 5,000-file chain (proves there is no recursion limit)
9. Shortest path and reachability
10. Python resolution: packages, relative imports, `src/` layout, externals, broken imports
11. JavaScript resolution: extensions, index files, `@/` alias, scoped packages
12. Ignored folders and non-code files are skipped
13. End-to-end on the sample daemon: finds the cycle `rules.py → actions/__init__.py → move.py → rules.py`,
    the broken `.compress` import, a valid build order, and the most critical file

## Limitations

- Static analysis only: imports built at runtime (`importlib.import_module(name)`, `require(variable)`)
  cannot be seen.
- Python's implicit import of parent packages (`import a.b` also runs `a/__init__.py`) is not drawn as a
  separate edge.
- Path aliases other than `@/` and `~/` (custom `tsconfig` paths) are reported as external packages.
- The critical-files ranking is still O(V · (V + E)) in the worst case, only 64 times smaller: about
  2 s for 100,000 files, growing faster than linearly.
- Unused files depend on guessing the start files. A library's public modules, plugins found by name and
  modules loaded with `importlib` show up as unused until they are named with `entry`. `package.json`'s
  `main` and `bin` fields are not read.
- Rule paths are folders and files, not wildcards (`*/models`).
- An import inside a plain `if` counts as load time even when the condition is false on this machine
  (click's Windows-only import, above): the tool cannot know which branch runs.
- In JavaScript, a `require()` inside a function is still counted as load time; only `import()` and
  `import type` are recognised as deferred.

## Future scope

- Read `requirements.txt` / `package.json` and flag packages that are imported but not listed (or listed but unused).
- Check installed package versions against version constraints (backtracking resolver).
- Support more languages (Java, Go) and C/C++ include folders from `compile_commands.json`.
- Bring the C++-only features (C/C++ files, load-time cycles, cycle fixes, graph export) to the web version.
- Estimate the critical-files ranking for huge projects instead of counting exactly: no known exact
  method is much faster than the bitsets in general, while estimates (Cohen's size-estimation method,
  1997) run in near-linear time.

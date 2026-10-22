# Package Dependency Resolver

A command-line program, written in C++17, that reads a code project and explains how its files depend
on each other. Give it a Python, JavaScript/TypeScript or C/C++ project folder and it will:

- find **circular imports** that can fail while modules load, with the exact lines, and suggest which import to remove
- find **broken imports** (imports of files that do not exist)
- give a **safe build order** and the layer of every file
- rank the **most critical files** (how many files may break if one changes)
- answer **"why does file A need file B?"** with the shortest chain of imports
- check **architecture rules** (layers, forbidden imports) written in a `pdr.rules` file
- list **unused files** that no script, test or entry point reaches
- list **outside packages**, split into "must be installed" and "built into the language"
- print everything as text, as JSON, or as a Graphviz graph

"Dependency" here means the imports between the files of one project. Choosing versions of downloaded
packages (what pip or npm do) is not part of it.

Course: Data Structures using C++ (E0556B). The program uses only the C++ standard library; every data
structure it needs (graph, hash table, min-heap, tries, bitsets) is written by hand.

## Contents

```
cpp/src/            the program (24 files)
cpp/tests/          engine tests (engine_tests.cpp) and command-line tests (cli_test.sh)
cpp/bench/          benchmark on generated projects, draws its own SVG charts
cpp/Makefile        build, test, install, benchmark
cpp/pdr.rules       this program's own architecture, checked by its tests
sample/fad_demo/    a small file automation daemon (17 Python files) used as the example
sample/fad_demo.rules   architecture rules for the example
```

## Build and test

Needs a C++17 compiler and Make (tested with GCC 16 on Arch Linux and on Ubuntu).

```
cd cpp
make            # builds build/pdr
make test       # 31 engine tests and 30 command-line tests
make install    # copies pdr to ~/.local/bin, so "pdr" works in any folder
make demo       # full report of the sample project
make bench      # benchmark -> build/bench.csv and build/bench-*.svg (about a minute)
make graph      # draws the sample and this program as SVG (needs Graphviz)
```

Without Make (for example on Windows with MinGW):

```
g++ -std=c++17 -O2 -o pdr cpp/src/*.cpp
```

Graphviz is only needed to draw `--dot` output (`sudo pacman -S graphviz` on Arch Linux,
`sudo apt install graphviz` on Ubuntu).

## Using it

Run `pdr` inside a project folder, or give the folder as the first argument. Files can be named by
their file name (`main.py`) or by the end of their path (`utils/paths.py`).

| Command | What it does |
|---|---|
| `pdr` | interactive menu |
| `pdr --report` | the whole analysis as text |
| `pdr --check` | for CI: every circular import, broken import and rule violation; exit code 1 if any |
| `--allow-cycles`, `--allow-broken` | with `--check`: do not fail on that kind of problem |
| `pdr --why A B` | the shortest chain of imports by which A needs B |
| `pdr --impact FILE` | every file that may break if FILE changes |
| `pdr --search TEXT` | files whose name or path starts with TEXT |
| `pdr --unused` | files that no script, test or entry point reaches |
| `pdr --json` | the whole analysis as JSON |
| `pdr --dot` | the graph for Graphviz: `pdr --dot \| dot -Tsvg -o graph.svg` |
| `--rules FILE` | read architecture rules from FILE instead of `pdr.rules` in the project |
| `pdr --help` | help |

Exit codes: 0 no problems, 1 `--check` found problems, 2 bad usage or a folder that cannot be read or
has no code. Colours appear only on a terminal, never in pipes or CI logs; `NO_COLOR=1` turns them off.

## Try it on the sample

```
$ cd sample/fad_demo
$ pdr --check
fad/actions/__init__.py:2: error: circular import: fad/actions/__init__.py -> fad/actions/move.py -> fad/rules.py -> fad/actions/__init__.py
    fad/actions/__init__.py:2:  from .move import move_file
    fad/actions/move.py:4:  from ..rules import Rule
    fad/rules.py:4:  from .actions import run_action
    fix: remove fad/rules.py:4:  from .actions import run_action
fad/actions/archive.py:3: error: broken import '.compress' (no such file in the project)
pdr: 17 files, 1 circular import, 1 broken import -> FAILED

$ pdr --check --rules ../fad_demo.rules --allow-cycles --allow-broken
fad/actions/move.py:4: error: fad/actions must not import fad/rules, a layer above it (../fad_demo.rules:7)
    fad/actions/move.py:4:  from ..rules import Rule
pdr: 17 files, 1 circular import (allowed), 1 broken import (allowed), 1 rule violation -> FAILED

$ pdr --why main.py paths.py
$ pdr --impact config.py
$ pdr --dot --rules ../fad_demo.rules | dot -Tsvg -o graph.svg
```

The circular import is real: with the missing `fad/actions/compress.py` added, `python3 -c "import fad.rules"`
fails with `ImportError: cannot import name 'Rule' from partially initialized module 'fad.rules'`.

## Which imports count for circular imports

Python runs a module from top to bottom the first time it is imported, so only an import that runs
*while a module is loading* can find another module half-loaded. Each import is tagged with when it runs:

| Where the import is | When it runs | Counted for cycles |
|---|---|---|
| top level, or inside `if` / `try` / `with` / `class` | while the module loads | yes |
| inside a `def` | when the function is called | no |
| inside `if __name__ == "__main__":` | only when the file is run as a script | no |
| inside `if TYPE_CHECKING:`, TypeScript `import type` | never | no |
| JavaScript `import('x')` | later | no |

`--why`, `--impact` and the critical-files ranking still count every import, because a file depends on
what it imports inside a function too.

Importing `a.b.c` also runs `a/__init__.py` and `a/b/__init__.py` first (unless the importing file is
inside that package, which is then already loading), so those count as links too. jesse's PR #633 bug
went through such a hidden step: `from jesse.models.Route import Route` ran `jesse/models/__init__.py`,
which imported a file that imported `jesse.routes` back.

## Tested on real projects and real bug fixes

pdr was run on 16 open-source projects (October 2026): requests, flask, httpx, rich, click, Django, SymPy,
FastAPI, Black and jesse (Python), Express, axios and Vue (JavaScript/TypeScript), Redis, LevelDB and
nlohmann/json (C/C++). Most take 5 to 300 ms; SymPy (1,609 files) takes about 1.1 s. Checking every
reported broken import by hand found false alarms (a code folder named `env/`, bundled `vendor/` code,
imports of `.css`, `.vue` and `.def` files, `dist/` build output, `?raw` imports, TypeScript `.d.ts`
files, Cython modules), which are all handled now.

It was also run on the code before and after six merged pull requests that fixed a circular import:
pydantic #10807, LangChain #23182, jesse #633, TransformerLens #1805, Astropy #20485 and SciPy #26210.
Before each fix pdr reports a loop through the lines the developers changed. In jesse's latest code it
also found a loop nobody had reported (now [jesse issue #636](https://github.com/jesse-ai/jesse/issues/636)): with `import jesse.store` as the first import, Python fails with
`cannot import name 'store' from partially initialized module 'jesse.store'`
(`store/__init__.py` → `state_app.py` → `models/__init__.py` → `FuturesExchange.py` → `store`).

## Architecture rules (`pdr.rules`)

```
# top layer first: a layer may import the layers below it, never one above
layers  app/ui  app/services|app/jobs  app/db
forbid  app/db -> app/ui        # nothing under app/db may import app/ui
entry   scripts                 # more start points for --unused
```

Paths are folders or files relative to the project; `app/db` covers the folder `app/db/` and the file
`app/db.py`, and paths joined with `|` share a layer. `--check` fails on every import that breaks a rule
and warns about rule paths that match no file.

## Data structures and algorithms

| Structure / algorithm | Used for | Cost |
|---|---|---|
| Adjacency list + reverse adjacency list | the graph: what a file imports, who imports it | O(V + E) space |
| Hash table with separate chaining (`hash_map.h`) | file and module lookups while resolving imports | O(1) average |
| Binary min-heap (`min_heap.h`) | Kahn's algorithm: by layer, then alphabetical, so the order is stable | O(log n) push / pop |
| Trie (`trie.*`) | file search by name or path prefix | O(length of the prefix) |
| Path trie (`path_trie.h`) | rule paths, one node per folder name | O(depth of the path) |
| Bitsets | how many files each file reaches | 64 files per machine word |
| Tarjan's SCC, iterative | circular imports | O(V + E) |
| Greedy cycle fix | imports to remove, ties broken by stability | O(k · E · (V + E)) inside a loop |
| Kahn's topological sort | build order and layers | O(V log V + E) |
| BFS | why A needs B, impact, unused files | O(V + E) |

V = files, E = import links.

## Limitations

- Static analysis only: imports whose name is built at run time (`importlib.import_module(name)`,
  `require(variable)`) cannot be seen.
- C/C++ include folders (`-I`) are not known; included files are matched by the end of their path.
- Unused files depend on recognising start files (scripts, tests, entry points); a library's public
  modules must be named with `entry`.
- Rule paths are folders and files, not wildcards.
- A loop whose imports only name whole modules (`from pkg import module`) cannot fail, because Python
  allows importing a module that is still loading, but it is still reported.

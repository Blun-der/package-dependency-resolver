// Builds report/build/Mini_Project_Report.docx.
//   node report/build_report.js [pages.json]
// pages.json (made by report/build.sh from a rendered copy) holds the page
// number of every heading, figure and table for the contents and the lists.
const fs = require("fs");
const path = require("path");
const L = require("./lib");
const { D, p, centered, blank, Report, grid, excerpt, roman, tocLine } = L;
const { AlignmentType: AT } = D;

const ROOT = path.resolve(__dirname, "..");
const SRC = path.join(ROOT, "cpp", "src");
const FIG = path.join(__dirname, "build", "fig");
const OUT = path.join(__dirname, "build", "Mini_Project_Report.docx");
const pages = process.argv[2] && fs.existsSync(process.argv[2]) ? JSON.parse(fs.readFileSync(process.argv[2], "utf8")) : {};

const TITLE = "Package Dependency Resolver";
const R = new Report(pages);
const c = (...k) => R.cite(...k);
const fig = (name) => path.join(FIG, name);
const src = (name) => path.join(SRC, name);

// =====================================================================
// Cover page (no page number)
// =====================================================================
const cover = [
    ...blank(1),
    centered("AJEENKYA D Y PATIL UNIVERSITY PUNE", { bold: true, size: 30 }, { spacing: { after: 60 } }),
    centered("SCHOOL OF ENGINEERING", { bold: true, size: 26 }, { spacing: { after: 60 } }),
    centered("Department of {{Department name}}", { size: 24 }),
    ...blank(1),
    centered("{{Insert University Logo here}}", { italics: true, size: 24 }),
    ...blank(1),
    centered("A MINI PROJECT REPORT ON", { bold: true, size: 24 }),
    ...blank(1),
    centered(`“${TITLE.toUpperCase()}”`, { bold: true, size: 36 }),
    ...blank(1),
    centered("Submitted in partial fulfilment of the requirements of the course", { size: 22 }, { spacing: { after: 40 } }),
    centered("Data Structures using C++ (E0556B)", { bold: true, size: 26 }, { spacing: { after: 40 } }),
    centered("Second Year B.Tech, Semester III", { size: 24 }),
    ...blank(1),
    centered("Submitted by", { bold: true, size: 24 }),
    grid(["Sr.", "Name of student", "PRN", "Roll No."], [["1", "{{Name}}", "{{PRN}}", "{{Roll no.}}"]], [0.08, 0.47, 0.25, 0.2], { size: 22, align: [AT.CENTER, AT.LEFT, AT.CENTER, AT.CENTER] }),
    ...blank(1),
    centered("Under the guidance of", { bold: true, size: 24 }, { spacing: { after: 40 } }),
    centered("{{Prof. Name of Guide}}", { bold: true, size: 26 }),
    ...blank(1),
    centered("Academic Year 2026 – 2027", { bold: true, size: 26 }),
];

// =====================================================================
// Front matter (roman page numbers)
// =====================================================================
const NOBORDER = { style: D.BorderStyle.NONE, size: 0, color: "FFFFFF" };
const NOBORDERS = { top: NOBORDER, bottom: NOBORDER, left: NOBORDER, right: NOBORDER };

const front = [];
front.push(R.title("CERTIFICATE", "Certificate"));
front.push(p(`This is to certify that the mini project entitled **“${TITLE}”** has been carried out and submitted by the following student of Second Year B.Tech, Semester III, Department of {{Department name}}, School of Engineering, Ajeenkya D Y Patil University Pune, in partial fulfilment of the course **Data Structures using C++ (E0556B)** during the academic year 2026 – 2027. The work has been found satisfactory.`));
front.push(grid([], [["1.", "{{Name of student}}", "{{PRN}}"]], [0.08, 0.6, 0.32], { header: false, size: 23, align: [AT.CENTER, AT.LEFT, AT.CENTER] }));
front.push(...blank(4));
front.push(grid([], [
    ["____________________", "____________________", "____________________"],
    ["**{{Name of Guide}}**", "**{{Name}}**", "**{{Name}}**"],
    ["Project Guide", "Head of Department", "Dean, School of Engineering"],
], [0.333, 0.333, 0.334], { header: false, size: 22, borders: NOBORDERS, align: [AT.CENTER, AT.CENTER, AT.CENTER] }));
front.push(...blank(3));
front.push(p("Place: {{__________}}"), p("Date: {{__________}}"));

front.push(R.title("DECLARATION", "Declaration"));
front.push(p(`I hereby declare that the mini project report entitled **“${TITLE}”** is my own original work, carried out under the guidance of {{Prof. Name of Guide}}. The work has not been submitted earlier, in part or in full, to this or any other university or institution for the award of any degree, diploma or credit. All sources of information, code and ideas used have been duly acknowledged in the references.`));
front.push(grid(["Sr.", "Name of student", "PRN", "Signature"], [["1", "{{Name}}", "{{PRN}}", ""]], [0.08, 0.42, 0.25, 0.25], { size: 22, align: [AT.CENTER, AT.LEFT, AT.CENTER, AT.LEFT] }));
front.push(...blank(1), p("Place: {{__________}}"), p("Date: {{__________}}"));

front.push(R.title("ACKNOWLEDGEMENT", "Acknowledgement"));
front.push(
    p("I would like to express my sincere gratitude to my project guide, {{Prof. Name of Guide}}, for the guidance, encouragement and patient feedback given throughout this mini project. The suggestions received during the reviews shaped both the direction of the work and the way it is presented in this report."),
    p("I am thankful to {{Name}}, Head of the Department of {{Department name}}, for providing the facilities and the academic environment in which this project could be carried out, and to {{Name}}, Dean of the School of Engineering, and Ajeenkya D Y Patil University for the opportunity to apply the concepts of the course Data Structures using C++ to a practical problem."),
    p("I also thank the faculty members and the laboratory staff of the department for their help and support during the semester."),
    p("Finally, I am grateful to my family and friends for their constant encouragement and support."),
    ...blank(1),
    new D.Paragraph({ alignment: AT.RIGHT, children: L.runs("{{Name}}", { italics: true }) }),
);

front.push(R.title("ABSTRACT", "Abstract"));
front.push(p("Every software project is a network of files that import each other, and many everyday faults are properties of that network: circular imports that crash a Python program while it starts, imports of files that no longer exist, changes whose effect nobody can predict, and layers of an application that use each other in the wrong direction. Existing tools each cover one language and usually stop at reporting that a problem exists. This project builds Package Dependency Resolver, a command-line program written in C++17 that reads a Python, JavaScript/TypeScript or C/C++ project, models it as a directed graph and explains each problem with the exact source lines involved. The graph is stored as adjacency and reverse adjacency lists; a hash table with separate chaining resolves imports to files; Tarjan’s strongly-connected-components algorithm finds circular imports, considering only imports that run while a module loads; a greedy search suggests which imports to remove; Kahn’s algorithm with a binary min-heap gives a build order; breadth-first search answers “why does A need B?”; bitsets count, for every file, how many files depend on it; and tries match file names and the paths of user-written architecture rules. Every data structure is implemented by hand using only the standard library. The program passes 28 unit tests and 29 command-line tests, runs clean under memory-error checkers, and on five widely used open-source Python libraries reports only real load-time loops. Measurements on generated projects of up to one million files confirm the expected linear growth of the graph algorithms. The project shows that classical data structures and graph algorithms solve a real software-engineering problem efficiently."));
front.push(p("**Keywords:** data structures, C++, directed graph, strongly connected components, topological sort, hash table, trie, static analysis"));

// contents and lists: filled from the previous pass
const tocTitle = R.title("TABLE OF CONTENTS");
const listsTitle = R.title("LIST OF FIGURES", "List of Figures, List of Tables, List of Abbreviations");

// =====================================================================
// Chapter 1: Introduction
// =====================================================================
const body = [];
const add = (...x) => body.push(...x.flat());

add(R.chapterHeading(1, "Introduction"));
add(R.h2("1.1 Background"));
add(p("A program of any real size is split into many source files, often called modules. A file uses the code of another file by importing it: `import` and `from … import` in Python, `import` and `require()` in JavaScript and TypeScript, and `#include` in C and C++. Taken together, these import statements form a **directed graph**: every file is a node, and an edge A → B means “file A imports file B”. In a project of a few hundred files no developer can keep this graph in mind, yet many everyday problems are properties of it."));
add(R.bullets([
    "**Circular imports.** Python runs the code of a module from top to bottom the first time the module is imported. If `a.py` imports `b.py` while it is still loading, and `b.py` imports something from `a.py`, then `b.py` finds `a.py` only half-loaded and the program stops with `ImportError: cannot import name … from partially initialized module`. The error message names one module, not the chain of files that forms the loop.",
    "**Broken imports.** An import of a file that has been renamed or deleted only fails when that line runs, possibly long after the change was made.",
    "**Impact of a change.** Before editing a widely used file such as a configuration module, a developer wants to know which files may break.",
    "**Order and layers.** Builds, tests and new team members all benefit from knowing which files are the foundation and which depend on everything else.",
    "**Architecture erosion.** Teams agree that, for example, the database layer must never use the user-interface layer. Nothing enforces such agreements, so over months small violations accumulate, together with files that nothing uses any more.",
]));
add(p(`Professional tools exist for parts of this problem (Chapter 2), but each covers a single language and most of them stop at saying that a problem exists. The underlying questions, however, are classical graph questions: a circular import is a strongly connected component ${c("tarjan")}, a build order is a topological order ${c("kahn")}, “why does A need B?” is a shortest path and “what breaks if A changes?” is reachability in the reversed graph ${c("clrs")}. This project, **Package Dependency Resolver** (command name \`pdr\`), applies these algorithms and the data structures of the course to real source code. Here “dependency” means the import dependencies between the files of one project; choosing compatible versions of downloaded packages, which package managers also call dependency resolution, is outside the scope (Section 1.4).`));
add(p("A small file automation daemon of 17 Python files is bundled with the project as a running example. It contains one circular import and one broken import on purpose, and it is used throughout this report."));

add(R.h2("1.2 Problem Statement"));
add(p("Existing import-analysis tools suffer from three limitations: each supports only one language family; they report that a circular import exists but not which import line to change; and they treat every import alike, although imports inside functions or written only for type checkers can never cause a circular-import failure. The proposed system aims to read an entire project written in Python, JavaScript/TypeScript or C/C++, build its dependency graph, and explain every circular, broken or rule-breaking import with the exact source lines involved and a suggested fix, using directed graphs, hash tables, a binary heap, tries and bitsets implemented in C++."));

add(R.h2("1.3 Objectives"));
add(R.numbered([
    "To design a directed-graph model of a software project in which files are nodes and imports are edges, built from Python, JavaScript/TypeScript and C/C++ source code.",
    "To implement the required data structures by hand in C++17: adjacency and reverse adjacency lists, a hash table with separate chaining, a binary min-heap, a character trie, a path trie and bitsets.",
    "To detect circular imports with Tarjan’s strongly-connected-components algorithm, considering only imports that run while a module loads, to show one concrete loop with its source lines, and to suggest which imports to remove.",
    "To compute a safe build order and layers with Kahn’s algorithm, to explain dependencies with breadth-first search, and to rank the files on which most other files depend.",
    "To let a team write architecture rules and find unused files, and to report every problem in a form that continuous integration (CI) systems can act on.",
    "To test the system with unit tests, command-line tests and real open-source projects, and to measure how its running time grows with the size of a project.",
]));

add(R.h2("1.4 Scope and Limitations"));
add(p("**Scope:** the system is a console program with an interactive menu and single-shot commands. It analyses Python, JavaScript/TypeScript and C/C++ projects on any operating system with a C++17 compiler. It is meant for developers who want to understand or clean up a code base, for students reading unfamiliar code, and for CI pipelines that should reject new circular or broken imports. Results are printed as text, as JSON for other programs, and in the DOT language of Graphviz for drawing the graph."));
add(p("**Limitations:**"));
add(R.bullets([
    "Static analysis only: imports whose module name is built while the program runs (for example `importlib.import_module(name)` or `require(variable)`) cannot be seen.",
    "Version resolution of packages (choosing versions that satisfy constraints, as pip or npm do) is not performed; packages from outside the project are only listed.",
    "The include folders passed to a C/C++ compiler with `-I` are not known; included files are matched by the end of their path instead.",
    "Finding unused files depends on recognising the files that run on their own (scripts, tests, entry points); a library’s public modules must be named in the rules file.",
    "The C++ program has no graphical interface. An earlier browser-based prototype exists, but it does not include the features added in the C++ version.",
]));

add(R.h2("1.5 Organisation of the Report"));
add(p("Chapter 2 surveys existing tools and the algorithms the project builds on, and identifies the gap it addresses. Chapter 3 analyses the requirements, justifies the choice of each data structure and presents the architecture, the algorithms and a flowchart. Chapter 4 describes the implementation module by module with key code and sample output. Chapter 5 lists the test cases, discusses results on the bundled sample and on five open-source projects, and reports measured running times and the complexity of each operation. Chapter 6 concludes against the objectives above and outlines future work. The appendices contain the source code, a user manual, the plagiarism report and the weekly progress log."));

// =====================================================================
// Chapter 2: Literature Survey
// =====================================================================
add(R.chapterHeading(2, "Literature Survey"));
add(R.h2("2.1 Existing Systems"));
add(R.h3("2.1.1 Tools for analysing imports"));
add(p(`**Madge** ${c("madge")} builds a graph of the modules of a JavaScript or TypeScript project, lists circular dependencies and modules that nothing imports, and can render the graph as an image through Graphviz. It is widely used, but it covers only the JavaScript family and reports a cycle as a list of files without saying which import should change.`));
add(p(`**dependency-cruiser** ${c("depcruise")} also targets JavaScript and TypeScript. Its main strength is validation: a configuration file states which dependencies are forbidden or allowed, and the tool reports every violation, including circular dependencies and orphan modules, in text, HTML or DOT form. It is designed for CI, but again for one language family only.`));
add(p(`**pydeps** ${c("pydeps")} draws the module dependency graph of a Python program by analysing its compiled bytecode with Python’s own module finder. It can highlight cycles. Because it relies on Python itself to locate modules, it needs a working environment for the analysed project, and its focus is on pictures rather than on checks.`));
add(p(`**import-linter** ${c("importlinter")} enforces “contracts” on Python projects: layered architectures, forbidden imports and independent modules. It is used in CI to stop a code base from eroding. It checks rules but is not meant for exploring the graph, ordering files or finding cycles outside the contracts.`));
add(p(`For C and C++, Lakos ${c("lakos")} showed that physical dependencies between files decide whether a large system can be built, tested and understood in parts, and introduced *levelization*: arranging components in levels so that each depends only on lower levels, with no cycles. Tools such as include-what-you-use ${c("iwyu")} analyse which headers a file really needs, using a full C++ compiler front end.`));
add(R.h3("2.1.2 Design principles"));
add(p(`Martin ${c("martin")} states two principles that this project turns into checks. The *Acyclic Dependencies Principle* says the dependency graph of components must have no cycles. The *Stable Dependencies Principle* says dependencies should point towards stable components, measured by instability I = Ce / (Ca + Ce), where Ca counts the components that depend on a component and Ce the components it depends on. A file with I near 0 is used by many and uses little; a file with I near 1 is the opposite. The cycle-fix suggestion in this project uses I to break ties (Section 3.4).`));
add(R.h3("2.1.3 Algorithms"));
add(p(`Tarjan ${c("tarjan")} gave a depth-first-search algorithm that finds all strongly connected components of a directed graph in O(V + E) time; each component with two or more nodes is a set of files that import each other in a loop. Kahn ${c("kahn")} described topological sorting by repeatedly removing nodes with no remaining incoming edges, which gives an order in which every file comes after the files it needs. Breadth-first search finds shortest paths in unweighted graphs and all reachable nodes ${c("clrs")}. Removing the fewest edges that make a directed graph acyclic is the *minimum feedback arc set* problem, one of Karp’s original NP-complete problems ${c("karp")}; greedy heuristics such as that of Eades, Lin and Smyth ${c("eades")} give good answers quickly. Counting, for every node, how many nodes it can reach has no known exact method much faster than one search per node in general, while Cohen ${c("cohen")} showed that the counts can be estimated in near-linear time.`));
add(p(`On the data-structure side, hashing with separate chaining keeps insert and lookup at O(1) expected time when the table grows with its contents ${c("knuth")}; the FNV-1a hash function ${c("fnv")} is a simple, fast hash for strings, and the splitmix64 finaliser ${c("splitmix")} mixes integer keys. The trie, introduced by Fredkin ${c("fredkin")}, retrieves keys by prefix in time proportional to the prefix length. The DOT language of Graphviz ${c("graphviz")} lays out directed graphs in layers, using the approach of Sugiyama et al. ${c("sugiyama")}.`));

add(R.h2("2.2 Comparative Study"));
add(R.table("2.1", "Comparison of existing systems and the proposed system", ["Sr.", "System / paper", "Technique or data structure", "Advantages", "Limitations"], [
    ["1", `Madge ${c("madge")}`, "Module graph, cycle and orphan search, Graphviz output", "Simple, popular, draws images", "JavaScript/TypeScript only; no fix suggested"],
    ["2", `dependency-cruiser ${c("depcruise")}`, "Module graph checked against rules in a config file", "Rich rules, CI-ready, many outputs", "JavaScript/TypeScript only"],
    ["3", `pydeps ${c("pydeps")}`, "Bytecode analysis with Python’s module finder", "Accurate module locations, clear pictures", "Python only; needs the project’s environment; mainly visual"],
    ["4", `import-linter ${c("importlinter")}`, "Layer, forbidden and independence contracts", "Enforces architecture in CI", "Python only; not for exploring the graph"],
    ["5", `Tarjan ${c("tarjan")}; Kahn ${c("kahn")}`, "Strongly connected components; topological sort", "Linear time, well understood", "Algorithms, not a tool for source code"],
    ["6", "Proposed system", "Adjacency lists, hash table, min-heap, tries, bitsets; Tarjan, Kahn, BFS, greedy cycle fix", "Three language families; load-time cycles only; exact lines and suggested fix; rules, unused files, CI output", "Static analysis only; no package version resolution"],
], [0.06, 0.18, 0.27, 0.25, 0.24]));

add(R.h2("2.3 Research Gap"));
add(p("The surveyed tools each serve one language family and either explore the graph or enforce rules, rarely both. None of them, as documented, suggests which import to change to break a cycle, and they do not distinguish an import that runs while a module loads from one inside a function or under `if TYPE_CHECKING:`, so they report loops that can never fail. This project fills that gap with one program for Python, JavaScript/TypeScript and C/C++ that reports only loops able to fail, points to the lines to change, and also checks architecture rules and finds unused files."));

// =====================================================================
// Chapter 3: System Analysis and Design
// =====================================================================
add(R.chapterHeading(3, "System Analysis and Design"));
add(R.h2("3.1 Requirement Analysis"));
add(p("**Functional requirements.** The system must:"));
add(R.numbered([
    "Read every Python, JavaScript/TypeScript and C/C++ file under a folder, skipping folders such as `node_modules`, `venv`, `.git`, `dist` and `build`.",
    "Recognise every import statement with its line number, ignoring text inside comments and string literals.",
    "Record whether each Python import runs while the module loads, later (inside a function or an `if __name__ == \"__main__\":` block) or never (under `if TYPE_CHECKING:`).",
    "Resolve each import to a file of the project, an outside package (marked as built-in or to be installed) or a broken import.",
    "Report every circular import with one concrete loop, the line of each import in it, and the imports to remove.",
    "Give a build order and layers; answer “why does A need B?” and “what breaks if A changes?”; rank the most critical files; search files by name.",
    "Read optional architecture rules (layers, forbidden imports, extra entry points) and report each import that breaks them; list files that nothing reaches.",
    "Offer an interactive menu, a full text report, a CI check with exit codes, JSON output and Graphviz output.",
]));
add(p("**Non-functional requirements.**"));
add(R.bullets([
    "**Performance:** linear-time graph algorithms; a project of 100,000 files analysed in a few seconds.",
    "**Usability:** files may be named by file name or by the end of their path; the current folder is used when none is given; messages use the `file:line: message` form that editors and CI systems turn into links.",
    "**Reliability:** no recursion in graph searches (no stack overflow on deep projects), automated tests, and runs free of memory errors under AddressSanitizer and UndefinedBehaviorSanitizer.",
    "**Portability:** standard C++17 and its standard library only; no third-party libraries.",
    "**Determinism:** adjacency lists are kept sorted by file name, so every run gives the same output in the same order.",
]));
add(R.table("3.1", "Hardware and software requirements", ["Category", "Specification"], [
    ["Processor and RAM", "Development: AMD Ryzen AI 7 350, 22 GB usable RAM. Minimum: any 64-bit processor, 2 GB RAM"],
    ["Operating system", "Arch Linux (kernel 7.2); also built and tested on Ubuntu by GitHub Actions"],
    ["Compiler / IDE", "GCC g++ 16.2.1 with GNU Make 4.4.1 (any C++17 compiler; Code::Blocks with MinGW should work but was not tested)"],
    ["Language", "C++ (C++17 standard, standard library only)"],
    ["Other tools", "Git 2.56 and GitHub, GitHub Actions (CI), Graphviz 16.1 (drawing `--dot` output)"],
], [0.28, 0.72]));

add(R.h2("3.2 Selection of Data Structures"));
add(p("Each data structure was chosen for the operation it makes cheap. Table 3.2 lists them with the place they are used and the cost of their key operations; Chapter 4 shows the code of the most important ones."));
add(R.table("3.2", "Data structures used and justification", ["Data structure", "Where used in the project", "Why it was chosen", "Time complexity of key operations"], [
    ["Adjacency list (array of sorted edge lists)", "The dependency graph: the files each file imports", "Import graphs are sparse: O(V + E) space instead of O(V²) for a matrix", "Visit all neighbours O(degree); add edge O(degree) to keep the list sorted"],
    ["Reverse adjacency list", "The files that import a given file", "“Who uses this file?” is as cheap as “what does it use?”", "Visit importers O(degree)"],
    ["Hash table with separate chaining (linked lists)", "File name → node id, module name → file, path ending → files, sets of visited links", "Resolving every import needs a fast lookup by string", "Insert / find / erase O(1) on average, O(n) worst case; growth amortised O(1)"],
    ["Binary min-heap", "Kahn’s algorithm: files ready to be placed", "Always takes the alphabetically first ready file, so the order is stable", "Push / pop O(log n)"],
    ["Stack (explicit)", "Tarjan’s algorithm: the current search path and the call stack", "Replaces recursion, so deep projects cannot overflow the call stack", "Push / pop O(1)"],
    ["Queue", "Breadth-first searches", "Visits files in order of distance, giving shortest chains", "Enqueue / dequeue O(1)"],
    ["Trie of characters", "File search by name or path prefix", "Prefix search independent of the number of files", "Insert and search O(length of the key)"],
    ["Path trie (one node per folder name)", "Paths in architecture rules and entry points", "Finds every rule that contains a file in one walk", "Match O(depth of the path)"],
    ["Bitset (rows of 64-bit words)", "Counting the files each file reaches", "One machine operation handles 64 files", "OR of two rows O(V / 64)"],
], [0.2, 0.28, 0.28, 0.24], { size: 20 }));

add(R.h2("3.3 System Architecture and Design"));
add(p("Figure 3.1 shows the architecture. The **loader** walks the project folder and reads every source file. The **parser** first blanks out comments and string literals, keeping line breaks so that line numbers stay correct, then reads the import statements and tags each one with when it runs. The **resolver** turns each import into a project file, an outside package or a broken import, using hash tables built once for the whole project. The resulting links fill two **dependency graphs** with the same nodes: one with every import and one with only the imports that run while a module loads. The **analyses** read these graphs, and the **outputs** present the results as a menu, a report, a CI check, JSON or a Graphviz drawing."));
add(R.figure(fig("architecture.png"), "3.1", "System architecture"));
add(p("Two graphs are kept because the questions differ. A file that imports another inside a function still depends on it: if the imported file is deleted, the function fails. So “why does A need B?”, “what breaks if A changes?” and the critical-files ranking use **every** import. A circular-import failure, however, can only happen while modules are loading, so cycle detection, the cycle fix and the build order use only the **load-time** imports."));
add(p("The program applies the same discipline to itself. Its own source files form six layers (command line; reports; analysis; resolver, rules and algorithms; parser and data structures; hash table, heap and helpers), written down in the file `cpp/pdr.rules` and checked by the program on every test run. Figure 3.2 shows the program’s include graph, drawn by the program itself."));
add(R.figure(fig("self-graph.png"), "3.2", "Include graph of the program’s own source code, drawn by `pdr --dot`", { maxHeightPx: 640 }));

add(R.h2("3.4 Algorithm and Flowchart"));
add(p("Figure 3.3 shows the main logic for the CI check, the path that touches every part of the system. The main algorithm is:"));
add(R.numbered([
    "Read the arguments. If the folder or a command is invalid, print the usage and stop with exit code 2.",
    "If the folder holds a `pdr.rules` file, or `--rules FILE` is given, parse the rules; a line that cannot be understood stops the program with its line number.",
    "Load every source file under the folder, skipping ignored folders. If there is none, stop with exit code 2.",
    "For each file, parse its imports, tag when each runs, and resolve each import. Add each link to the graph of all imports, and load-time links also to the load-time graph.",
    "Run Tarjan’s algorithm on the load-time graph. For each component of two or more files, find one concrete loop with breadth-first search and compute the cycle fix.",
    "Compute the build order with Kahn’s algorithm, the dependency counts with bitsets, the unused files, and the rule violations.",
    "Print every circular import, broken import and rule violation as `file:line: message`. Exit with code 1 if any of them is not allowed by a flag, otherwise with code 0.",
]));
add(R.figure(fig("flowchart.png"), "3.3", "Flowchart of the main logic (`pdr --check`)", { maxHeightPx: 700 }));

add(R.h3("3.4.1 When an import runs"));
add(p("Python marks blocks by indentation, so the blocks that contain a line are the nearest earlier statements with less indentation. The parser keeps a stack of open statements, each with its indentation and the timing of the lines inside it:"));
add(R.numbered([
    "For each line of the masked source that starts a new statement (not inside open brackets and not after a trailing backslash), pop every stack entry whose indentation is greater than or equal to this line’s indentation.",
    "The line’s timing is that of the entry now on top of the stack (load time if the stack is empty).",
    "If the timing is load time and the line starts with `def` or `async def`, or is `if __name__ == \"__main__\":`, lines inside it run later (deferred). If it is `if TYPE_CHECKING:` or `if typing.TYPE_CHECKING:`, lines inside it never run. Push the line with that inner timing.",
]));
add(p("Each line is pushed and popped at most once, so the pass costs O(characters in the file). TypeScript `import type` lines are tagged as never running, and JavaScript `import('x')` calls as deferred."));

add(R.h3("3.4.2 Resolving imports"));
add(p("All lookups go through hash tables built once: the set of project files, an index from dotted Python module names to files, and, for C/C++, an index from every path ending to the files with that ending (`a/b/c.h` is reachable as `a/b/c.h`, `b/c.h` and `c.h`). A Python import `a.b.c` tries `a/b/c.py`, `a/b/c/__init__.py`, then the shorter names; relative imports climb folders from the importing file. A file’s own folder is searched first only when that folder is not a package: inside a package, `import types` means the standard-library module, never the package’s own `types.py`. A quoted C/C++ include is looked up next to the including file, then by path ending; when several files match, the one sharing the most leading folders with the including file wins."));

add(R.h3("3.4.3 Tarjan’s strongly connected components"));
add(p(`Tarjan’s algorithm ${c("tarjan")} gives each node a discovery index and a *low-link*, the smallest index reachable from it through the current search path. A node whose low-link equals its own index is the root of a component, and the nodes above it on the stack form that component. To avoid recursion, the implementation keeps an explicit stack of (node, next edge to try):`));
add(R.numbered([
    "For each node in alphabetical order that has no index yet, push it: give it the next index, set its low-link to that index, put it on the path stack and on the work stack.",
    "Look at the top of the work stack. If it has an untried edge to w, advance the edge; if w has no index, push w; else if w is on the path stack, lower the node’s low-link to w’s index.",
    "If the node has no untried edge, pop it from the work stack and lower its parent’s low-link to its own. If its low-link equals its index, pop the path stack down to it: those nodes are one component.",
]));
add(p("Each node is pushed once and each edge tried once, so the algorithm runs in O(V + E). A component is completed only after every component it imports, so the components come out in dependencies-first order, which Section 3.4.6 relies on."));

add(R.h3("3.4.4 Suggesting which imports to remove"));
add(p(`Finding the fewest links whose removal leaves no loop is NP-hard ${c("karp")}, so the system uses a greedy method in the spirit of ${c("eades")}, applied to each component separately:`));
add(R.numbered([
    "Copy the component into a small graph of its own, so every trial costs O(size of the component) instead of O(size of the project). If it has more than 2,000 links, report that it is too large to search.",
    "For every link still inside a loop, remove it for a moment, run Tarjan’s algorithm, and count the files still in loops.",
    "Keep the removal that leaves the fewest files in loops. On a tie, prefer the link that most violates the Stable Dependencies Principle: the largest I(target) − I(source), using instability over the whole project. Remaining ties go to the alphabetically first link.",
    "Repeat until no loop is left. The links removed are the suggested fix.",
]));
add(R.table("3.3", "Choosing the import to remove in the sample project’s loop", ["Link in the loop", "Files still in loops without it", "I(source)", "I(target)", "I(target) − I(source)"], [
    ["`actions/__init__.py` → `actions/move.py`", "0", "4/5 = 0.80", "3/4 = 0.75", "−0.05"],
    ["`actions/move.py` → `rules.py`", "0", "3/4 = 0.75", "3/7 = 0.43", "−0.32"],
    ["`rules.py` → `actions/__init__.py`", "0", "3/7 = 0.43", "4/5 = 0.80", "**+0.37 (chosen)**"],
], [0.36, 0.16, 0.14, 0.14, 0.2], { size: 20, align: [AT.LEFT, AT.CENTER, AT.CENTER, AT.CENTER, AT.CENTER] }));
add(p("Table 3.3 shows the method on the sample’s three-file loop. Removing any one of the three links breaks the loop, so the tie-break decides. `rules.py` is imported by four files and imports three (I = 3/7), while the `actions` package imports four files and is imported by one (I = 4/5). The stable rule engine depending on the unstable actions package is the link that points the wrong way, so the tool suggests removing `from .actions import run_action` from `rules.py`."));

add(R.h3("3.4.5 Build order and layers"));
add(p(`Kahn’s algorithm ${c("kahn")} only works on a graph without loops, so each component found by Tarjan’s algorithm is first collapsed into a single unit. The condensed graph has no loops, and Kahn’s algorithm then always produces a complete order:`));
add(R.numbered([
    "For each unit, count the units it depends on. Push every unit with count 0 into the min-heap, ordered by the name of its first file.",
    "Pop the smallest unit and append it to the order. For each unit that depends on it, set its layer to at least this unit’s layer + 1 and decrease its count; push it when the count reaches 0.",
    "Repeat until the heap is empty. Units of two or more files are shown as “loaded together (cycle)”.",
]));
add(p("Every unit enters and leaves the heap once, so the cost is O(V log V + E). The layer of a unit is 0 if it imports nothing and otherwise one more than the highest layer it imports."));

add(R.h3("3.4.6 Dependency counts with bitsets"));
add(p("For each file the system reports how many files it needs and how many files need it. One breadth-first search per file costs O(V · (V + E)). The system computes the same numbers on the components instead:"));
add(R.numbered([
    "Run Tarjan’s algorithm on the graph of all imports. Give every component a row of bits, one bit per file.",
    "Visit the components in dependencies-first order. A component’s row is its own files OR the row of every component it imports; those rows are already complete.",
    "A file’s count is the number of set bits in its component’s row minus one (the file itself). The same pass in the opposite order, following importers, gives the other count.",
]));
add(p("One OR of two 64-bit words handles 64 files at once, so the work drops to O(V / 64 · (V + E)). For large projects the files are processed in slices so that all rows together stay under 64 MB."));

add(R.h3("3.4.7 Shortest chains, impact and unused files"));
add(p("“Why does A need B?” is answered by breadth-first search from A with a parent pointer for every visited file; following the parents back from B gives the shortest chain of imports, each with its line. “What breaks if A changes?” is a breadth-first search from A over the reverse lists. Unused files come from one breadth-first search that starts from all *start files* at once: scripts, tests, files with a `__main__` guard, configuration files, every C/C++ source file (each is compiled on its own), and the paths named by `entry` rules. Reaching a Python module also reaches the `__init__.py` file of each enclosing package, because Python runs those too. Every file not reached is reported."));

add(R.h3("3.4.8 Architecture rules"));
add(p("A rules file contains `layers` lines (paths from the top layer down; paths joined with `|` share a layer), `forbid FROM -> TO` lines and `entry` lines. Every path is stored in a path trie whose nodes are folder names. Walking a file’s path down the trie once yields every rule path that contains the file, shallowest first, so the deepest layer path decides the file’s layer. Then each import link A → B is checked: it breaks a layers rule if both files have a layer and A’s layer is below B’s, and it breaks a forbid rule if A lies under the rule’s first path and B under its second. A rule path that matches no file produces a warning, which catches spelling mistakes."));

add(R.h2("3.5 Module Description"));
add(R.table("3.4", "Modules of the system", ["Module", "Purpose", "Data structure used", "Team member"], [
    ["`util`", "File types, ignored folders, built-in module lists, path and string helpers", "Arrays, hash set", "{{Name}}"],
    ["`parser`", "Masks comments and strings; reads imports with lines and timing", "Indentation stack, array of line starts (binary search)", "{{Name}}"],
    ["`resolver`", "Import → project file, outside package or broken", "Hash tables (module index, path endings)", "{{Name}}"],
    ["`graph`", "Files and import links with the line of each import", "Adjacency and reverse adjacency lists, hash table", "{{Name}}"],
    ["`hash_map`", "Hash table and hash set (templates)", "Separate chaining with linked lists", "{{Name}}"],
    ["`min_heap`, `trie`, `path_trie`", "Ordering, file search, rule paths", "Binary heap, tries", "{{Name}}"],
    ["`algorithms`", "Tarjan, cycle fix, Kahn, BFS, bitset counts", "Stacks, queue, heap, bitsets", "{{Name}}"],
    ["`rules`", "Reads `pdr.rules` and checks every link", "Path trie", "{{Name}}"],
    ["`analyzer`", "Runs the whole pipeline; unused files", "All of the above", "{{Name}}"],
    ["`json_report`, `dot_export`", "JSON and Graphviz output", "Hash sets", "{{Name}}"],
    ["`main`", "Menu, commands, `--check` and exit codes", "—", "{{Name}}"],
], [0.2, 0.38, 0.27, 0.15], { size: 20 }));

// =====================================================================
// Chapter 4: Implementation
// =====================================================================
add(R.chapterHeading(4, "Implementation"));
add(R.h2("4.1 Tools and Technologies"));
add(R.bullets([
    "**Language:** C++17 with the standard library only. No third-party library is used; every data structure in Table 3.2 is written by hand.",
    "**Compiler and build:** GCC g++ 16.2.1 with the flags `-std=c++17 -O2 -Wall -Wextra -pedantic`, and GNU Make 4.4.1. The program builds without warnings. Without Make it builds with `g++ -std=c++17 -O2 -o pdr cpp/src/*.cpp`.",
    "**Version control and CI:** Git and a private GitHub repository. A GitHub Actions workflow builds the program and runs every test on Ubuntu on each push.",
    "**Testing:** a small test framework written for the project (28 engine tests), a shell script of 29 command-line tests, and GCC’s AddressSanitizer and UndefinedBehaviorSanitizer.",
    "**Supporting tools:** Graphviz to draw the `--dot` output. The benchmark draws its own charts as SVG, so no plotting library is needed.",
    "**AI assistance:** Claude Code (Anthropic), an AI coding assistant, was used during development and in drafting this report. All code was reviewed, tested and is explained in this report.",
]));
add(p("The source code is organised as follows: `cpp/src/` holds the program (24 files, 3,382 lines), `cpp/tests/` the engine and command-line tests, `cpp/bench/` the benchmark and the code that draws its charts, `cpp/pdr.rules` the program’s own architecture rules, and `sample/fad_demo/` the bundled example project."));

add(R.h2("4.2 Module-wise Implementation"));
add(R.h3("4.2.1 Parser: when each import runs"));
add(p("The parser first produces a masked copy of the file in which comments and string contents are replaced by spaces; line breaks are kept, so offsets and line numbers are the same in both copies. Listing 4.1 is the heart of the timing pass of Section 3.4.1: on each line that starts a statement it pops the blocks that have ended, takes its timing from the enclosing block, and pushes itself as a possible new block."));
add(R.listing("4.1", "Timing of a Python line from the stack of open blocks (`parser.cpp`)", excerpt(src("parser.cpp"), "if (p < end && text[p] != '\\r' && depth == 0 && !continued) {", "open.push_back({indent, inside});") + "\n}"));

add(R.h3("4.2.2 Resolver"));
add(p("Listing 4.2 shows the rule that fixed a real mistake found on the `click` library (Section 5.2): a module inside a package must not be resolved to a file next to it when it has the same name as a standard-library module. Only scripts, which live in folders without `__init__.py`, have their own folder on Python’s search path."));
add(R.listing("4.2", "Looking up an absolute Python module (`resolver.cpp`)", excerpt(src("resolver.cpp"), "std::optional<std::string> lookupPyModule(", "return std::nullopt;") + "\n}"));

add(R.h3("4.2.3 Hash table with separate chaining"));
add(p("`HashMap<K, V>` stores an array of buckets, each the head of a singly linked list of the entries whose hash falls there. The number of buckets is a power of two, so the bucket of a key is `hash & (buckets − 1)`. When the table is more than 75% full it doubles, and `grow()` relinks the existing nodes into the new array without copying any entry (Listing 4.3). Strings are hashed with FNV-1a and integer keys with the splitmix64 finaliser."));
add(R.listing("4.3", "Insertion and growth of the hash table (`hash_map.h`)", excerpt(src("hash_map.h"), "bool insert(const K& key, const V& value) {", "return true;") + "\n}\n" + excerpt(src("hash_map.h"), "void grow() {", "head = next;")));

add(R.h3("4.2.4 Graph"));
add(p("`Graph` stores, for every node, a list of outgoing edges and a list of incoming edges, each edge carrying the line number and text of the import that created it. A second import of the same file keeps the first evidence. New edges are inserted at their sorted position, so every walk over the graph visits neighbours alphabetically and every result is the same on every run. A hash table maps file names to node ids."));

add(R.h3("4.2.5 Tarjan’s algorithm without recursion"));
add(p("Listing 4.4 is the part of the iterative Tarjan’s algorithm that runs when all edges of a node have been tried: the node “returns” to its caller by passing its low-link up, and if it is the root of a component, the component is popped from the path stack."));
add(R.listing("4.4", "Completing a node in the iterative Tarjan’s algorithm (`algorithms.cpp`)", excerpt(src("algorithms.cpp"), "// all edges of v done", "sccs.push_back(comp);")));

add(R.h3("4.2.6 Cycle fix"));
add(p("Listing 4.5 is the inner loop of the greedy search of Section 3.4.4. Each link of a loop is removed for a moment (by adding it to a hash set of removed links that Tarjan’s algorithm is told to skip), the files still in loops are counted, and the best candidate is kept."));
add(R.listing("4.5", "Trying each link of a loop (`algorithms.cpp`)", excerpt(src("algorithms.cpp"), "for (const Edge& e : loop.out(v)) {", "                }", { skip: 0 })));

add(R.h3("4.2.7 Build order with a min-heap"));
add(R.listing("4.6", "Kahn’s algorithm on the condensed graph (`algorithms.cpp`)", excerpt(src("algorithms.cpp"), "while (!heap.empty()) {", "if (--remaining[d] == 0) heap.push(d);") + "\n    }\n}"));
add(p("The heap is a hand-written binary min-heap template ordered by a comparison function; here it compares the name of the first file of each unit, so files that become ready at the same time are placed alphabetically."));

add(R.h3("4.2.8 Dependency counts with bitsets"));
add(p("Listing 4.7 is the pass of Section 3.4.6 over one slice of files. `rows` holds one row of `words` 64-bit words per component; `setBit` marks a file of the current slice."));
add(R.listing("4.7", "Bitset rows over the components (`algorithms.cpp`)", excerpt(src("algorithms.cpp"), "for (int c : visitOrder) {", "for (int f : sccs[c]) count[f] += bits;") + "\n}"));

add(R.h3("4.2.9 Path trie and rules"));
add(p("Each node of the path trie has a hash table from a folder or file name to a child node and a list of rule ids. Listing 4.8 walks a file’s path and collects the ids along the way. The last part of the path may also match without its extension, so the rule path `fad/utils` covers both the folder `fad/utils/` and the module file `fad/utils.py`."));
add(R.listing("4.8", "Every rule path that contains a file (`path_trie.h`)", excerpt(src("path_trie.h"), "std::vector<int> matches(const std::string& path) const {", "return out;") + "\n}"));

add(R.h3("4.2.10 Unused files"));
add(R.listing("4.9", "Multi-source breadth-first search with parent packages (`analyzer.cpp`)", excerpt(src("analyzer.cpp"), "for (size_t head = 0; head < queue.size(); head++) {", "if (const int init = graph.id(d + \"/__init__.py\"); init >= 0) reach(init);") + "\n}"));

add(R.h3("4.2.11 Command line and CI check"));
add(p("`main.cpp` reads the arguments (the folder is optional and defaults to the current folder), loads the rules file, runs the analysis and prints the requested output. The CI check prints one problem per line in `file:line: message` form, followed by indented lines with the imports involved, and returns exit code 0 when no problem is left, 1 when the check failed and 2 for incorrect use. Flags `--allow-cycles` and `--allow-broken` relax one rule each, so a project with old problems can still block new kinds of problem."));

add(R.h2("4.3 Sample Input and Output"));
add(p("The figures below show the program run on the bundled sample project, a file automation daemon of 17 Python files (`sample/fad_demo`). Without arguments the program opens the menu of Figure 4.1; every menu item is also available as a command."));
add(R.figure(fig("menu.png"), "4.1", "Interactive menu", { maxWidth: 0.8 }));
add(R.figure(fig("summary.png"), "4.2", "Summary at the top of the full report (`pdr --report`)", { maxWidth: 0.9 }));
add(p("Figure 4.3 shows the CI check. The circular import is printed with every line of the loop and the suggested fix, followed by the broken import. The exit code is 1, so a CI pipeline marks the run as failed."));
add(R.figure(fig("check.png"), "4.3", "CI check of the sample project (`pdr --check`)"));
add(R.figure(fig("why.png"), "4.4", "Shortest chain of imports from `main.py` to `paths.py` (`pdr --why`)", { maxWidth: 0.95 }));
add(R.figure(fig("impact.png"), "4.5", "Files that may break if `paths.py` changes (`pdr --impact`)", { maxWidth: 0.9 }));
add(p("With the rules file `sample/fad_demo.rules`, which describes the daemon’s intended layers, the check also reports the import that points upwards (Figure 4.6). Figure 4.7 is the Graphviz drawing of the same analysis: the loop is red, the import the cycle fix removes is dashed and labelled “remove”, the import that breaks the layers is orange, and the broken import points to a dashed “not found” note."));
add(R.figure(fig("rules.png"), "4.6", "Architecture rule violation (`pdr --check --rules`)"));
add(R.figure(fig("sample-rules.png"), "4.7", "Dependency graph of the sample project (`pdr --dot`, drawn with Graphviz)"));

// =====================================================================
// Chapter 5: Testing and Results
// =====================================================================
add(R.chapterHeading(5, "Testing and Results"));
add(R.h2("5.1 Test Cases"));
add(p("Testing was done at three levels: engine tests that call the data structures and algorithms directly (Table 5.1), command-line tests that run the finished program and check its output and exit code (Table 5.2), and runs on real open-source projects (Section 5.2). `make test` runs the first two; all 57 tests pass on Arch Linux and on Ubuntu in GitHub Actions."));
const PASS = "Pass";
const engine = [
    ["Python import forms", "`import os, sys as system`; multi-line `from a.b import (c, d)`; relative imports; commented and docstring imports", "6 imports, correct levels and lines; comment and docstring ignored"],
    ["JavaScript import forms", "`import`, `export … from`, `require()`, `import()`, `import type`, a commented import", "6 module names, commented one ignored"],
    ["Import text in strings", "`\"import x from './not-real'\"` and a template literal", "Only `./real` found"],
    ["Python import timing", "Imports in `def`, `async def`, method with multi-line signature, `class`, `try`, `else`, `if TYPE_CHECKING`, `__main__` guard", "Each import tagged load / deferred / type-checking correctly"],
    ["JavaScript import timing", "`import type`, `export type`, default import named `type`, `import()`", "Tags L, T, T, L, D"],
    ["C/C++ includes", "`<vector>`, `#  include \"graph.h\"`, commented and quoted includes, `#include_next`", "3 includes with kinds and lines"],
    ["Min-heap order", "Push 5, 1, 4, 2, 3, 0", "Pops 0, 1, 2, 3, 4, 5"],
    ["Trie prefix search", "Sample file paths; search `r`, `ru`, `zzz`", "2 files, `rules.py`, none"],
    ["Hash table basics", "Duplicate insert, overwrite, 5,000 inserts, erase, copy", "Values correct, load factor ≤ 0.75, longest chain ≤ 8, deep copy"],
    ["Hash table collisions", "100 keys with a hash that always returns 0", "One chain of 100; find and erase from its middle still work"],
    ["Tarjan components", "a→b→c→a, c→d, e→d", "Components {a,b,c}, {d}, {e}"],
    ["Build order with a cycle", "Graph with a two-file loop x↔y", "json, db, web, x+y, app; layers 0 … 2"],
    ["Long chain", "5,001 files in a chain", "Complete order, no stack overflow"],
    ["Shortest path, reachability", "a→b→c, a→c, c→d", "a→c→d; no path back; importers of d are a, b, c"],
    ["Cycle fix", "Shared link, two disjoint loops, stability tie, size limit", "1 link, 2 links, `zcore→plugin`, no plan over the limit"],
    ["Bitsets equal BFS", "20 random graphs of up to 300 files, one slice and 64-file slices", "Identical counts in both directions"],
    ["Python resolution", "`src/` layout, packages, relative, outside and missing imports", "Expected links; outside `os`, `requests`; `.missing` broken at line 3"],
    ["Load-time cycles", "a→b at load time, b→a under `TYPE_CHECKING` and inside a function; a package `types.py`", "No cycle; both links in the full graph; `import types` is the standard library"],
    ["JavaScript resolution", "Extensions, `index.js`, `@/` alias, scoped packages", "Expected links; outside `lodash`, `@scope/pkg`, `fs`; `./nope` broken"],
    ["Path trie", "Paths `app`, `app/core`, a file path, `lib/utils`", "Matches shallowest first; module file matched; no partial names"],
    ["Rules file parsing", "Layers with `|`, forbid, entry, comments; three bad lines", "Correct rules; errors carry `pdr.rules:N:`"],
    ["Rules check", "7 links against layers, forbid and an unused path", "Exactly 2 violations and 1 warning"],
    ["Unused files", "13-file project with scripts, tests, packages, an entry, headers", "Unused: `pkg/old.py`, `pkg/older.py`, `src/unused.h`"],
    ["C/C++ resolution", "Same folder, path ending, two candidates, system headers, missing", "Expected links; the closer `config.h`; `missing.h` broken"],
    ["Ignored folders", "Files in `venv/`, `node_modules/` and a `README.md`", "Only `a.py` and `b.py` analysed"],
    ["Sample project end to end", "`sample/fad_demo`", "17 files, the 3-file cycle, `.compress` broken, valid order, `config.py` most critical, fix `rules.py:4`"],
    ["Graphviz export", "`sample/fad_demo`", "Valid DOT with folder boxes, “remove” and “not found” labels"],
    ["JSON output", "A hand-written JSON checker on six bad inputs; the sample; import lines with quotes and backslashes", "Bad inputs rejected; both outputs valid JSON"],
].map((r, i) => [`T${i + 1}`, r[0], r[1], r[2], "As expected", PASS]);
add(R.table("5.1", "Engine test cases and results", ["Test ID", "Description", "Input", "Expected output", "Actual output", "Pass / Fail"], engine, [0.07, 0.17, 0.3, 0.26, 0.11, 0.09], { size: 18, align: [AT.CENTER, AT.LEFT, AT.LEFT, AT.LEFT, AT.LEFT, AT.CENTER] }));
const cli = [
    ["`--check` on the sample", "Exit 1; circular import line with the full loop"],
    ["`--check` on the sample", "Broken import `.compress` reported at `archive.py:3`"],
    ["`--check --allow-cycles --allow-broken`", "Exit 0, “OK”"],
    ["`--allow-cycles --check`", "Exit 1; broken import still reported"],
    ["`--check` on a clean 3-file project", "Exit 0; “3 files, 0 circular imports, 0 broken imports -> OK”"],
    ["`--check` on the sample", "“fix: remove fad/rules.py:4”"],
    ["`--check` on the program’s own source", "Exit 0, with its `pdr.rules`"],
    ["`--json`", "Contains `\"cycles\": 1` (validity: test T28)"],
    ["`--dot`", "Starts `digraph dependencies {`; renders with Graphviz"],
    ["`--why main.py paths.py`", "“fad/main.py needs fad/utils/paths.py”"],
    ["`--help`", "Exit 0, lists exit codes"],
    ["No folder given (two tests)", "The current folder is analysed"],
    ["Folder that does not exist / has no code", "Exit 2 with a clear message (two tests)"],
    ["Unknown command; wrong argument count", "Exit 2 (two tests)"],
    ["`--allow-cycles` without `--check`", "Exit 2"],
    ["`--impact __init__.py`", "Exit 2, “matches several files”"],
    ["Rules: upward import; summary line", "Exit 1, violation at `move.py:4`; “1 rule violation” (two tests)"],
    ["Rules: `pdr.rules` read from the folder; unused path", "Exit 0; warning `pdr.rules:2: warning: 'nowhere' matches no file` (two tests)"],
    ["Rules: bad line; `--rules` without a file; missing file", "Exit 2 with the reason (three tests)"],
    ["`--unused` on the sample and on a project with an extra file", "“None …”; then the extra file listed (two tests)"],
].map((r, i) => [`C${i + 1}`, r[0], r[1], "As expected", PASS]);
add(R.table("5.2", "Command-line test cases and results (29 tests in 20 groups)", ["Test ID", "Command / situation", "Expected output", "Actual output", "Pass / Fail"], cli, [0.07, 0.38, 0.36, 0.1, 0.09], { size: 18, align: [AT.CENTER, AT.LEFT, AT.LEFT, AT.LEFT, AT.CENTER] }));

add(R.h2("5.2 Results and Discussion"));
add(R.h3("5.2.1 Reproducing a real failure"));
add(p("The sample project’s loop is not only a finding on paper: in a copy of the sample with the missing `compress.py` added, Python itself fails with the circular-import error (Figure 5.1, top). Python’s message names `fad.rules` and `move.py`; the tool names all three files of the loop, the line of each import and the import to change (Figure 5.1, bottom). Moving that one import into the function that uses it makes `import fad.rules` succeed and the check pass."));
add(R.figure(fig("python-vs-pdr.png"), "5.1", "Python’s error on the sample compared with the tool’s report"));

add(R.h3("5.2.2 Real open-source projects"));
add(p("To test the system on code it was not written for, it was run on shallow copies of five widely used Python libraries (October 2026). The first version counted every import for cycle detection, and it reported loops in all five, although all five libraries work. Inspecting each reported line showed why: the loops went through imports under `if TYPE_CHECKING:` (requests, httpx, rich), imports inside functions (flask) and imports under `if __name__ == \"__main__\":` (rich), none of which runs while a module loads. The same inspection revealed a resolver mistake: inside the `click` package, `from types import TracebackType` had been linked to `click/types.py` instead of Python’s standard `types` module. Both problems were fixed (Sections 3.4.1 and 3.4.2) and covered by new tests. Table 5.3 shows the result."));
add(R.table("5.3", "Cycles found in five open-source Python projects", ["Project", "Files", "Imports inside functions / type-checking only", "Cycles, all imports", "Cycles, load-time only"], [
    ["requests", "37", "9 / 34", "1", "0"],
    ["httpx", "60", "23 / 19", "2", "0"],
    ["rich", "213", "64 / 41", "2", "0"],
    ["flask", "83", "52 / 49", "2", "1 (example app)"],
    ["click", "91", "73 / 24", "1", "1 (Windows only)"],
], [0.16, 0.1, 0.32, 0.2, 0.22], { align: [AT.LEFT, AT.CENTER, AT.CENTER, AT.CENTER, AT.CENTER] }));
add(p("The two loops that remain are real load-time loops that work only because of the order of the lines in the files. Flask’s is in an example application: `__init__.py` creates `app` and then imports `views`, which imports `app` back; moving that import above the line that creates `app` would break the application. Click’s loop between `_compat.py` and `_winconsole.py` exists only on Windows. These are exactly the fragile situations the check is meant to flag. The unused-files search reported nothing for four of the projects and 21 files for rich, all of them `rich/_unicode_data/unicode*.py`, which rich loads with `import_module` using a name built at run time. No static tool can see such imports, which is why the output asks the user to check before deleting and why `entry` rules exist."));

add(R.h3("5.2.3 Architecture rules"));
add(p("On the sample project, the layer rules report exactly one violation: `actions/move.py` imports `rules.py`, a higher layer (Figure 4.6). This is the same loop found by Tarjan’s algorithm, seen from the design. Interestingly, the rule points to a different import than the cycle fix (Figure 4.7): the fix removes as few links as possible and prefers `rules.py:4`, while the rule knows which direction the design intends. Both are correct answers to different questions. The program’s own `pdr.rules` passes; when a header of the lowest layer was made to include the analyser in a scratch copy, the check reported both the new cycle and the layer violation."));

add(R.h3("5.2.4 Performance"));
add(p("`make bench` generates projects shaped like real code (packages of 10 files, each file importing one to five others, one small circular import in every 20 packages, fixed random seed), times each algorithm and reports the median of several runs. All times were measured on the development machine of Table 3.1 with `g++ -O2`. Table 5.4 and Figure 5.2 show the graph algorithms."));
const B = { graph: [], hashmap: [], ranking: [], analyze: [] };
for (const line of fs.readFileSync(path.join(ROOT, "cpp", "build", "bench.csv"), "utf8").split("\n")) {
    if (!line || line.startsWith("#")) continue;
    const [k, ...v] = line.split(",");
    B[k].push(v.map(Number));
}
const n = (x) => x.toLocaleString("en-US");
const ms = (x) => (x < 10 ? x.toFixed(1) : n(Math.round(x)));
add(R.table("5.4", "Running time of the graph algorithms (milliseconds)", ["Files (V)", "Import links (E)", "Tarjan SCC", "Kahn + min-heap", "One BFS"],
    B.graph.map(([v, e, t, k, b]) => [n(v), n(e), ms(t), ms(k), ms(b)]), [0.2, 0.22, 0.18, 0.22, 0.18], { align: [AT.RIGHT, AT.RIGHT, AT.RIGHT, AT.RIGHT, AT.RIGHT] }));
add(R.figure(fig("bench-graph.png"), "5.2", "Running time of the graph algorithms against V + E"));
add(p("Tarjan’s algorithm and breadth-first search grow in step with V + E, as their O(V + E) bound predicts: one million files with 2.8 million links take about 0.3 s and 0.07 s. Kahn’s algorithm grows slightly faster than linearly and is the slowest of the three. Part of this is the log V factor of the heap; part is that the condensed graph keeps the neighbours of each unit in a balanced-tree set, which also costs O(log) per insertion. Replacing those sets with the project’s own hash set is a possible improvement."));
const h1m = B.hashmap.find((r) => r[0] === 1000000);
add(p(`Figures 5.3 and 5.4 compare the hand-written hash table with \`std::unordered_map\` on file-path keys. Lookups of n present and n absent keys are faster in the hand-written table (${ms(h1m[3])} ms against ${ms(h1m[4])} ms for a million keys), probably mainly because its bucket count is a power of two, so the bucket of a key is found with a bit mask, whereas the GNU implementation of \`std::unordered_map\` uses prime bucket counts and a division. Insertion is slightly slower (${ms(h1m[1])} ms against ${ms(h1m[2])} ms); both tables allocate one node per entry, and the cause of this small difference was not investigated further. Both curves are straight lines, as expected for O(1) operations repeated n times.`));
add(R.figure(fig("bench-hash-find.png"), "5.3", "Lookup time: hand-written hash table against `std::unordered_map`"));
add(R.figure(fig("bench-hash-insert.png"), "5.4", "Insertion time: hand-written hash table against `std::unordered_map`"));
add(R.table("5.5", "Critical-files ranking: one BFS per file against bitsets (milliseconds)", ["Files", "Import links", "One BFS per file", "Bitsets", "Speed-up"],
    B.ranking.map(([v, e, f, b]) => [n(v), n(e), f < 0 ? "not run" : ms(f), ms(b), f < 0 ? "—" : `${Math.round(f / b)}×`]), [0.18, 0.2, 0.24, 0.2, 0.18], { align: [AT.RIGHT, AT.RIGHT, AT.RIGHT, AT.RIGHT, AT.RIGHT] }));
add(p("Table 5.5 and Figure 5.5 show the effect of the bitset method of Section 3.4.6. It gives the same counts as one breadth-first search per file (test T16) between 22 and 33 times faster. Both methods are O(V · (V + E)) in the worst case; the bitsets divide the work by the 64 files held in a machine word, and the remaining gap to 64 is the work outside the word operations. The one-search-per-file method was not run beyond 20,000 files because it becomes too slow to be useful."));
add(R.figure(fig("bench-ranking.png"), "5.5", "Critical-files ranking: bitsets against one BFS per file"));
const a11 = B.analyze.find((r) => r[0] === 11000), aMax = B.analyze[B.analyze.length - 1];
add(p(`Figure 5.6 shows the whole analysis, from reading the imports to every algorithm, on generated Python projects. It takes ${ms(a11[2])} ms for ${n(a11[0])} files (366 ms before the bitset method) and ${(aMax[2] / 1000).toFixed(1)} s for ${n(aMax[0])} files. The growth is faster than linear because the ranking still grows with V · (V + E); for every project of realistic size the analysis finishes in well under a second.`));
add(R.figure(fig("bench-analyze.png"), "5.6", "Running time of the whole analysis"));

add(R.h3("5.2.5 Reliability"));
add(p("The program and its tests were also built with GCC’s AddressSanitizer and UndefinedBehaviorSanitizer, which stop the program at the first invalid memory access, memory leak or undefined operation. All 57 tests, every output mode on the five open-source projects, the program’s own source and a scripted menu session ran without a single report. This matters most for the hash table, whose linked lists are managed with raw pointers. Every push to GitHub runs the full test suite on Ubuntu, so the program is also checked with a second compiler set-up."));

add(R.h2("5.3 Complexity Analysis"));
add(p("V is the number of files, E the number of import links, C the number of characters read, L the length of a key and d the depth of a path. Table 5.6 lists the cost of each operation."));
add(R.table("5.6", "Time and space complexity of each operation", ["Operation", "Time complexity", "Space complexity"], [
    ["Reading imports and their timing", "O(C)", "O(C)"],
    ["Resolving one import (hash tables)", "O(L) on average", "O(V) for the indexes"],
    ["Hash table insert / find / erase", "O(1) average, O(n) worst", "O(n)"],
    ["Building the graph", "O(V + E · degree) (sorted insertion)", "O(V + E)"],
    ["Circular imports (Tarjan SCC)", "O(V + E)", "O(V)"],
    ["One concrete loop per component (BFS)", "O(V + E)", "O(V)"],
    ["Cycle fix for a component of k removals", "O(k · Ec · (Vc + Ec)) within the component", "O(Vc + Ec)"],
    ["Build order and layers (Kahn + min-heap)", "O(V log V + E log V)", "O(V + E)"],
    ["Why A needs B (BFS with parents)", "O(V + E)", "O(V)"],
    ["Impact of a change (BFS on reverse lists)", "O(V + E)", "O(V)"],
    ["Critical-files ranking (bitsets)", "O(V / 64 · (V + E))", "O(components · slice), at most 64 MB"],
    ["Unused files (multi-source BFS)", "O(V · d + E)", "O(V)"],
    ["Rule check (path trie)", "O(V · d + E · r), r = rules per file", "O(rule paths)"],
    ["File search (trie)", "O(L) plus the matches returned", "O(total length of names)"],
], [0.42, 0.33, 0.25], { size: 20 }));

// =====================================================================
// Chapter 6: Conclusion and Future Scope
// =====================================================================
add(R.chapterHeading(6, "Conclusion and Future Scope"));
add(R.h2("6.1 Conclusion"));
add(p("This project built Package Dependency Resolver, a C++17 program that models a software project as a directed graph of files and imports and explains the problems hidden in that graph. Against the objectives of Section 1.3:"));
add(R.numbered([
    "**Graph model.** Python, JavaScript/TypeScript and C/C++ projects are read into a graph of files and imports, with the line of every import. The program analyses its own source as well as the five open-source projects of Section 5.2.",
    "**Data structures.** Adjacency and reverse adjacency lists, a hash table with separate chaining, a binary min-heap, a character trie, a path trie and bitsets are implemented by hand with the standard library only, and each is tested, including the worst case of a hash table in which every key collides.",
    "**Circular imports.** Tarjan’s algorithm, run only on imports that execute while modules load, finds every circular import; each is shown as one concrete loop with its lines and a greedy, stability-aware suggestion of which imports to remove. On five working libraries this reduced eight reported loops to the two that are genuinely load-order dependent.",
    "**Order and impact.** Kahn’s algorithm with a min-heap gives a stable build order and layers, breadth-first search explains dependencies and the impact of a change, and bitsets made the critical-files ranking 22 to 33 times faster.",
    "**Rules, unused files and CI.** A rules file of layers, forbidden imports and entries is checked with a path trie; unused files are found with a multi-source search; every problem is printed in a form CI systems understand, with meaningful exit codes. The program checks its own architecture on every test run.",
    "**Testing and measurement.** 28 engine tests and 29 command-line tests pass on two operating systems, the program runs clean under memory-error checkers, and measurements on projects of up to one million files confirm the linear growth of the graph algorithms.",
]));
add(p("Beyond the individual algorithms, the project showed how much the choice of model matters: the same Tarjan’s algorithm gave eight false alarms or two true findings depending only on which imports were placed in the graph. Testing on real code that the program was not written for was what exposed this."));
add(R.h2("6.2 Future Scope"));
add(R.bullets([
    "**Package version resolution.** Reading `requirements.txt`, `pyproject.toml` and `package.json` to report packages that are imported but not declared, or declared but never used, and choosing package versions that satisfy all constraints with a backtracking search.",
    "**Graphical interface.** Bringing the C++-only features (C/C++ files, load-time cycles, cycle fixes, rules, unused files) to the browser version, for example by compiling the C++ engine to WebAssembly.",
    "**Exact C/C++ include paths.** Reading `compile_commands.json` to learn the real `-I` folders instead of matching path endings.",
    "**Very large projects.** Estimating the critical-files counts in near-linear time with Cohen’s method, and analysing only the files that changed since the last run.",
    "**More languages.** Java, Go and Rust use similar import statements and would reuse the whole graph engine.",
]));

// =====================================================================
// References (IEEE, in order of first citation)
// =====================================================================
const REFS = {
    tarjan: "R. Tarjan, “Depth-first search and linear graph algorithms,” *SIAM J. Comput.*, vol. 1, no. 2, pp. 146–160, Jun. 1972.",
    kahn: "A. B. Kahn, “Topological sorting of large networks,” *Commun. ACM*, vol. 5, no. 11, pp. 558–562, Nov. 1962.",
    clrs: "T. H. Cormen, C. E. Leiserson, R. L. Rivest, and C. Stein, *Introduction to Algorithms*, 4th ed. Cambridge, MA, USA: MIT Press, 2022.",
    madge: "“Madge: Create graphs from your CommonJS, AMD or ES6 module dependencies,” GitHub repository pahen/madge. [Online]. Available: https://github.com/pahen/madge (accessed Oct. 21, 2026).",
    depcruise: "“dependency-cruiser: Validate and visualize dependencies,” GitHub repository sverweij/dependency-cruiser. [Online]. Available: https://github.com/sverweij/dependency-cruiser (accessed Oct. 21, 2026).",
    pydeps: "“pydeps: Python module dependency visualization,” GitHub repository thebjorn/pydeps. [Online]. Available: https://github.com/thebjorn/pydeps (accessed Oct. 21, 2026).",
    importlinter: "“Import Linter documentation,” Read the Docs. [Online]. Available: https://import-linter.readthedocs.io (accessed Oct. 21, 2026).",
    lakos: "J. Lakos, *Large-Scale C++ Software Design*. Reading, MA, USA: Addison-Wesley, 1996.",
    iwyu: "“Include What You Use,” include-what-you-use.org. [Online]. Available: https://include-what-you-use.org (accessed Oct. 21, 2026).",
    martin: "R. C. Martin, *Clean Architecture: A Craftsman’s Guide to Software Structure and Design*. Boston, MA, USA: Prentice Hall, 2017.",
    karp: "R. M. Karp, “Reducibility among combinatorial problems,” in *Complexity of Computer Computations*, R. E. Miller and J. W. Thatcher, Eds. New York, NY, USA: Plenum, 1972, pp. 85–103.",
    eades: "P. Eades, X. Lin, and W. F. Smyth, “A fast and effective heuristic for the feedback arc set problem,” *Inf. Process. Lett.*, vol. 47, no. 6, pp. 319–323, 1993.",
    cohen: "E. Cohen, “Size-estimation framework with applications to transitive closure and reachability,” *J. Comput. Syst. Sci.*, vol. 55, no. 3, pp. 441–453, 1997.",
    knuth: "D. E. Knuth, *The Art of Computer Programming, Vol. 3: Sorting and Searching*, 2nd ed. Reading, MA, USA: Addison-Wesley, 1998.",
    fnv: "G. Fowler, L. C. Noll, and K.-P. Vo, “FNV hash,” isthe.com. [Online]. Available: http://www.isthe.com/chongo/tech/comp/fnv/ (accessed Oct. 21, 2026).",
    splitmix: "G. L. Steele Jr., D. Lea, and C. H. Flood, “Fast splittable pseudorandom number generators,” in *Proc. ACM OOPSLA*, 2014, pp. 453–472.",
    fredkin: "E. Fredkin, “Trie memory,” *Commun. ACM*, vol. 3, no. 9, pp. 490–499, Sep. 1960.",
    graphviz: "E. R. Gansner and S. C. North, “An open graph visualization system and its applications to software engineering,” *Softw. Pract. Exper.*, vol. 30, no. 11, pp. 1203–1233, 2000.",
    sugiyama: "K. Sugiyama, S. Tagawa, and M. Toda, “Methods for visual understanding of hierarchical system structures,” *IEEE Trans. Syst., Man, Cybern.*, vol. SMC-11, no. 2, pp. 109–125, Feb. 1981.",
    pyimport: "Python Software Foundation, “The import system,” *The Python Language Reference*. [Online]. Available: https://docs.python.org/3/reference/import.html (accessed Oct. 21, 2026).",
    claude: "Anthropic, “Claude Code,” claude.com. [Online]. Available: https://claude.com/claude-code (accessed Oct. 21, 2026).",
};
// cited outside the chapters, so they come last in order
c("pyimport");
c("claude");
const refs = [R.title("REFERENCES", "References")];
R.citeOrder.forEach((k, i) => {
    if (!REFS[k]) throw new Error(`no reference for ${k}`);
    refs.push(new D.Paragraph({ alignment: AT.JUSTIFIED, indent: { left: 567, hanging: 567 }, spacing: { after: 100 }, children: [new D.TextRun(`[${i + 1}]`), new D.TextRun({ children: [new D.Tab()] }), ...L.runs(REFS[k])], tabStops: [{ type: D.TabStopType.LEFT, position: 567 }] }));
});
for (const k of Object.keys(REFS)) if (!R.citeOrder.includes(k)) throw new Error(`reference ${k} is never cited`);

// =====================================================================
// Appendices
// =====================================================================
const appx = [R.title("APPENDICES", "Appendices")];
appx.push(R.appendixHeading("A", "Source Code"));
appx.push(p("The complete source code of the program follows, layer by layer from the bottom up, as in `cpp/pdr.rules`. The tests (`cpp/tests/`), the benchmark (`cpp/bench/`), the bundled sample and the browser prototype are in the submitted source archive and in the project repository ({{repository link or “available on request”}})."));
const FILES = [
    ["Build", ["../Makefile", "../pdr.rules"]],
    ["Helpers and data structures", ["util.h", "util.cpp", "hash_map.h", "min_heap.h"]],
    ["Parser, graph and tries", ["parser.h", "parser.cpp", "graph.h", "graph.cpp", "trie.h", "trie.cpp", "path_trie.h"]],
    ["Resolver, rules and algorithms", ["resolver.h", "resolver.cpp", "rules.h", "rules.cpp", "algorithms.h", "algorithms.cpp"]],
    ["Analysis", ["analyzer.h", "analyzer.cpp"]],
    ["Reports", ["json_report.h", "json_report.cpp", "dot_export.h", "dot_export.cpp"]],
    ["Command line", ["main.cpp"]],
];
for (const [module, files] of FILES) {
    for (const f of files) {
        const shown = f.startsWith("../") ? "cpp/" + f.slice(3) : "cpp/src/" + f;
        const text = fs.readFileSync(path.join(SRC, f), "utf8").replace(/\t/g, "    ").replace(/\n$/, "");
        appx.push(new D.Paragraph({ keepNext: true, spacing: { before: 240, after: 80 }, children: [new D.TextRun({ text: `File: ${shown}`, bold: true }), new D.TextRun({ text: `   (module: ${module})`, italics: true })] }));
        appx.push(...L.codeBlock(text.split("\n"), false));
    }
}

appx.push(R.appendixHeading("B", "User Manual"));
appx.push(R.h3("B.1 Building and installing"));
appx.push(p("On Linux or macOS with GCC or Clang and Make:"));
appx.push(...L.codeBlock(["cd cpp", "make            # builds build/pdr", "make test       # runs the 28 engine tests and 29 command-line tests", "make install    # copies pdr to ~/.local/bin, so it runs in any folder", "make bench      # benchmark and its SVG charts (about a minute)", "make dist       # the source zip (from a git checkout)", "make graph      # draws the sample and the program as SVG (needs Graphviz)"], false));
appx.push(p("Without Make, for example on Windows with MinGW: `g++ -std=c++17 -O2 -o pdr cpp/src/*.cpp`. Graphviz (`sudo pacman -S graphviz` on Arch Linux) is needed only to draw `--dot` output."));
appx.push(R.h3("B.2 Running"));
appx.push(p("Run `pdr` inside a project folder, or give the folder as the first argument. Files can be named by their file name (`main.py`) or by the end of their path (`utils/paths.py`)."));
appx.push(grid(["Command", "What it does"], [
    ["`pdr`", "Interactive menu (Figure 4.1)"],
    ["`pdr --report`", "The whole analysis as text"],
    ["`pdr --check`", "CI check: every circular import, broken import and rule violation; exit code 1 if any"],
    ["`--allow-cycles`, `--allow-broken`", "With `--check`: do not fail on that kind of problem"],
    ["`pdr --why A B`", "The shortest chain of imports by which A needs B"],
    ["`pdr --impact FILE`", "Every file that may break if FILE changes"],
    ["`pdr --search TEXT`", "Files whose name or path starts with TEXT"],
    ["`pdr --unused`", "Files that no script, test or entry point reaches"],
    ["`pdr --json`", "The whole analysis as JSON"],
    ["`pdr --dot`", "The graph for Graphviz: `pdr --dot | dot -Tsvg -o graph.svg`"],
    ["`--rules FILE`", "Read architecture rules from FILE instead of `pdr.rules`"],
    ["`pdr --help`", "Help text"],
], [0.38, 0.62], { size: 21 }));
appx.push(...blank(1));
appx.push(p("Exit codes: 0 when no problem is found, 1 when `--check` found problems, 2 for incorrect use or a folder that cannot be read or has no code."));
appx.push(R.h3("B.3 Architecture rules"));
appx.push(p("Create a file named `pdr.rules` in the project folder. Lines starting with `#` are comments."));
appx.push(...L.codeBlock(["# top layer first: a layer may import the layers below it, never one above", "layers  app/ui  app/services|app/jobs  app/db", "", "# nothing under app/db may import app/ui", "forbid  app/db -> app/ui", "", "# extra start points for --unused", "entry   scripts"], false));
appx.push(p("Paths are folders or files relative to the project folder. `pdr --check` then also fails on every import that breaks a rule, and warns about rule paths that match no file."));

appx.push(R.appendixHeading("C", "Plagiarism Report"));
appx.push(p("{{Attach the similarity report here.}}"));

appx.push(R.appendixHeading("D", "Weekly Progress Log and Team Contribution"));
appx.push(grid(["Week", "Dates", "Work planned", "Work completed", "Guide’s remarks", "Guide’s sign"],
    [1, 2, 3, 4].map((w) => [String(w), "{{..}}", "{{..}}", "{{..}}", "", ""]), [0.08, 0.14, 0.24, 0.24, 0.18, 0.12], { size: 21 }));
appx.push(...blank(1));
appx.push(centered("Team contribution", { italics: true, size: 22 }, { keepNext: true }));
appx.push(grid(["Student", "Modules coded", "Report chapters written"], [["{{Name}}", "All modules (solo project)", "All chapters"]], [0.34, 0.33, 0.33], { size: 22 }));

// =====================================================================
// Contents and lists (page numbers from the previous pass)
// =====================================================================
const tocPage = [tocTitle];
const entries = [
    { level: 0, text: "Certificate", key: "CERTIFICATE" },
    { level: 0, text: "Declaration", key: "DECLARATION" },
    { level: 0, text: "Acknowledgement", key: "ACKNOWLEDGEMENT" },
    { level: 0, text: "Abstract", key: "ABSTRACT" },
    { level: 0, text: "Table of Contents", key: "TABLE OF CONTENTS" },
    { level: 0, text: "List of Figures, List of Tables, List of Abbreviations", key: "LIST OF FIGURES" },
    ...R.toc.filter((e) => !["CERTIFICATE", "DECLARATION", "ACKNOWLEDGEMENT", "ABSTRACT", "LIST OF FIGURES"].includes(e.key)),
];
for (const e of entries) tocPage.push(tocLine(e.text, R.page(e.key), e.level));

const lists = [listsTitle];
lists.push(grid(["Figure No.", "Title of the figure", "Page"], R.figures.map((f) => [`Figure ${f.num}`, f.title, R.page(f.key)]), [0.16, 0.72, 0.12], { size: 21, align: [AT.CENTER, AT.LEFT, AT.CENTER] }));
lists.push(new D.Paragraph({ alignment: AT.CENTER, keepNext: true, spacing: { before: 360, after: 240 }, children: [new D.TextRun({ text: "LIST OF TABLES", bold: true, size: 32 })] }));
lists.push(grid(["Table No.", "Title of the table", "Page"], R.tables.map((t) => [`Table ${t.num}`, t.title, R.page(t.key)]), [0.16, 0.72, 0.12], { size: 21, align: [AT.CENTER, AT.LEFT, AT.CENTER] }));
lists.push(new D.Paragraph({ alignment: AT.CENTER, pageBreakBefore: true, spacing: { before: 0, after: 240 }, children: [new D.TextRun({ text: "LIST OF ABBREVIATIONS", bold: true, size: 32 })] }));
lists.push(grid(["Abbreviation", "Full form"], [
    ["BFS", "Breadth-First Search"], ["CI", "Continuous Integration"], ["CLI", "Command-Line Interface"],
    ["CSV", "Comma-Separated Values"], ["DAG", "Directed Acyclic Graph"], ["DFS", "Depth-First Search"],
    ["DOT", "The graph description language of Graphviz"], ["FNV", "Fowler–Noll–Vo (hash function)"],
    ["JS", "JavaScript"], ["JSON", "JavaScript Object Notation"], ["SCC", "Strongly Connected Component"],
    ["SDP", "Stable Dependencies Principle"], ["STL", "Standard Template Library"], ["SVG", "Scalable Vector Graphics"],
    ["TS", "TypeScript"], ["V, E", "Number of files (vertices) and import links (edges)"],
], [0.3, 0.7], { size: 22 }));

// =====================================================================
// Document
// =====================================================================
const header = new D.Header({
    children: [new D.Paragraph({
        border: { bottom: { style: D.BorderStyle.SINGLE, size: 6, color: "000000", space: 4 } },
        spacing: { after: 0, line: 240 },
        children: [new D.TextRun({ text: TITLE, italics: true, size: 20 }), new D.TextRun({ text: " | ", size: 20 }), ...L.runs("{{Department name}}", { size: 20 }), new D.TextRun({ text: ", ADYPU Pune", size: 20 })],
    })],
});
const footer = new D.Footer({ children: [new D.Paragraph({ alignment: AT.CENTER, children: [new D.TextRun({ children: [D.PageNumber.CURRENT], size: 22 })] })] });
const pageProps = (format) => ({
    page: { size: { width: L.PAGE.width, height: L.PAGE.height }, margin: { ...L.MARGIN, header: 708, footer: 708 }, pageNumbers: format ? { start: 1, formatType: format } : undefined },
});

const doc = new D.Document({
    creator: "{{Name}}",
    title: TITLE,
    description: "Mini project report, Data Structures using C++ (E0556B)",
    styles: {
        default: { document: { run: { font: L.FONT, size: 24 }, paragraph: { spacing: { line: 360, after: 120 } } } },
        paragraphStyles: [
            { id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true, run: { font: L.FONT, size: 32, bold: true }, paragraph: { outlineLevel: 0, alignment: AT.CENTER } },
            { id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true, run: { font: L.FONT, size: 28, bold: true }, paragraph: { outlineLevel: 1 } },
            { id: "Heading3", name: "Heading 3", basedOn: "Normal", next: "Normal", quickFormat: true, run: { font: L.FONT, size: 24, bold: true, italics: true }, paragraph: { outlineLevel: 2 } },
        ],
    },
    numbering: {
        config: [
            { reference: "bullets", levels: [{ level: 0, format: D.LevelFormat.BULLET, text: "•", alignment: AT.LEFT, style: { paragraph: { indent: { left: 567, hanging: 284 } } } }] },
            { reference: "numbers", levels: [{ level: 0, format: D.LevelFormat.DECIMAL, text: "%1.", alignment: AT.LEFT, style: { paragraph: { indent: { left: 567, hanging: 340 } } } }] },
        ],
    },
    sections: [
        { properties: pageProps(null), children: cover },
        { properties: pageProps(D.NumberFormat.LOWER_ROMAN), headers: { default: header }, footers: { default: footer }, children: [...front, ...tocPage, ...lists] },
        { properties: pageProps(D.NumberFormat.DECIMAL), headers: { default: header }, footers: { default: footer }, children: [...body, ...refs, ...appx] },
    ],
});

// what build.sh needs to find every entry in the rendered PDF
const index = {
    entries: entries.map((e) => e.key),
    figures: R.figures.map((f) => f.key),
    tables: R.tables.map((t) => t.key),
};
fs.mkdirSync(path.dirname(OUT), { recursive: true });
fs.writeFileSync(path.join(__dirname, "build", "index.json"), JSON.stringify(index, null, 1));
// The docx library writes <w:highlightCs> next to every highlight; the Word schema
// does not allow it there, so it is removed before saving.
const JSZip = require("jszip");
D.Packer.toBuffer(doc)
    .then((buf) => JSZip.loadAsync(buf))
    .then(async (zip) => {
        for (const name of Object.keys(zip.files).filter((n) => /^word\/(document|header\d*|footer\d*)\.xml$/.test(n))) {
            const xml = await zip.file(name).async("string");
            zip.file(name, xml.replace(/<w:highlightCs\b[^>]*\/>/g, ""));
        }
        return zip.generateAsync({ type: "nodebuffer", compression: "DEFLATE" });
    })
    .then((buf) => {
        fs.writeFileSync(OUT, buf);
        console.log(`wrote ${OUT}`);
    });

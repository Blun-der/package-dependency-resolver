/*
 * Package Dependency Resolver — analysis engine
 * ---------------------------------------------
 * Pure JavaScript, no dependencies. Runs in the browser and in Node (for tests).
 *
 * Pipeline:  files -> parse imports -> resolve to files -> graph
 *            graph -> Tarjan SCC (cycles) -> Kahn + min-heap (build order)
 *            graph -> BFS (paths, impact) ; Trie (file search)
 */
(function (root) {
  'use strict';

  // ---------------------------------------------------------------------------
  // 1. Files and languages
  // ---------------------------------------------------------------------------

  const IGNORED_DIRS = new Set([
    'node_modules', '.git', '__pycache__', 'venv', '.venv', 'env', '.env',
    'dist', 'build', '.next', 'coverage', '.idea', '.vscode', 'site-packages',
    '.mypy_cache', '.pytest_cache', '.tox', 'out', '.cache', 'egg-info'
  ]);
  const JS_EXT = ['.js', '.jsx', '.ts', '.tsx', '.mjs', '.cjs'];

  // Built-in modules, so externals can be split into "standard library"
  // and "third-party packages you must install".
  const PY_STDLIB = new Set((
    '__future__ abc argparse array ast asyncio atexit base64 bisect builtins bz2 calendar cmath cmd ' +
    'codecs collections colorsys concurrent configparser contextlib contextvars copy copyreg csv ctypes ' +
    'curses dataclasses datetime dbm decimal difflib dis doctest email encodings enum errno faulthandler ' +
    'fcntl filecmp fileinput fnmatch fractions ftplib functools gc getopt getpass gettext glob graphlib ' +
    'grp gzip hashlib heapq hmac html http imaplib importlib inspect io ipaddress itertools json keyword ' +
    'linecache locale logging lzma mailbox marshal math mimetypes mmap multiprocessing netrc numbers ' +
    'operator optparse os pathlib pdb pickle pkgutil platform plistlib poplib posix pprint profile pstats ' +
    'pty pwd py_compile queue quopri random re readline reprlib resource rlcompleter runpy sched secrets ' +
    'select selectors shelve shlex shutil signal site smtplib socket socketserver sqlite3 ssl stat ' +
    'statistics string stringprep struct subprocess symtable sys sysconfig syslog tabnanny tarfile ' +
    'tempfile termios textwrap threading time timeit tkinter token tokenize tomllib trace traceback ' +
    'tracemalloc tty turtle types typing unicodedata unittest urllib uuid venv warnings wave weakref ' +
    'webbrowser winreg winsound wsgiref xml xmlrpc zipapp zipfile zipimport zlib zoneinfo _thread'
  ).split(' '));
  const NODE_BUILTINS = new Set((
    'assert async_hooks buffer child_process cluster console constants crypto dgram diagnostics_channel ' +
    'dns domain events fs http http2 https inspector module net os path perf_hooks process punycode ' +
    'querystring readline repl stream string_decoder sys timers tls trace_events tty url util v8 vm ' +
    'wasi worker_threads zlib test'
  ).split(' '));

  function isBuiltin(lang, name) {
    return lang === 'py' ? PY_STDLIB.has(name) : NODE_BUILTINS.has(name.replace(/^node:/, '').split('/')[0]);
  }

  function langOf(path) {
    const m = /\.[^./]+$/.exec(path);
    if (!m) return null;
    const ext = m[0].toLowerCase();
    if (ext === '.py') return 'py';
    if (JS_EXT.includes(ext)) return 'js';
    return null;
  }

  function isIgnored(path) {
    return path.split('/').some(p => IGNORED_DIRS.has(p) || p.endsWith('.egg-info'));
  }

  function dirOf(path) {
    const i = path.lastIndexOf('/');
    return i < 0 ? '' : path.slice(0, i);
  }

  function joinPath(a, b) {
    return a ? (b ? a + '/' + b : a) : b;
  }

  // Normalise "a/./b/../c" -> "a/c". Returns null if it climbs above the root.
  function normalize(path) {
    const out = [];
    for (const part of path.split('/')) {
      if (part === '' || part === '.') continue;
      if (part === '..') {
        if (!out.length) return null;
        out.pop();
      } else out.push(part);
    }
    return out.join('/');
  }

  function lineAt(text, index) {
    let n = 1;
    for (let i = 0; i < index; i++) if (text.charCodeAt(i) === 10) n++;
    return n;
  }

  // ---------------------------------------------------------------------------
  // 2. Import parsing
  //    Each import -> { kind, spec, names, level, line, text }
  // ---------------------------------------------------------------------------

  // Blank out comments and string literals but keep every newline, so line
  // numbers stay correct and "import" inside a docstring is not counted.
  function maskPython(src) {
    let out = '';
    let i = 0;
    const n = src.length;
    while (i < n) {
      const c = src[i];
      if (c === '#') {
        while (i < n && src[i] !== '\n') { out += ' '; i++; }
        continue;
      }
      if (c === '"' || c === "'") {
        const triple = src.substr(i, 3) === c + c + c;
        const q = triple ? c + c + c : c;
        out += ' '.repeat(q.length);
        i += q.length;
        while (i < n) {
          if (src[i] === '\\') { out += '  '; i += 2; continue; }
          if (src.substr(i, q.length) === q) { out += ' '.repeat(q.length); i += q.length; break; }
          if (!triple && src[i] === '\n') break; // unterminated single-line string
          out += src[i] === '\n' ? '\n' : ' ';
          i++;
        }
        continue;
      }
      out += c;
      i++;
    }
    return out;
  }

  function parsePython(src) {
    const text = maskPython(src);
    const lines = src.split('\n');
    const imports = [];

    // import a.b.c, d as e
    const reImport = /^[ \t]*import[ \t]+([\w.]+(?:[ \t]+as[ \t]+\w+)?(?:[ \t]*,[ \t]*[\w.]+(?:[ \t]+as[ \t]+\w+)?)*)/gm;
    let m;
    while ((m = reImport.exec(text))) {
      const line = lineAt(text, m.index);
      for (const part of m[1].split(',')) {
        const spec = part.trim().split(/\s+/)[0];
        if (spec) imports.push({ kind: 'import', spec, names: [], level: 0, line, text: (lines[line - 1] || '').trim() });
      }
    }

    // from .a.b import (x, y)   /   from . import x
    const reFrom = /^[ \t]*from[ \t]+(\.*)([\w.]*)[ \t]+import[ \t]+(\([^)]*\)|[^\n]*)/gm;
    while ((m = reFrom.exec(text))) {
      const line = lineAt(text, m.index);
      const names = m[3].replace(/[()\\]/g, ' ')
        .split(',')
        .map(s => s.trim().split(/\s+/)[0])
        .filter(s => s && s !== '*');
      imports.push({
        kind: 'from', spec: m[2], names, level: m[1].length, line,
        text: (lines[line - 1] || '').trim()
      });
    }
    return imports.sort((a, b) => a.line - b.line);
  }

  // Remove JS comments (keeps newlines). With blankStrings, string contents are
  // blanked too, which tells us whether a keyword sits in real code.
  function maskJs(src, blankStrings) {
    let out = '';
    let i = 0;
    const n = src.length;
    while (i < n) {
      const c = src[i];
      const d = src[i + 1];
      if (c === '/' && d === '/') {
        while (i < n && src[i] !== '\n') { out += ' '; i++; }
        continue;
      }
      if (c === '/' && d === '*') {
        out += '  '; i += 2;
        while (i < n && !(src[i] === '*' && src[i + 1] === '/')) { out += src[i] === '\n' ? '\n' : ' '; i++; }
        out += '  '; i += 2;
        continue;
      }
      if (c === '"' || c === "'" || c === '`') {
        out += c; i++;
        while (i < n && src[i] !== c) {
          if (src[i] === '\\') { out += blankStrings ? '  ' : src[i] + (src[i + 1] || ''); i += 2; continue; }
          if (src[i] === '\n' && c !== '`') break;
          out += blankStrings && src[i] !== '\n' ? ' ' : src[i]; i++;
        }
        if (i < n) { out += src[i]; i++; }
        continue;
      }
      out += c; i++;
    }
    return out;
  }

  function parseJs(src) {
    const text = maskJs(src, false);
    const code = maskJs(src, true);
    const lines = src.split('\n');
    const found = new Map(); // index -> import (dedupe overlapping regexes)
    const patterns = [
      /\bimport\s+(?:type\s+)?(?:[\w$*{}\s,]+?\s+from\s+)?['"]([^'"\n]+)['"]/g,
      /\bexport\s+(?:type\s+)?(?:\*(?:\s+as\s+[\w$]+)?|\{[^}]*\})\s+from\s+['"]([^'"\n]+)['"]/g,
      /\brequire\s*\(\s*['"]([^'"\n]+)['"]\s*\)/g,
      /\bimport\s*\(\s*['"]([^'"\n]+)['"]\s*\)/g
    ];
    for (const re of patterns) {
      let m;
      while ((m = re.exec(text))) {
        const at = m.index + m[0].lastIndexOf(m[1]);
        if (found.has(at)) continue;
        // keyword must be real code, not text inside a string literal
        if (code.slice(m.index, m.index + 6) !== text.slice(m.index, m.index + 6)) continue;
        const line = lineAt(text, at);
        found.set(at, { kind: 'import', spec: m[1], names: [], level: 0, line, text: (lines[line - 1] || '').trim() });
      }
    }
    return [...found.values()].sort((a, b) => a.line - b.line);
  }

  function parseImports(path, src) {
    const lang = langOf(path);
    if (lang === 'py') return parsePython(src);
    if (lang === 'js') return parseJs(src);
    return [];
  }

  // ---------------------------------------------------------------------------
  // 3. Resolution: import spec -> file in the project (hash map lookups)
  // ---------------------------------------------------------------------------

  function buildPythonIndex(paths, fileSet) {
    // A "source root" is a folder Python would put on sys.path: the project
    // root plus any folder with .py files that is NOT a package (no __init__.py).
    const pyFiles = paths.filter(p => langOf(p) === 'py');
    const roots = new Set(['']);
    for (const p of pyFiles) {
      const d = dirOf(p);
      if (d && !fileSet.has(d + '/__init__.py')) roots.add(d);
      // The folder that holds a top-level package is also a root ("src/" layout).
      if (p.endsWith('__init__.py')) {
        const parent = dirOf(d);
        if (d && !fileSet.has(joinPath(parent, '__init__.py'))) roots.add(parent);
      }
    }
    const sortedRoots = [...roots].sort((a, b) => a.split('/').length - b.split('/').length || a.localeCompare(b));
    const index = new Map(); // dotted module name -> file
    for (const r of sortedRoots) {
      for (const p of pyFiles) {
        if (r && !p.startsWith(r + '/')) continue;
        let rel = r ? p.slice(r.length + 1) : p;
        rel = rel.replace(/\.py$/, '').replace(/\/__init__$/, '').replace(/^__init__$/, '');
        if (!rel) continue;
        const dotted = rel.split('/').join('.');
        if (!index.has(dotted)) index.set(dotted, p);
      }
    }
    return index;
  }

  function pyFileAt(dir, dotted, fileSet) {
    const base = joinPath(dir, dotted ? dotted.split('.').join('/') : '');
    if (base && fileSet.has(base + '.py')) return base + '.py';
    const init = joinPath(base, '__init__.py');
    if (fileSet.has(init)) return init;
    return null;
  }

  function lookupPyModule(dotted, fromFile, ctx) {
    // Script folder first (Python puts the running script's folder on sys.path),
    // then the project-wide index.
    return pyFileAt(dirOf(fromFile), dotted, ctx.fileSet) || ctx.pyIndex.get(dotted) || null;
  }

  // Returns [{ target, external, broken }]
  function resolvePython(imp, fromFile, ctx) {
    const results = [];
    if (imp.level > 0) {
      // Relative import: climb (level - 1) folders from the file's own package.
      let base = dirOf(fromFile);
      for (let k = 1; k < imp.level; k++) {
        if (base === '') { base = null; break; }
        base = dirOf(base);
      }
      const label = '.'.repeat(imp.level) + imp.spec;
      if (base === null) return [{ broken: label }];
      if (imp.spec) {
        let hitSub = false;
        for (const name of imp.names) {
          const sub = pyFileAt(base, imp.spec + '.' + name, ctx.fileSet);
          if (sub) { results.push({ target: sub }); hitSub = true; }
        }
        if (!hitSub) {
          const mod = pyFileAt(base, imp.spec, ctx.fileSet);
          results.push(mod ? { target: mod } : { broken: label });
        }
      } else {
        // from . import a, b
        for (const name of imp.names) {
          const sub = pyFileAt(base, name, ctx.fileSet);
          if (sub) results.push({ target: sub });
          else {
            const init = pyFileAt(base, '', ctx.fileSet);
            results.push(init ? { target: init } : { broken: '.'.repeat(imp.level) + name });
          }
        }
      }
      return results;
    }

    // Absolute import
    const parts = imp.spec.split('.');
    if (imp.kind === 'from') {
      let hitSub = false;
      for (const name of imp.names) {
        const sub = lookupPyModule(imp.spec + '.' + name, fromFile, ctx);
        if (sub) { results.push({ target: sub }); hitSub = true; }
      }
      if (hitSub) return results;
    }
    for (let k = parts.length; k > 0; k--) {
      const hit = lookupPyModule(parts.slice(0, k).join('.'), fromFile, ctx);
      if (hit) return [{ target: hit }];
    }
    return [{ external: parts[0] }];
  }

  function resolveJs(imp, fromFile, ctx) {
    const spec = imp.spec;
    const tryPaths = base => {
      if (base === null) return null;
      const cands = [base];
      for (const e of JS_EXT) cands.push(base + e);
      for (const e of JS_EXT) cands.push(joinPath(base, 'index' + e));
      // "./x.js" written in TypeScript source but the file is x.ts
      const stripped = base.replace(/\.(m|c)?js$/, '');
      if (stripped !== base) for (const e of JS_EXT) cands.push(stripped + e);
      return cands.find(c => ctx.fileSet.has(c)) || null;
    };
    if (spec.startsWith('.') || spec.startsWith('/')) {
      const base = spec.startsWith('/') ? normalize(spec) : normalize(joinPath(dirOf(fromFile), spec));
      const hit = tryPaths(base);
      return [hit ? { target: hit } : { broken: spec }];
    }
    if (spec.startsWith('@/') || spec.startsWith('~/')) {
      const rest = spec.slice(2);
      const hit = tryPaths(normalize('src/' + rest)) || tryPaths(normalize(rest));
      return [hit ? { target: hit } : { broken: spec }];
    }
    let pkg = spec.replace(/^node:/, '');
    pkg = pkg.startsWith('@') ? pkg.split('/').slice(0, 2).join('/') : pkg.split('/')[0];
    return [{ external: pkg }];
  }

  // ---------------------------------------------------------------------------
  // 4. Data structures
  // ---------------------------------------------------------------------------

  // Directed graph stored as adjacency lists (Map of Maps).
  // adj.get(A).get(B) = evidence that A imports B.  radj is the reverse graph.
  class Graph {
    constructor() {
      this.adj = new Map();
      this.radj = new Map();
    }
    addNode(id) {
      if (!this.adj.has(id)) { this.adj.set(id, new Map()); this.radj.set(id, new Map()); }
    }
    addEdge(from, to, evidence) {
      this.addNode(from); this.addNode(to);
      if (!this.adj.get(from).has(to)) {
        this.adj.get(from).set(to, evidence);
        this.radj.get(to).set(from, evidence);
      }
    }
    nodes() { return [...this.adj.keys()]; }
    out(id) { return [...(this.adj.get(id) || new Map()).keys()]; }
    in(id) { return [...(this.radj.get(id) || new Map()).keys()]; }
    edge(from, to) { return this.adj.get(from) ? this.adj.get(from).get(to) : undefined; }
    get edgeCount() { let e = 0; for (const m of this.adj.values()) e += m.size; return e; }
  }

  // Binary min-heap, used by Kahn's algorithm to pick the next ready item
  // in a stable, alphabetical order.
  class MinHeap {
    constructor(less) { this.a = []; this.less = less; }
    get size() { return this.a.length; }
    push(x) {
      const a = this.a; a.push(x);
      let i = a.length - 1;
      while (i > 0) {
        const p = (i - 1) >> 1;
        if (!this.less(a[i], a[p])) break;
        [a[i], a[p]] = [a[p], a[i]]; i = p;
      }
    }
    pop() {
      const a = this.a;
      const top = a[0];
      const last = a.pop();
      if (a.length) {
        a[0] = last;
        let i = 0;
        for (;;) {
          const l = 2 * i + 1, r = l + 1;
          let m = i;
          if (l < a.length && this.less(a[l], a[m])) m = l;
          if (r < a.length && this.less(a[r], a[m])) m = r;
          if (m === i) break;
          [a[i], a[m]] = [a[m], a[i]]; i = m;
        }
      }
      return top;
    }
  }

  // Trie over lower-case search keys (file name and full path) for prefix search.
  class Trie {
    constructor() { this.root = { kids: new Map(), ids: new Set() }; }
    insert(key, id) {
      let node = this.root;
      for (const ch of key.toLowerCase()) {
        if (!node.kids.has(ch)) node.kids.set(ch, { kids: new Map(), ids: new Set() });
        node = node.kids.get(ch);
        node.ids.add(id); // every prefix node remembers the ids below it
      }
    }
    search(prefix, limit = 20) {
      let node = this.root;
      for (const ch of prefix.toLowerCase()) {
        node = node.kids.get(ch);
        if (!node) return [];
      }
      return [...node.ids].slice(0, limit);
    }
  }

  // ---------------------------------------------------------------------------
  // 5. Algorithms
  // ---------------------------------------------------------------------------

  // Tarjan's strongly connected components, iterative (no recursion limit).
  // Every SCC with 2+ files is a group of files that import each other in a loop.
  function tarjanSCC(graph) {
    let counter = 0;
    const idx = new Map(), low = new Map(), onStack = new Set(), stack = [], sccs = [];
    const nodes = graph.nodes().sort();
    for (const s of nodes) {
      if (idx.has(s)) continue;
      const work = [[s, graph.out(s).sort()[Symbol.iterator]()]];
      idx.set(s, counter); low.set(s, counter); counter++;
      stack.push(s); onStack.add(s);
      while (work.length) {
        const frame = work[work.length - 1];
        const v = frame[0];
        const step = frame[1].next();
        if (!step.done) {
          const w = step.value;
          if (!idx.has(w)) {
            idx.set(w, counter); low.set(w, counter); counter++;
            stack.push(w); onStack.add(w);
            work.push([w, graph.out(w).sort()[Symbol.iterator]()]);
          } else if (onStack.has(w)) {
            low.set(v, Math.min(low.get(v), idx.get(w)));
          }
        } else {
          work.pop();
          if (work.length) {
            const u = work[work.length - 1][0];
            low.set(u, Math.min(low.get(u), low.get(v)));
          }
          if (low.get(v) === idx.get(v)) {
            const comp = [];
            let w;
            do { w = stack.pop(); onStack.delete(w); comp.push(w); } while (w !== v);
            sccs.push(comp.sort());
          }
        }
      }
    }
    return sccs;
  }

  // Shortest path with BFS. `allowed` optionally restricts the nodes visited.
  function shortestPath(graph, from, to, allowed) {
    if (!graph.adj.has(from) || !graph.adj.has(to)) return null;
    const parent = new Map([[from, null]]);
    const queue = [from];
    for (let head = 0; head < queue.length; head++) {
      const v = queue[head];
      for (const w of graph.out(v).sort()) {
        if (allowed && !allowed.has(w)) continue;
        if (w === to && v !== to) {
          const path = [w];
          for (let x = v; x !== null; x = parent.get(x)) path.push(x);
          return path.reverse();
        }
        if (!parent.has(w)) { parent.set(w, v); queue.push(w); }
      }
    }
    return null;
  }

  // One concrete loop inside a strongly connected component: start -> ... -> start
  function cycleIn(graph, comp) {
    const allowed = new Set(comp);
    const start = comp[0];
    return shortestPath(graph, start, start, allowed);
  }

  // All nodes reachable from start (BFS). With reverse=true, follows importers.
  function reachable(graph, start, reverse) {
    const seen = new Set([start]);
    const queue = [start];
    for (let head = 0; head < queue.length; head++) {
      const v = queue[head];
      for (const w of (reverse ? graph.in(v) : graph.out(v))) {
        if (!seen.has(w)) { seen.add(w); queue.push(w); }
      }
    }
    seen.delete(start);
    return seen;
  }

  // Build order with Kahn's algorithm on the condensed graph (each cycle
  // collapsed into one unit). Dependencies come before the files that use them.
  // Also returns each unit's layer: 0 = imports nothing, k = 1 + max(layer of deps).
  function buildOrder(graph, sccs) {
    const compOf = new Map();
    sccs.forEach((c, i) => c.forEach(f => compOf.set(f, i)));
    const deps = sccs.map(() => new Set());       // unit -> units it depends on
    const dependents = sccs.map(() => new Set()); // unit -> units depending on it
    for (const a of graph.nodes()) {
      for (const b of graph.out(a)) {
        const ca = compOf.get(a), cb = compOf.get(b);
        if (ca !== cb) { deps[ca].add(cb); dependents[cb].add(ca); }
      }
    }
    const remaining = deps.map(s => s.size);
    const layer = sccs.map(() => 0);
    const heap = new MinHeap((x, y) => sccs[x][0] < sccs[y][0]);
    remaining.forEach((n, i) => { if (n === 0) heap.push(i); });
    const order = [];
    while (heap.size) {
      const c = heap.pop();
      order.push({ files: sccs[c], cyclic: sccs[c].length > 1, layer: layer[c] });
      for (const d of dependents[c]) {
        layer[d] = Math.max(layer[d], layer[c] + 1);
        if (--remaining[d] === 0) heap.push(d);
      }
    }
    const layerOf = new Map();
    sccs.forEach((c, i) => c.forEach(f => layerOf.set(f, layer[i])));
    return { order, layerOf, compOf };
  }

  // ---------------------------------------------------------------------------
  // 6. Full analysis
  // ---------------------------------------------------------------------------

  // files: [{ path, content }]  (paths use "/" and are relative to the project)
  // options.stripRoot: drop the folder name the browser puts in front of every
  // path when a whole folder is picked ("myproject/a.py" -> "a.py").
  function analyze(files, options = {}) {
    const t0 = Date.now();
    const clean = files
      .map(f => ({ path: f.path.replace(/\\/g, '/').replace(/^\.\//, ''), content: f.content || '' }))
      .filter(f => langOf(f.path) && !isIgnored(f.path));

    const firsts = new Set(clean.map(f => f.path.split('/')[0]));
    let rootName = '';
    if (options.stripRoot && clean.length && firsts.size === 1 && clean.every(f => f.path.includes('/'))) {
      rootName = [...firsts][0];
      clean.forEach(f => { f.path = f.path.slice(rootName.length + 1); });
    }

    const paths = clean.map(f => f.path).sort();
    const fileSet = new Set(paths);
    const ctx = { fileSet, pyIndex: buildPythonIndex(paths, fileSet) };

    const graph = new Graph();
    const fileInfo = new Map();
    const externals = new Map(); // package -> Set(files)
    const broken = [];
    let importCount = 0;

    for (const f of clean) graph.addNode(f.path);

    for (const f of clean) {
      const lang = langOf(f.path);
      const imports = parseImports(f.path, f.content);
      importCount += imports.length;
      const info = {
        path: f.path, lang,
        lines: f.content ? f.content.split('\n').length : 0,
        imports: [], externals: new Set()
      };
      for (const imp of imports) {
        const res = lang === 'py' ? resolvePython(imp, f.path, ctx) : resolveJs(imp, f.path, ctx);
        for (const r of res) {
          const ev = { line: imp.line, text: imp.text };
          if (r.target) {
            if (r.target === f.path) continue; // package importing itself
            graph.addEdge(f.path, r.target, ev);
            info.imports.push({ type: 'file', target: r.target, line: imp.line, text: imp.text });
          } else if (r.external) {
            if (!externals.has(r.external)) {
              externals.set(r.external, { files: new Set(), builtin: isBuiltin(lang, r.external), lang });
            }
            externals.get(r.external).files.add(f.path);
            info.externals.add(r.external);
            info.imports.push({ type: 'external', target: r.external, line: imp.line, text: imp.text });
          } else if (r.broken) {
            broken.push({ file: f.path, spec: r.broken, line: imp.line, text: imp.text });
            info.imports.push({ type: 'broken', target: r.broken, line: imp.line, text: imp.text });
          }
        }
      }
      fileInfo.set(f.path, info);
    }

    const sccs = tarjanSCC(graph);
    const cycles = sccs
      .filter(c => c.length > 1)
      .map(c => {
        const loop = cycleIn(graph, c);
        const steps = [];
        for (let i = 0; loop && i < loop.length - 1; i++) {
          const ev = graph.edge(loop[i], loop[i + 1]);
          steps.push({ from: loop[i], to: loop[i + 1], line: ev.line, text: ev.text });
        }
        return { files: c, loop, steps };
      })
      .sort((a, b) => b.files.length - a.files.length || a.files[0].localeCompare(b.files[0]));

    const { order, layerOf, compOf } = buildOrder(graph, sccs);

    const metrics = new Map();
    for (const id of graph.nodes()) {
      metrics.set(id, {
        fanOut: graph.out(id).length,
        fanIn: graph.in(id).length,
        dependents: reachable(graph, id, true).size,
        dependencies: reachable(graph, id, false).size,
        layer: layerOf.get(id),
        inCycle: sccs[compOf.get(id)].length > 1
      });
    }

    const critical = graph.nodes()
      .filter(id => metrics.get(id).dependents > 0)
      .sort((a, b) => metrics.get(b).dependents - metrics.get(a).dependents ||
        metrics.get(b).fanIn - metrics.get(a).fanIn || a.localeCompare(b));

    const entryPoints = graph.nodes()
      .filter(id => metrics.get(id).fanIn === 0)
      .sort();

    const trie = new Trie();
    for (const id of graph.nodes()) {
      const base = id.split('/').pop();
      trie.insert(base, id);
      trie.insert(id, id);
    }

    const externalList = [...externals.entries()]
      .map(([name, e]) => ({ name, files: [...e.files].sort(), builtin: e.builtin, lang: e.lang }))
      .sort((a, b) => a.builtin - b.builtin || b.files.length - a.files.length || a.name.localeCompare(b.name));

    const maxLayer = order.reduce((m, u) => Math.max(m, u.layer), 0);
    const langs = { py: 0, js: 0 };
    for (const f of clean) langs[langOf(f.path)]++;

    return {
      rootName, graph, fileInfo, metrics, cycles, order, critical, entryPoints,
      externals: externalList, broken, trie, langs,
      stats: {
        files: graph.adj.size,
        edges: graph.edgeCount,
        importStatements: importCount,
        cycles: cycles.length,
        filesInCycles: cycles.reduce((s, c) => s + c.files.length, 0),
        externals: externalList.length,
        thirdParty: externalList.filter(e => !e.builtin).length,
        broken: broken.length,
        layers: graph.adj.size ? maxLayer + 1 : 0,
        ms: Date.now() - t0
      }
    };
  }

  // Why does `from` depend on `to`? Returns steps with the import line for each hop.
  function explainPath(result, from, to) {
    const path = shortestPath(result.graph, from, to);
    if (!path) return null;
    const steps = [];
    for (let i = 0; i < path.length - 1; i++) {
      const ev = result.graph.edge(path[i], path[i + 1]);
      steps.push({ from: path[i], to: path[i + 1], line: ev.line, text: ev.text });
    }
    return { path, steps };
  }

  function impactOf(result, file) {
    return [...reachable(result.graph, file, true)].sort();
  }

  const api = {
    analyze, explainPath, impactOf,
    parseImports, parsePython, parseJs,
    Graph, MinHeap, Trie, tarjanSCC, buildOrder, shortestPath, reachable,
    langOf, isIgnored, normalize, isBuiltin
  };

  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.PDR = api;
})(typeof self !== 'undefined' ? self : this);

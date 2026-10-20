// Run with:  node --test test/
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const PDR = require('../src/engine.js');

const F = (p, c) => ({ path: p, content: c });

function loadDir(dir) {
  const out = [];
  (function walk(d) {
    for (const e of fs.readdirSync(d, { withFileTypes: true })) {
      const full = path.join(d, e.name);
      if (e.isDirectory()) walk(full);
      else out.push(F(path.relative(dir, full).split(path.sep).join('/'), fs.readFileSync(full, 'utf8')));
    }
  })(dir);
  return out;
}

// ---------- parsing ----------

test('python: import forms, aliases and line numbers', () => {
  const imps = PDR.parsePython([
    'import os, sys as system',
    'from a.b import (c,',
    '    d)',
    'from . import x',
    'from ..pkg import y as z',
    '# import commented',
    '"""import inside docstring"""',
    'def f():',
    '    import lazy'
  ].join('\n'));
  assert.deepEqual(imps.map(i => [i.spec, i.level, i.line]), [
    ['os', 0, 1], ['sys', 0, 1], ['a.b', 0, 2], ['', 1, 4], ['pkg', 2, 5], ['lazy', 0, 9]
  ]);
  assert.deepEqual(imps[2].names, ['c', 'd']);
  assert.deepEqual(imps[4].names, ['y']);
});

test('js: import, export-from, require, dynamic import, comments ignored', () => {
  const imps = PDR.parseJs([
    "import React, { useState } from 'react';",
    "import './styles.css';",
    "export { a } from './a';",
    "const fs = require('fs');",
    "// import x from './commented';",
    "const lazy = () => import('./lazy');",
    "import type { T } from './types';"
  ].join('\n'));
  assert.deepEqual(imps.map(i => i.spec), ['react', './styles.css', './a', 'fs', './lazy', './types']);
});

test('js: import text inside string literals is ignored', () => {
  const imps = PDR.parseJs([
    "const example = \"import x from './not-real'\";",
    "const tpl = `require('also-not-real')`;",
    "import real from './real';"
  ].join('\n'));
  assert.deepEqual(imps.map(i => i.spec), ['./real']);
});

// ---------- data structures ----------

test('min-heap returns items in order', () => {
  const h = new PDR.MinHeap((a, b) => a < b);
  [5, 1, 4, 2, 3, 0].forEach(x => h.push(x));
  const out = [];
  while (h.size) out.push(h.pop());
  assert.deepEqual(out, [0, 1, 2, 3, 4, 5]);
});

test('trie prefix search', () => {
  const t = new PDR.Trie();
  t.insert('rules.py', 'fad/rules.py');
  t.insert('rename.py', 'fad/actions/rename.py');
  t.insert('daemon.py', 'fad/daemon.py');
  assert.deepEqual(t.search('r').sort(), ['fad/actions/rename.py', 'fad/rules.py']);
  assert.deepEqual(t.search('ru'), ['fad/rules.py']);
  assert.deepEqual(t.search('zzz'), []);
});

// ---------- graph algorithms ----------

function graphOf(edges) {
  const g = new PDR.Graph();
  for (const [a, b] of edges) g.addEdge(a, b, { line: 1, text: '' });
  return g;
}

test('tarjan finds cycles and singletons', () => {
  const g = graphOf([['a', 'b'], ['b', 'c'], ['c', 'a'], ['c', 'd'], ['e', 'd']]);
  const sccs = PDR.tarjanSCC(g).map(c => c.join(',')).sort();
  assert.deepEqual(sccs, ['a,b,c', 'd', 'e']);
});

test('build order puts dependencies first and collapses cycles', () => {
  const g = graphOf([['app', 'web'], ['app', 'db'], ['web', 'json'], ['db', 'json'], ['x', 'y'], ['y', 'x'], ['app', 'x']]);
  const sccs = PDR.tarjanSCC(g);
  const { order, layerOf } = PDR.buildOrder(g, sccs);
  const flat = order.map(u => u.files.join('+'));
  assert.deepEqual(flat, ['json', 'db', 'web', 'x+y', 'app']);
  assert.equal(order.find(u => u.cyclic).files.join(), 'x,y');
  assert.equal(layerOf.get('json'), 0);
  assert.equal(layerOf.get('app'), 2);
});

test('build order is valid on a long chain (no recursion limit)', () => {
  const edges = [];
  for (let i = 0; i < 5000; i++) edges.push(['n' + i, 'n' + (i + 1)]);
  const g = graphOf(edges);
  const sccs = PDR.tarjanSCC(g);
  assert.equal(sccs.length, 5001);
  const { order } = PDR.buildOrder(g, sccs);
  assert.equal(order[0].files[0], 'n5000');
  assert.equal(order[5000].files[0], 'n0');
});

test('shortest path and reachability', () => {
  const g = graphOf([['a', 'b'], ['b', 'c'], ['a', 'c'], ['c', 'd']]);
  assert.deepEqual(PDR.shortestPath(g, 'a', 'd'), ['a', 'c', 'd']);
  assert.equal(PDR.shortestPath(g, 'd', 'a'), null);
  assert.deepEqual([...PDR.reachable(g, 'd', true)].sort(), ['a', 'b', 'c']);
});

// ---------- resolution ----------

test('python resolution: packages, relative, src layout, externals, broken', () => {
  const r = PDR.analyze([
    F('proj/src/app/__init__.py', ''),
    F('proj/src/app/main.py', 'from app.core import engine\nimport requests\nfrom .missing import x'),
    F('proj/src/app/core/__init__.py', 'from . import engine'),
    F('proj/src/app/core/engine.py', 'from ..util import helper\nimport os.path'),
    F('proj/src/app/util.py', ''),
    F('proj/scripts/run.py', 'import helpers\nfrom app.main import *'),
    F('proj/scripts/helpers.py', '')
  ], { stripRoot: true });
  assert.equal(r.rootName, 'proj');
  const out = f => r.graph.out(f).sort();
  assert.deepEqual(out('src/app/main.py'), ['src/app/core/engine.py']);
  assert.deepEqual(out('src/app/core/__init__.py'), ['src/app/core/engine.py']);
  assert.deepEqual(out('src/app/core/engine.py'), ['src/app/util.py']);
  assert.deepEqual(out('scripts/run.py'), ['scripts/helpers.py', 'src/app/main.py']);
  assert.deepEqual(r.externals.map(e => e.name).sort(), ['os', 'requests']);
  assert.equal(r.broken.length, 1);
  assert.equal(r.broken[0].spec, '.missing');
  assert.equal(r.broken[0].line, 3);
});

test('js resolution: extensions, index files, aliases, packages', () => {
  const r = PDR.analyze([
    F('src/index.ts', "import App from './App'\nimport { api } from '@/lib/api'\nimport x from 'lodash/get'\nimport y from '@scope/pkg/sub'"),
    F('src/App.tsx', "import './components'\nimport z from './nope'"),
    F('src/components/index.js', ''),
    F('src/lib/api.js', "const fs = require('node:fs')")
  ]);
  assert.deepEqual(r.graph.out('src/index.ts').sort(), ['src/App.tsx', 'src/lib/api.js']);
  assert.deepEqual(r.graph.out('src/App.tsx'), ['src/components/index.js']);
  assert.deepEqual(r.externals.map(e => e.name).sort(), ['@scope/pkg', 'fs', 'lodash']);
  assert.equal(r.broken[0].spec, './nope');
});

test('ignored folders and non-code files are skipped', () => {
  const r = PDR.analyze([
    F('a.py', 'import b'), F('b.py', ''),
    F('venv/lib/x.py', ''), F('node_modules/m/i.js', ''), F('README.md', '# hi')
  ]);
  assert.deepEqual(r.graph.nodes().sort(), ['a.py', 'b.py']);
});

// ---------- end to end on the bundled sample ----------

test('sample daemon project: cycle, order, critical files, path', () => {
  const r = PDR.analyze(loadDir(path.join(__dirname, '..', 'sample', 'fad_demo')));
  assert.equal(r.stats.files, 17);

  // rules -> actions/__init__ -> move -> rules
  assert.equal(r.cycles.length, 1);
  assert.deepEqual(r.cycles[0].files, ['fad/actions/__init__.py', 'fad/actions/move.py', 'fad/rules.py']);
  assert.equal(r.cycles[0].loop[0], r.cycles[0].loop[r.cycles[0].loop.length - 1]);
  assert.ok(r.cycles[0].steps.every(s => s.line > 0));

  // archive.py imports a module that does not exist
  assert.deepEqual(r.broken.map(b => [b.file, b.spec]), [['fad/actions/archive.py', '.compress']]);

  // every unit appears after all of its dependencies
  const pos = new Map();
  r.order.forEach((u, i) => u.files.forEach(f => pos.set(f, i)));
  for (const a of r.graph.nodes()) for (const b of r.graph.out(a)) {
    assert.ok(pos.get(b) <= pos.get(a), `${b} should come before ${a}`);
  }

  assert.equal(r.critical[0], 'fad/config.py');
  const why = PDR.explainPath(r, 'fad/main.py', 'fad/utils/paths.py');
  assert.equal(why.path[0], 'fad/main.py');
  assert.equal(why.path[why.path.length - 1], 'fad/utils/paths.py');
  assert.deepEqual(r.externals.filter(e => !e.builtin).map(e => e.name).sort(), ['pytest', 'requests', 'watchdog', 'yaml']);
  assert.ok(r.externals.find(e => e.name === 'os').builtin);
  assert.ok(r.entryPoints.includes('fad/main.py'));
});

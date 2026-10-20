// Bundles src/app.html + src/engine.js + the bundled starter project into dist/index.html
// Run with:  node build.js
const fs = require('node:fs');
const path = require('node:path');

const root = __dirname;
const engine = fs.readFileSync(path.join(root, 'src', 'engine.js'), 'utf8');
let html = fs.readFileSync(path.join(root, 'src', 'app.html'), 'utf8');

const sampleDir = path.join(root, 'sample', 'fad_demo');
const files = [];
(function walk(dir) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
    const full = path.join(dir, e.name);
    if (e.isDirectory()) walk(full);
    else files.push({ path: path.relative(sampleDir, full).split(path.sep).join('/'), content: fs.readFileSync(full, 'utf8') });
  }
})(sampleDir);

const sample = JSON.stringify({ name: 'file_automation_daemon', files }).replace(/</g, '\\u003c');
html = html.replace('/*__ENGINE__*/', () => engine.replace(/<\/script/gi, '<\\/script'));
html = html.replace('/*__SAMPLE__*/', () => sample);

fs.mkdirSync(path.join(root, 'dist'), { recursive: true });
fs.writeFileSync(path.join(root, 'dist', 'index.html'), html);
console.log('dist/index.html written:', (html.length / 1024).toFixed(1), 'KB,', files.length, 'sample files');

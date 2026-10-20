// Formatting helpers for the report, following the ADYPU mini-project template:
// A4, margins left 3.8 cm / others 2.5 cm, Times New Roman 12 pt justified,
// line spacing 1.5 with 6 pt after, chapter headings 16 pt bold capitals,
// sections 14 pt bold, sub-sections 12 pt bold italic, captions 11 pt italic
// (figure captions below, table captions above), code in Courier New 10 pt.
const fs = require("fs");
const path = require("path");
const D = require("docx");

const FONT = "Times New Roman";
const MONO = "Courier New";
const CM = 567;                    // DXA per centimetre
const PAGE = { width: 11906, height: 16838 };
const MARGIN = { left: Math.round(3.8 * CM), right: Math.round(2.5 * CM), top: Math.round(2.5 * CM), bottom: Math.round(2.5 * CM) };
const TEXT_WIDTH = PAGE.width - MARGIN.left - MARGIN.right;  // 8334 DXA = 14.7 cm
const TEXT_WIDTH_PX = Math.floor(TEXT_WIDTH / 15);           // 96 px per inch, 1440 DXA per inch
const HEADER_FILL = "D9E2F3";

// ---------- inline text: `code`, **bold**, *italic*, {{placeholder}} ----------

function runs(text, base = {}) {
    const out = [];
    const re = /(\*\*[^*]+\*\*|`[^`]+`|\*[^*\s][^*]*\*|\{\{[^}]+\}\})/g;
    let last = 0;
    for (let m; (m = re.exec(text));) {
        if (m.index > last) out.push(new D.TextRun({ text: text.slice(last, m.index), ...base }));
        const t = m[0];
        if (t.startsWith("`")) out.push(new D.TextRun({ text: t.slice(1, -1), font: MONO, size: base.size ? base.size - 2 : 22, ...omit(base, ["font", "size"]) }));
        else if (t.startsWith("**")) out.push(...runs(t.slice(2, -2), { ...base, bold: true }));  // may hold {{..}} or `..`
        else if (t.startsWith("{{")) out.push(new D.TextRun({ text: "[" + t.slice(2, -2) + "]", ...base, highlight: "yellow" }));
        else out.push(...runs(t.slice(1, -1), { ...base, italics: true }));
        last = m.index + t.length;
    }
    if (last < text.length) out.push(new D.TextRun({ text: text.slice(last), ...base }));
    return out;
}

function omit(o, keys) {
    const r = { ...o };
    for (const k of keys) delete r[k];
    return r;
}

function p(text, opts = {}) {
    return new D.Paragraph({ alignment: D.AlignmentType.JUSTIFIED, ...opts, children: runs(text, opts.run || {}) });
}

function centered(text, run = {}, opts = {}) {
    return new D.Paragraph({ alignment: D.AlignmentType.CENTER, ...opts, children: runs(text, run) });
}

function blank(n = 1) {
    return Array.from({ length: n }, () => new D.Paragraph({ children: [] }));
}

// ---------- document state: headings, figures, tables, citations ----------

class Report {
    constructor(pages) {
        this.pages = pages || {};  // search key -> page number text, from the previous pass
        this.toc = [];             // {level, text, key}
        this.figures = [];         // {num, title, key}
        this.tables = [];
        this.chapter = 0;
        this.counts = { fig: 0, tab: 0, lst: 0 };
        this.citeOrder = [];
        this.numbering = 0;
    }

    page(key) {
        return this.pages[key] || "0";
    }

    cite(...keys) {
        const nums = keys.map((k) => {
            let i = this.citeOrder.indexOf(k);
            if (i < 0) {
                this.citeOrder.push(k);
                i = this.citeOrder.length - 1;
            }
            return i + 1;
        });
        return nums.map((n) => `[${n}]`).join(", ");
    }

    // A front-matter or back-matter title (CERTIFICATE, REFERENCES ...), on a new page.
    title(text, tocText) {
        if (tocText) this.toc.push({ level: 0, text: tocText, key: text });
        return new D.Paragraph({
            heading: D.HeadingLevel.HEADING_1,
            alignment: D.AlignmentType.CENTER,
            pageBreakBefore: true,
            spacing: { before: 0, after: 240 },
            children: [new D.TextRun({ text, bold: true, size: 32 })],
        });
    }

    chapterHeading(n, name) {
        this.chapter = n;
        this.counts = { fig: 0, tab: 0, lst: 0 };
        const label = `CHAPTER ${n}`;
        this.toc.push({ level: 0, text: `Chapter ${n}: ${titleCase(name)}`, key: label });
        return new D.Paragraph({
            heading: D.HeadingLevel.HEADING_1,
            alignment: D.AlignmentType.CENTER,
            pageBreakBefore: true,
            spacing: { before: 0, after: 360 },
            children: [new D.TextRun({ text: label, bold: true, size: 32 }), new D.TextRun({ text: name.toUpperCase(), bold: true, size: 32, break: 1 })],
        });
    }

    appendixHeading(letter, name) {
        const label = `APPENDIX ${letter}`;
        this.toc.push({ level: 1, text: `Appendix ${letter}: ${name}`, key: label });
        return new D.Paragraph({
            heading: D.HeadingLevel.HEADING_2,
            pageBreakBefore: letter !== "A",
            spacing: { before: 240, after: 120 },
            children: [new D.TextRun({ text: `${label}: ${name.toUpperCase()}`, bold: true, size: 28 })],
        });
    }

    h2(text) {
        this.toc.push({ level: 1, text, key: text });
        return new D.Paragraph({ heading: D.HeadingLevel.HEADING_2, keepNext: true, spacing: { before: 240, after: 120 }, children: [new D.TextRun({ text, bold: true, size: 28 })] });
    }

    h3(text) {
        return new D.Paragraph({ heading: D.HeadingLevel.HEADING_3, keepNext: true, spacing: { before: 180, after: 80 }, children: [new D.TextRun({ text, bold: true, italics: true, size: 24 })] });
    }

    // Figure: image, then caption below. `num` must be the next number in this chapter.
    figure(file, num, title, { maxWidth = 1, maxHeightPx = 760 } = {}) {
        this.counts.fig++;
        const expected = `${this.chapter}.${this.counts.fig}`;
        if (num !== expected) throw new Error(`figure ${title}: numbered ${num}, should be ${expected}`);
        const data = fs.readFileSync(file);
        const [w, h] = pngSize(data);
        let width = Math.round(TEXT_WIDTH_PX * maxWidth);
        let height = Math.round((width * h) / w);
        if (height > maxHeightPx) {
            height = maxHeightPx;
            width = Math.round((height * w) / h);
        }
        const key = `Figure ${num}:`;
        this.figures.push({ num, title, key });
        return [
            new D.Paragraph({ alignment: D.AlignmentType.CENTER, keepNext: true, spacing: { before: 120, after: 60, line: 240 }, children: [new D.ImageRun({ type: "png", data, transformation: { width, height } })] }),
            new D.Paragraph({ alignment: D.AlignmentType.CENTER, spacing: { after: 200 }, children: runs(`${key} ${title}`, { italics: true, size: 22 }) }),
        ];
    }

    // Table with its caption above. widths: fractions of the text width.
    table(num, title, headers, rows, widths, { size = 21, align } = {}) {
        this.counts.tab++;
        const expected = `${this.chapter}.${this.counts.tab}`;
        if (num !== expected) throw new Error(`table ${title}: numbered ${num}, should be ${expected}`);
        const key = `Table ${num}:`;
        this.tables.push({ num, title, key });
        return [
            new D.Paragraph({ alignment: D.AlignmentType.CENTER, keepNext: true, spacing: { before: 160, after: 80 }, children: runs(`${key} ${title}`, { italics: true, size: 22 }) }),
            grid(headers, rows, widths, { size, align }),
            ...blank(),
        ];
    }

    listing(num, title, code) {
        this.counts.lst++;
        const expected = `${this.chapter}.${this.counts.lst}`;
        if (num !== expected) throw new Error(`listing ${title}: numbered ${num}, should be ${expected}`);
        const lines = code.split("\n");
        if (lines.length > 20) throw new Error(`listing ${num} has ${lines.length} lines (template: at most 20)`);
        return [
            ...codeBlock(lines, true),
            new D.Paragraph({ alignment: D.AlignmentType.CENTER, spacing: { before: 80, after: 200 }, children: runs(`Listing ${num}: ${title}`, { italics: true, size: 22 }) }),
        ];
    }

    bullets(items) {
        return items.map((t) => new D.Paragraph({ alignment: D.AlignmentType.JUSTIFIED, numbering: { reference: "bullets", level: 0 }, spacing: { after: 60 }, children: runs(t) }));
    }

    numbered(items) {
        const instance = ++this.numbering;
        return items.map((t) => new D.Paragraph({ alignment: D.AlignmentType.JUSTIFIED, numbering: { reference: "numbers", level: 0, instance }, spacing: { after: 60 }, children: runs(t) }));
    }
}

function titleCase(s) {
    const small = new Set(["and", "of", "the", "for", "in", "on", "to", "a"]);
    return s.toLowerCase().split(" ").map((w, i) => (i > 0 && small.has(w) ? w : w[0].toUpperCase() + w.slice(1))).join(" ");
}

function pngSize(buf) {
    if (buf.readUInt32BE(0) !== 0x89504e47) throw new Error("not a PNG");
    return [buf.readUInt32BE(16), buf.readUInt32BE(20)];
}

function codeBlock(lines, keep) {
    return lines.map((line, i) => new D.Paragraph({
        keepNext: keep && i < lines.length - 1,
        keepLines: true,
        shading: { type: D.ShadingType.CLEAR, fill: "F2F2F2", color: "auto" },
        // a long line that wraps continues 4 characters right of its own indentation
        // (Courier New 10 pt: 120 DXA per character)
        indent: { left: (line.match(/^ */)[0].length + 4) * 120, hanging: (line.match(/^ */)[0].length + 4) * 120 },
        spacing: { before: 0, after: 0, line: 240 },
        children: [new D.TextRun({ text: line.length ? line : " ", font: MONO, size: 20 })],
    }));
}

const BORDER = { style: D.BorderStyle.SINGLE, size: 4, color: "000000" };
const BORDERS = { top: BORDER, bottom: BORDER, left: BORDER, right: BORDER };

function grid(headers, rows, widths, { size = 21, align, borders = BORDERS, header = true } = {}) {
    const cols = widths.map((f) => Math.round(f * TEXT_WIDTH));
    cols[cols.length - 1] += TEXT_WIDTH - cols.reduce((a, b) => a + b, 0);
    const cell = (text, i, isHead) => new D.TableCell({
        width: { size: cols[i], type: D.WidthType.DXA },
        borders,
        margins: { top: 50, bottom: 50, left: 90, right: 90 },
        shading: isHead ? { type: D.ShadingType.CLEAR, fill: HEADER_FILL, color: "auto" } : undefined,
        verticalAlign: D.VerticalAlign.CENTER,
        children: String(text).split("\n").map((t) => new D.Paragraph({
            alignment: align && align[i] ? align[i] : D.AlignmentType.LEFT,
            spacing: { before: 0, after: 0, line: 240 },
            children: runs(t, { size, bold: isHead || undefined }),
        })),
    });
    const trs = [];
    if (header) trs.push(new D.TableRow({ tableHeader: true, cantSplit: true, children: headers.map((h, i) => cell(h, i, true)) }));
    for (const r of rows) trs.push(new D.TableRow({ cantSplit: true, children: r.map((c, i) => cell(c, i, false)) }));
    return new D.Table({ width: { size: TEXT_WIDTH, type: D.WidthType.DXA }, columnWidths: cols, rows: trs });
}

// Lines `from`..`to` (substrings of the first and last line) of a source file, dedented.
function excerpt(file, from, to, { skip = 0 } = {}) {
    const lines = fs.readFileSync(file, "utf8").split("\n");
    let a = -1;
    for (let i = 0, seen = 0; i < lines.length; i++) {
        if (lines[i].includes(from)) {
            if (seen++ === skip) {
                a = i;
                break;
            }
        }
    }
    if (a < 0) throw new Error(`${file}: no line with ${from}`);
    let b = -1;
    for (let i = a; i < lines.length; i++) if (lines[i].includes(to)) { b = i; break; }
    if (b < 0) throw new Error(`${file}: no line with ${to} after ${from}`);
    const part = lines.slice(a, b + 1);
    const indent = Math.min(...part.filter((l) => l.trim()).map((l) => l.match(/^ */)[0].length));
    return part.map((l) => l.slice(indent)).join("\n");
}

function roman(n) {
    const t = [[10, "x"], [9, "ix"], [5, "v"], [4, "iv"], [1, "i"]];
    let s = "";
    for (const [v, r] of t) while (n >= v) { s += r; n -= v; }
    return s;
}

// A table-of-contents line with a dot leader and a right-aligned page number.
function tocLine(text, page, level) {
    return new D.Paragraph({
        tabStops: [{ type: D.TabStopType.RIGHT, position: TEXT_WIDTH, leader: "dot" }],
        indent: { left: level * 400 },
        spacing: { before: level === 0 ? 100 : 0, after: 30, line: 276 },
        children: [new D.TextRun({ text, bold: level === 0, size: 23 }), new D.TextRun({ children: [new D.Tab(), String(page)], size: 23 })],
    });
}

module.exports = { D, FONT, MONO, CM, PAGE, MARGIN, TEXT_WIDTH, TEXT_WIDTH_PX, HEADER_FILL, runs, p, centered, blank, Report, grid, codeBlock, excerpt, roman, tocLine, BORDERS };

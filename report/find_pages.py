"""Finds the printed page number of every heading, figure and table.

    python3 find_pages.py report.txt index.json pages.json

report.txt is `pdftotext -layout` of the rendered report (pages separated by
form feeds). Front matter is numbered i, ii, ... from the certificate; the
chapters are numbered 1, 2, ... from "CHAPTER 1".
"""
import json
import re
import sys


def roman(n):
    out = ""
    for value, letters in ((10, "x"), (9, "ix"), (5, "v"), (4, "iv"), (1, "i")):
        while n >= value:
            out += letters
            n -= value
    return out


def main():
    text_file, index_file, out_file = sys.argv[1:4]
    pages = [re.sub(r"\s+", " ", p) for p in open(text_file, encoding="utf-8").read().split("\f")]
    index = json.load(open(index_file, encoding="utf-8"))

    def find(key, start):
        for i in range(start, len(pages)):
            if key in pages[i]:
                return i
        sys.exit(f"find_pages: '{key}' not found after page {start + 1}")

    found = {}
    at = 1  # pdf page index 1 = the page after the cover
    for key in index["entries"]:
        at = find(key, at)
        found[key] = at
    cert, chapter1 = found["CERTIFICATE"], found["CHAPTER 1"]
    if cert != 1:
        sys.exit(f"find_pages: the cover takes {cert} pages, it must take one")

    def printed(i):
        return roman(i - cert + 1) if i < chapter1 else str(i - chapter1 + 1)

    result = {key: printed(i) for key, i in found.items()}
    for group in ("figures", "tables"):
        at = chapter1
        for key in index[group]:
            at = find(key, at)
            result[key] = printed(at)
    json.dump(result, open(out_file, "w", encoding="utf-8"), indent=1)


if __name__ == "__main__":
    main()

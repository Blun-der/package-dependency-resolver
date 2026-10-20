#!/bin/sh
# Builds the report: figures, then the .docx twice. The first pass is rendered to
# PDF to find the page of every heading, figure and table; the second pass fills
# those numbers into the contents and the lists.
#   sh report/build.sh      -> report/build/Mini_Project_Report.docx and .pdf
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B=$HERE/build
SOFFICE=${SOFFICE:-soffice}

sh "$HERE/make_figures.sh"

# LibreOffice with its own profile, which replaces Times New Roman and Courier New
# by the metric-compatible Nimbus Roman and Nimbus Mono PS, so that pages break as in Word.
rm -rf "$B/lo"
mkdir -p "$B/lo"
cp -r "$HERE/lo-profile/user" "$B/lo/"
render() {
    rm -f "$B/Mini_Project_Report.pdf"
    "$SOFFICE" -env:UserInstallation="file://$B/lo" --headless --convert-to pdf --outdir "$B" "$B/Mini_Project_Report.docx" >/dev/null 2>&1
    pdftotext -layout "$B/Mini_Project_Report.pdf" "$B/report.txt"
}

node "$HERE/build_report.js"
render
python3 -I "$HERE/find_pages.py" "$B/report.txt" "$B/index.json" "$B/pages.json"
for pass in 2 3; do
    cp "$B/pages.json" "$B/pages.prev.json"
    node "$HERE/build_report.js" "$B/pages.json"
    render
    python3 -I "$HERE/find_pages.py" "$B/report.txt" "$B/index.json" "$B/pages.json"
    if cmp -s "$B/pages.json" "$B/pages.prev.json"; then
        echo "page numbers stable after pass $pass"
        break
    fi
done
echo "pages: $(pdfinfo "$B/Mini_Project_Report.pdf" | awk '/^Pages/ {print $2}')"

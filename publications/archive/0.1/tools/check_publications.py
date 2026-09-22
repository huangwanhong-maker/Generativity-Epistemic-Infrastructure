"""Check declared draft sources, build freshness and PDF requirement coverage.

Requires pypdf. This checks artifacts, not substantive conformance or accessibility.
Run after a full build; results are saved under build/publications.
"""
from __future__ import annotations

from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import sys

try:
    from pypdf import PdfReader
except ImportError:
    raise SystemExit("pypdf is needed for PDF text inspection; install it in the review environment first.")

ROOT = Path(__file__).resolve().parents[1]


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    manifest = json.loads((ROOT / "publications/documents.json").read_text(encoding="utf-8"))
    report_path = ROOT / "build/publications/build-report.json"
    if not report_path.is_file():
        raise SystemExit("A full build report is required. Run tools/build_publications.py first.")
    build = json.loads(report_path.read_text(encoding="utf-8"))
    built = {entry["id"]: entry for entry in build["documents"]}
    source_entries = []
    all_ids = []
    errors = []
    for doc in manifest["documents"]:
        path = ROOT / doc["source"]
        source = path.read_text(encoding="utf-8")
        ids = re.findall(r"\\req\{([A-Z][A-Z0-9]*-\d{3})\}", source)
        all_ids.extend(ids)
        source_entries.append((doc, source, ids))
    duplicates = [key for key, count in Counter(all_ids).items() if count > 1]
    if duplicates:
        errors.append("Duplicate requirement IDs: " + ", ".join(duplicates))
    known_ids = set(all_ids)
    rows = []
    for doc, source, ids in source_entries:
        name = doc["id"]
        findings = []
        entry = built.get(name)
        if not entry or not entry.get("built"):
            findings.append("No successful full-build record")
        else:
            if entry["source_sha256"] != digest(ROOT / doc["source"]):
                findings.append("Build source hash is stale")
            if entry["preamble_sha256"] != digest(ROOT / "publications/preamble.tex"):
                findings.append("Build preamble hash is stale")
            if entry["review_warnings"]:
                findings.extend(entry["review_warnings"])
        if not ids:
            findings.append("No stable requirement identifiers")
        if "responsible editor:" not in source.lower() or doc["source"] not in source:
            findings.append("Missing editor/source metadata")
        references = set(re.findall(r"\b(?:CH|TC|EIP|D1|D2|V100|R110|E200|C210)-\d{3}\b", source))
        if references - known_ids:
            findings.append("Unknown requirement references: " + ", ".join(sorted(references - known_ids)))
        pdf_path = ROOT / "build/publications" / (name + ".pdf")
        pages = None
        if not pdf_path.is_file():
            findings.append("Review PDF missing")
        else:
            if entry and entry.get("pdf_sha256") != digest(pdf_path):
                findings.append("Review PDF hash differs from build report")
            pdf = PdfReader(pdf_path)
            pages = len(pdf.pages)
            extracted = [page.extract_text() or "" for page in pdf.pages]
            text = "\n".join(extracted)
            # Layout can introduce whitespace but must not lose an identifier.
            compact = re.sub(r"\s+", "", text)
            missing_ids = [identifier for identifier in ids if identifier not in compact]
            if missing_ids:
                findings.append("Requirements missing from extracted PDF: " + ", ".join(missing_ids))
            if "WORKINGDRAFTFORREVIEW" not in re.sub(r"\s+", "", extracted[0]):
                findings.append("PDF title-page draft status missing")
            if "notadopted" not in compact.lower():
                findings.append("PDF non-adoption statement missing")
            if "\ufffd" in text:
                findings.append("Unicode replacement character in extracted PDF")
        errors.extend(name + ": " + issue for issue in findings)
        rows.append({"id": name, "requirements": len(ids), "pages": pages, "findings": findings})
    result = {
        "checked_at_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "Source metadata, unique/resolved requirement IDs, full-build freshness, PDF hashes and extracted requirement coverage. Not substantive conformance, rendering completeness, or accessibility assurance.",
        "passed": not errors, "requirement_count": len(all_ids),
        "documents": rows, "errors": errors,
    }
    (ROOT / "build/publications/check-report.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return int(bool(errors))


if __name__ == "__main__":
    sys.exit(main())

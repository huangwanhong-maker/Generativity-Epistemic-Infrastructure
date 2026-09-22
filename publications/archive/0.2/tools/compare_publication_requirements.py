"""Write a reproducible requirement change register against archived draft 0.1.

This reports text changes; it does not decide semantic equivalence or adoption.
"""
from __future__ import annotations

from collections import Counter
import csv
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def requirements(text: str) -> dict[str, str]:
    result = {}
    for match in re.finditer(r"\\req\{([A-Z][A-Z0-9]*-\d{3})\}\{", text):
        start = match.end()
        depth = 1
        end = start
        while depth and end < len(text):
            char = text[end]
            if char == "{" and text[end - 1] != "\\":
                depth += 1
            elif char == "}" and text[end - 1] != "\\":
                depth -= 1
            end += 1
        if depth:
            raise ValueError("Unclosed requirement " + match.group(1))
        identifier = match.group(1)
        if identifier in result:
            raise ValueError("Duplicate requirement " + identifier)
        result[identifier] = " ".join(text[start:end - 1].split())
    return result


def main() -> None:
    manifest = json.loads((ROOT / "publications/documents.json").read_text(encoding="utf-8"))
    rows = []
    for doc in manifest["documents"]:
        old = requirements((ROOT / "publications/archive/0.1" / doc["source"]).read_text(encoding="utf-8"))
        new = requirements((ROOT / doc["source"]).read_text(encoding="utf-8"))
        for identifier in sorted(old.keys() | new.keys()):
            before, after = old.get(identifier, ""), new.get(identifier, "")
            if identifier not in old:
                status = "added"
            elif identifier not in new:
                status = "removed"
            elif before == after:
                status = "unchanged"
            elif before.replace("Working Draft 0.1", "Working Draft 0.2") == after:
                status = "dependency-edition"
            else:
                status = "revised"
            rows.append({"document": doc["id"], "requirement": identifier, "change": status,
                         "old_text": before, "new_text": after,
                         "old_text_sha256": hashlib.sha256(before.encode()).hexdigest() if before else "",
                         "new_text_sha256": hashlib.sha256(after.encode()).hexdigest() if after else ""})
    target = ROOT / "docs/reviews/requirement_changes_0.1_to_0.2.csv"
    with target.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    counts = Counter(row["change"] for row in rows)
    print(json.dumps(dict(counts), indent=2))
    if counts["removed"]:
        raise SystemExit("Historical requirement IDs were removed; review before proceeding")


if __name__ == "__main__":
    main()

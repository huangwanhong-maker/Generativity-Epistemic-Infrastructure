"""Compile the declared working drafts with LuaLaTeX, locally or through WSL.

Run from any directory. Requires Python 3.10+ and latexmk/LuaLaTeX.
No dependencies are downloaded and no files outside the workspace are removed.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "publications" / "documents.json"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wsl", metavar="DISTRIBUTION", help="Use this WSL distribution from Windows")
    parser.add_argument("--document", action="append", help="Build only the named manifest ID (repeatable)")
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    docs = manifest["documents"]
    if args.document:
        unknown = set(args.document) - {doc["id"] for doc in docs}
        if unknown:
            parser.error("Unknown document IDs: " + ", ".join(sorted(unknown)))
        docs = [doc for doc in docs if doc["id"] in args.document]
    missing = [doc["source"] for doc in docs if not (ROOT / doc["source"]).is_file()]
    if missing:
        parser.error("Missing sources: " + ", ".join(missing))
    if args.wsl:
        if sys.platform != "win32":
            parser.error("--wsl is for invocation from Windows; invoke natively inside WSL instead")
        converted = subprocess.run(
            ["wsl.exe", "-d", args.wsl, "--", "wslpath", "-a", str(ROOT)],
            check=True, capture_output=True, text=True, encoding="utf-8",
        ).stdout.strip()
        prefix = ["wsl.exe", "-d", args.wsl, "--cd", converted, "--"]
    else:
        if not shutil.which("latexmk"):
            parser.error("latexmk was not found. From Windows, use --wsl Ubuntu-24.04 if available.")
        prefix = []

    output_root = ROOT / "build" / "publications"
    output_root.mkdir(parents=True, exist_ok=True)
    tool_versions = {"build_python": sys.version.split()[0]}
    for executable, option in (("latexmk", "-v"), ("lualatex", "--version")):
        version_result = subprocess.run(prefix + [executable, option], cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
        version_lines = [line.strip() for line in version_result.stdout.splitlines() if line.strip()]
        tool_versions[executable] = version_lines[0] if version_lines else "Version unavailable"
    reports = []
    failed = False
    for doc in docs:
        name = doc["id"]
        if not re.fullmatch(r"[A-Za-z0-9-]+", name):
            parser.error("Invalid manifest document ID: " + name)
        source = (ROOT / doc["source"]).resolve()
        if not source.is_relative_to(ROOT):
            parser.error("Source is outside workspace: " + str(source))
        outdir = output_root / name
        outdir.mkdir(parents=True, exist_ok=True)
        command = prefix + [
            "latexmk", "-lualatex", "-interaction=nonstopmode", "-halt-on-error",
            "-file-line-error", "-no-shell-escape", "-outdir=" + outdir.relative_to(ROOT).as_posix(),
            "-jobname=" + name, source.relative_to(ROOT).as_posix(),
        ]
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
        transcript = outdir / "build-output.txt"
        transcript.write_text(result.stdout + result.stderr, encoding="utf-8")
        log_path = outdir / (name + ".log")
        log = log_path.read_text(encoding="utf-8", errors="replace") if log_path.exists() else ""
        problems = re.findall(r"(?:Overfull \\[hv]box[^\n]*|(?:LaTeX|Package [^\n]+) Warning:[^\n]*|Missing character:[^\n]*)", log)
        advisories = re.findall(r"Underfull \\[hv]box[^\n]*", log)
        pdf = outdir / (name + ".pdf")
        success = result.returncode == 0 and pdf.is_file()
        if success:
            shutil.copy2(pdf, output_root / (name + ".pdf"))
        failed |= not success or bool(problems)
        reports.append({
            "id": name, "source": doc["source"], "source_sha256": digest(source),
            "preamble_sha256": digest(ROOT / "publications" / "preamble.tex"),
            "built": success, "compiler_exit": result.returncode,
            "pdf": "build/publications/" + name + ".pdf" if success else None,
            "pdf_sha256": digest(pdf) if success else None,
            "review_warnings": problems,
            "spacing_advisories": advisories,
        })
        print(f"{name}: {'built' if success else 'FAILED'}; {len(problems)} review warnings; {len(advisories)} spacing advisories", flush=True)
        if not success:
            print("\n".join((result.stdout + result.stderr).splitlines()[-22:]), flush=True)
    report = {
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "status": manifest["status"], "engine": "LuaLaTeX via latexmk",
        "tool_versions": tool_versions,
        "scope": "Requested document set; build success is not standards adoption or substantive conformance",
        "documents": reports,
    }
    report_name = "build-report.json" if not args.document else "build-report-selected.json"
    (output_root / report_name).write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())

"""Capture v0.7 implementation/artifact/evidence hashes after acceptance.

Pass only accepted install/test run directories. Existing manifests are never
overwritten. Markdown is omitted from implementation inputs, but versioned
benchmark/review evidence in docs/validation is hashed separately.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-dir", type=Path, action="append", default=[])
    parser.add_argument("--release-result", required=True)
    parser.add_argument("--asan-result", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    evidence = root / "docs/validation"
    source_manifest = evidence / "v0.7-source-artifacts.json"
    evidence_manifest = evidence / "v0.7-evidence-manifest.json"
    recheck = evidence / "v0.7-baseline-reverification.json"
    for path in (source_manifest, evidence_manifest, recheck):
        if path.exists():
            raise FileExistsError(path)

    def entry(path):
        raw = path.read_bytes()
        return {"path": path.relative_to(root).as_posix(), "bytes": len(raw),
                "sha256": hashlib.sha256(raw).hexdigest()}

    def encoded(value):
        return (json.dumps(value, indent=2) + "\n").encode("utf-8")

    source_extensions = {".cpp", ".hpp", ".h", ".c", ".py", ".toml", ".json", ".txt",
                         ".in", ".cmake", ".ps1", ".def"}
    source_paths = []
    for directory in ("include", "src", "bindings", "tests", "examples", "benchmarks",
                      "packages", "tools", "cmake", "experiments"):
        for path in (root / directory).rglob("*"):
            if path.is_file() and path.suffix.lower() in source_extensions and not any(
                part in {"__pycache__", "build", "dist"} or part.endswith(".egg-info")
                for part in path.relative_to(root).parts
            ):
                source_paths.append(path)
    source_paths.extend(root / name for name in ("CMakeLists.txt", ".clang-format", ".gitignore"))
    source_paths.extend(path for path in (root / "third_party").glob("*.json"))
    artifacts = set()
    for directory in [root / "build/Release", root / "build-asan/Release", *args.artifact_dir]:
        directory = directory.resolve()
        if not directory.is_relative_to(root) or not directory.is_dir():
            raise ValueError(f"Artifact directory must exist inside GR-OS: {directory}")
        for path in directory.rglob("*"):
            if path.is_file() and path.suffix.lower() in {".exe", ".dll", ".lib", ".so", ".a", ".pyd", ".whl"}:
                artifacts.add(path)
    timestamp = datetime.now(timezone.utc).isoformat()
    source_document = {"format": "gr.validation-source-manifest/1", "version": "0.7.0",
        "captured_utc": timestamp,
        "scope": "Implementation/build/test/example/benchmark/package inputs and accepted Release/ASan native artifacts and wheels. Living Markdown excluded; vendored source integrity checked separately.",
        "release_ctest": args.release_result, "asan_ctest": args.asan_result,
        "sources": [entry(p) for p in sorted(set(source_paths))],
        "artifacts": [entry(p) for p in sorted(artifacts)]}
    # The preserved baseline manifest uses its original relative path convention.
    baseline_manifest = json.loads((evidence / "v0.6-pre-v0.7/manifest.json").read_text(encoding="utf-8"))
    checked = []
    for item in baseline_manifest["files"]:
        path = root / item["path"]
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != item["sha256"]:
            raise ValueError(f"Preserved baseline changed: {path}")
        checked.append(item["path"])
    recheck_document = {"format": "gr.baseline-reverification/1", "captured_utc": timestamp,
        "baseline_commit": "bcbb477479d4f5e5988a7cd078a47bd76eb3ac57",
        "checked_files": checked, "count": len(checked), "result": "all hashes matched"}
    paths = [p for p in evidence.rglob("*") if p.is_file() and "v0.7" in p.relative_to(evidence).as_posix()
             and p != evidence_manifest]
    # Validate/hash everything before publishing success-shaped output. The
    # evidence manifest is written last and is the completion marker.
    pending = [(source_manifest, encoded(source_document)), (recheck, encoded(recheck_document))]
    entries = [entry(p) for p in sorted(paths)]
    entries.extend({"path": p.relative_to(root).as_posix(), "bytes": len(raw),
                    "sha256": hashlib.sha256(raw).hexdigest()} for p, raw in pending)
    evidence_document = {"format": "gr.validation-evidence-manifest/1", "version": "0.7.0",
        "captured_utc": timestamp,
        "scope": "Versioned v0.7 logs, initial/final measurements, package records, preserved baseline evidence, source/artifact manifest and recheck. Self and living top-level review/verification documents excluded.",
        "files": sorted(entries, key=lambda item: item["path"])}
    pending.append((evidence_manifest, encoded(evidence_document)))
    for path, raw in pending:
        with path.open("xb") as stream:
            stream.write(raw)
    print(json.dumps({"sources": len(set(source_paths)), "artifacts": len(artifacts),
                      "evidence": len(entries), "baseline_checked": len(checked)}))


if __name__ == "__main__":
    main()

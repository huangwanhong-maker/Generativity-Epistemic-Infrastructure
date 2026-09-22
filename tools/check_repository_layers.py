"""Inspect the four source repositories, pinned submodules and privacy boundaries.

Read-only: this does not fetch, initialize, commit, repair or inspect user records.
"""
from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
INFRA = "applicative_infrastructure"
REPOSITORIES = {
    ".": [INFRA],
    INFRA: ["gr_generalized_application", "gr_academia_application"],
    f"{INFRA}/gr_generalized_application": [],
    f"{INFRA}/gr_academia_application": [],
}
PRIVATE_PARTS = {".runtime", ".data", ".venv", ".venv-linux", "node_modules",
                 "__pycache__", ".pytest_cache", "test-results", "build", "dist"}


def git(repo, *arguments, check=True):
    result = subprocess.run(["git", "-C", str(repo), *arguments],
                            capture_output=True, text=True, encoding="utf-8")
    if check and result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip())
    return result


def main():
    report = {"class": "Bounded source-repository verification", "repositories": [], "errors": []}
    for relative, expected_children in REPOSITORIES.items():
        repo = ROOT / relative
        errors = []
        entry = {"path": relative}
        try:
            actual_root = Path(git(repo, "rev-parse", "--show-toplevel").stdout.strip()).resolve()
            if actual_root != repo.resolve():
                raise RuntimeError("Expected an independent repository at this path")
            entry["head"] = git(repo, "rev-parse", "HEAD").stdout.strip()
            entry["git_directory"] = str(Path(git(repo, "rev-parse", "--absolute-git-dir").stdout.strip()).relative_to(ROOT))
            if git(repo, "status", "--porcelain", "--untracked-files=normal").stdout:
                errors.append("Working tree or index is not clean")
            index = {}
            for record in git(repo, "ls-files", "--stage", "-z").stdout.split("\0"):
                if not record:
                    continue
                fields, path = record.split("\t", 1)
                mode, object_id, stage = fields.split()
                index[path] = (mode, object_id)
                if stage != "0":
                    errors.append("Unresolved index entry: " + path)
                name = Path(path).name
                if (set(Path(path).parts) & PRIVATE_PARTS or
                    name.endswith((".sqlite", ".sqlite-wal", ".sqlite-shm", ".sqlite-journal", ".key", ".pyc")) or
                    (name.startswith(".env") and name != ".env.example")):
                    errors.append("Private/generated path is tracked: " + path)
            children = {path: oid for path, (mode, oid) in index.items() if mode == "160000"}
            if set(children) != set(expected_children):
                errors.append("Gitlink paths differ from the declared repository architecture")
            entry["submodules"] = children
            for child in expected_children:
                child_head = git(repo / child, "rev-parse", "HEAD").stdout.strip()
                if children.get(child) != child_head:
                    errors.append("Parent index does not pin the checked-out child: " + child)
                committed = git(repo, "rev-parse", "HEAD:" + child).stdout.strip()
                if committed != child_head:
                    errors.append("Parent commit does not pin the checked-out child: " + child)
                for key in ("path", "url"):
                    value = git(repo, "config", "--file", ".gitmodules", "--get",
                                f"submodule.{child}.{key}").stdout.strip()
                    if not value or (key == "path" and value != child):
                        errors.append("Missing or inconsistent submodule configuration: " + child)
            for name in ("README.md", "LICENSE", "LICENSE.md", "CONTRIBUTORS.md", ".gitignore", ".gitattributes"):
                if name not in index:
                    errors.append("Required repository artifact is not tracked: " + name)
            probes = ["build/private-backups/probe.txt", ".pytest_cache/probe"]
            if relative == INFRA:
                probes += [".runtime/generalized/accounts.sqlite", ".runtime/academia/records/private.key"]
            elif relative != ".":
                probes += ["web_application/node_modules/probe", "web_application/.env", "web_application/instance.sqlite"]
            for probe in probes:
                if git(repo, "check-ignore", "-q", "--no-index", probe, check=False).returncode:
                    errors.append("Private/generated probe is not ignored: " + probe)
        except (RuntimeError, ValueError, OSError) as error:
            errors.append(str(error))
        entry["errors"] = errors
        report["repositories"].append(entry)
        report["errors"].extend(f"{relative}: {error}" for error in errors)
    academia = ROOT / INFRA / "gr_academia_application"
    if git(academia, "merge-base", "--is-ancestor",
           "b98730b46eca5fdf441937aef0180f5b3d4f94dd", "HEAD", check=False).returncode:
        report["errors"].append("Academia's original source commit is not retained in ancestry")
    report["passed"] = not report["errors"]
    print(json.dumps(report, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())

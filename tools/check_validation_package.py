"""Check the 0.2 review ledger and execute the bounded P0 arithmetic fixture.

This tool checks traceability and arithmetic. It does not assess truth, source
interpretation, human participation, institutional performance or conformance.
"""
from __future__ import annotations

from collections import Counter, defaultdict
import csv
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform

from compare_publication_requirements import requirements

ROOT = Path(__file__).resolve().parents[1]
REVIEW = ROOT / "docs/reviews/validation_0.2"
FIXTURES = ROOT / "examples/validation_0.2"
BASELINE = ROOT / "publications/archive/0.2"


def read(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def arithmetic(data: dict) -> dict:
    values = [data.get(key) for key in ("N", "Y", "D", "A")]
    if any(type(value) is not int or value < 0 for value in values):
        return {"valid_input": False, "numeric_pass": None}
    n, yes, no, abstain = values
    returned = yes + no + abstain
    if returned > n:
        return {"valid_input": False, "numeric_pass": None}
    substantive = yes + no
    required_returned = (2 * n + 2) // 3
    required_approvals = (2 * substantive + 2) // 3
    quorum = returned >= required_returned
    minimum = substantive >= 3
    approval = yes >= required_approvals
    return {"valid_input": True, "returned": returned,
            "required_returned": required_returned, "substantive": substantive,
            "required_approvals": required_approvals, "quorum_met": quorum,
            "minimum_met": minimum, "approval_met": approval,
            "numeric_pass": quorum and minimum and approval}


def main() -> int:
    errors = []
    baseline = {}
    for doc in read(BASELINE / "publications/documents.json")["documents"]:
        path = BASELINE / doc["source"]
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        for identifier, text in requirements(path.read_text(encoding="utf-8")).items():
            if identifier in baseline:
                errors.append("Duplicate baseline ID: " + identifier)
            baseline[identifier] = {"document": doc["id"], "text": text,
                                    "source": path.relative_to(ROOT).as_posix(),
                                    "source_sha256": digest}
    for item in read(BASELINE / "snapshot.json")["files"]:
        path = ROOT / item["archived"]
        if hashlib.sha256(path.read_bytes()).hexdigest() != item["sha256"]:
            errors.append("Changed baseline archive: " + item["archived"])

    groups = {}
    membership = defaultdict(list)
    group_files = ["governance_review_groups.json", "conceptual_review_groups.json",
                   "epistemic_review_groups.json"]
    for name in group_files:
        for group in read(REVIEW / name):
            key = group["id"]
            if key in groups:
                errors.append("Duplicate review group: " + key)
            groups[key] = group
            if group["review_status"] not in {"reviewed", "finding", "open"}:
                errors.append("Unknown editorial review status: " + key)
            for field in ("topic", "basis", "evidence", "next_action"):
                if not group.get(field):
                    errors.append("Missing group " + field + ": " + key)
            if not group["requirements"] or len(group["requirements"]) != len(set(group["requirements"])):
                errors.append("Empty or duplicated group membership: " + key)
            for identifier in group["requirements"]:
                if identifier not in baseline:
                    errors.append("Unknown baseline requirement: " + identifier)
                elif baseline[identifier]["document"] != group["document"]:
                    errors.append("Incorrect document for " + identifier + " in " + key)
                membership[identifier].append(key)
    missing = set(baseline) - set(membership)
    if missing:
        errors.append("Baseline requirements without an editorial review group: " + ", ".join(sorted(missing)))

    assessments = read(FIXTURES / "p1_assessment.json")
    if isinstance(assessments, dict):
        assessments = assessments["assessments"]
    components = defaultdict(list)
    whole_results = {}
    component_keys = set()
    for row in assessments:
        identifier = row["requirement"]
        if identifier not in baseline:
            errors.append("Unknown assessment requirement: " + identifier)
        if row["result"] not in {"fulfilled", "not fulfilled", "not assessed", "not applicable"}:
            errors.append("Unknown component assessment result: " + identifier)
        if row.get("assessment_level") not in {"requirement", "component"}:
            errors.append("Missing or unknown assessment level: " + identifier)
        if row.get("assessment_level") == "requirement":
            if identifier in whole_results:
                errors.append("Duplicate whole-requirement result: " + identifier)
            whole_results[identifier] = row["result"]
            if row["result"] == "fulfilled" and not row.get("component_coverage_complete"):
                errors.append("Whole-requirement fulfillment without complete coverage: " + identifier)
        for field in ("component", "subject", "applicability", "evidence", "rationale", "fixture_locator"):
            if not row.get(field):
                errors.append("Missing assessment " + field + ": " + identifier)
        key = (identifier, row["component"], row["subject"])
        if key in component_keys:
            errors.append("Duplicate assessment component: " + str(key))
        component_keys.add(key)
        components[identifier].append(row)
        filename, _, anchor = row["fixture_locator"].partition("#")
        location = (FIXTURES / filename).resolve()
        if not location.is_relative_to(FIXTURES.resolve()) or not location.is_file():
            errors.append("Invalid fixture locator: " + row["fixture_locator"])
        elif anchor and ('id="' + anchor + '"') not in location.read_text(encoding="utf-8"):
            errors.append("Unresolved fixture anchor: " + row["fixture_locator"])

    case_results = []
    case_ids = set()
    for case in read(FIXTURES / "p0_ballot_cases.json")["cases"]:
        if case["id"] in case_ids:
            errors.append("Duplicate arithmetic case: " + case["id"])
        case_ids.add(case["id"])
        actual = arithmetic(case["input"])
        differences = {key: {"expected": value, "actual": actual.get(key)}
                       for key, value in case["expected"].items()
                       if actual.get(key) != value or type(actual.get(key)) is not type(value)}
        if differences:
            errors.append("Arithmetic differs from authored expectation: " + case["id"])
        for identifier in case["clause_refs"]:
            if identifier not in baseline:
                errors.append("Unknown arithmetic clause: " + identifier)
        case_results.append({"id": case["id"], "actual": actual,
                             "expected_matches": not differences, "differences": differences})

    rows = []
    for identifier, item in baseline.items():
        keys = membership[identifier]
        statuses = sorted({groups[key]["review_status"] for key in keys})
        finding_ids = sorted({finding for key in keys for finding in groups[key]["finding_ids"]})
        rows.append({"requirement": identifier, "document": item["document"],
                     "baseline": "0.2", "source": item["source"],
                     "source_sha256": item["source_sha256"], "requirement_text": item["text"],
                     "editorial_review_groups": " | ".join(keys),
                     "editorial_review_statuses": " | ".join(statuses) if keys else "not reviewed",
                     "finding_ids": " | ".join(finding_ids),
                     "p1_assessment_rows": len(components[identifier]),
                     "selected_p1_component_rows": sum(row["assessment_level"] == "component" for row in components[identifier]),
                     "selected_p1_results": " | ".join(sorted({r["result"] for r in components[identifier]})),
                     "p1_whole_requirement_assessment": whole_results.get(identifier, "not assessed in the P1 register")})
    if not errors:
        with (REVIEW / "requirement_review_matrix.csv").open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)

    inputs = [BASELINE / "snapshot.json", BASELINE / "publications/documents.json",
              *(REVIEW / name for name in group_files),
              FIXTURES / "p0_ballot_cases.json", FIXTURES / "p1_assessment.json",
              FIXTURES / "p0-governance-rehearsal.md", FIXTURES / "p1-contested-decision.md",
              Path(__file__).resolve(), ROOT / "tools/compare_publication_requirements.py"]
    report = {"checked_at_utc": datetime.now(timezone.utc).isoformat(),
              "python_version": platform.python_version(),
              "input_sha256": {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                               for path in inputs},
              "scope": "Fixed-baseline review coverage, identifier and status integrity, and bounded ballot arithmetic. No substantive or institutional conformance finding.",
              "passed": not errors, "baseline_requirements": len(baseline),
              "review_groups": len(groups), "requirements_with_review_group": len(set(baseline) & set(membership)),
              "p1_assessment_rows": len(assessments),
              "p1_component_results": dict(Counter(row["result"] for row in assessments if row["assessment_level"] == "component")),
              "p1_whole_requirement_results": dict(Counter(row["result"] for row in assessments if row["assessment_level"] == "requirement")),
              "p0_arithmetic_cases": case_results, "errors": errors}
    target = ROOT / "build/validation_0.2"
    target.mkdir(parents=True, exist_ok=True)
    (target / "check-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in report.items()
                      if key not in {"p0_arithmetic_cases", "input_sha256"}}, indent=2))
    print("Arithmetic cases:", len(case_results), "; matched:", sum(row["expected_matches"] for row in case_results))
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())

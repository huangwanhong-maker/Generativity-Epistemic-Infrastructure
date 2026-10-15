"""Compare matching benchmark workloads; optional generous regression gate."""
import argparse
import json
import math
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("before", type=Path)
parser.add_argument("after", type=Path)
parser.add_argument("--max-ratio", type=float, default=3.0)
parser.add_argument("--check", action="store_true", help="Fail when a matching median exceeds the ratio; use controlled hosts")
args = parser.parse_args()
if not math.isfinite(args.max_ratio) or args.max_ratio <= 1:
    parser.error("max-ratio must exceed one")
before, after = [json.loads(p.read_text(encoding="utf-8")) for p in [args.before, args.after]]
if before["suite"] != after["suite"]:
    raise SystemExit("Different benchmark suites cannot be compared")
old = {c["name"]: c for c in before["cases"]}
failures = []
compared = 0
incompatible = []
for field in ("platform", "machine", "python", "cpu_count"):
    if before["environment"].get(field) != after["environment"].get(field):
        incompatible.append("environment." + field)
if before["method"] != after["method"]:
    incompatible.append("measurement method")
if set(old) != {c["name"] for c in after["cases"]}:
    incompatible.append("workload inventory")
for current in after["cases"]:
    previous = old.get(current["name"])
    if previous is None or previous["work"] != current["work"]:
        incompatible.append("workload: " + current["name"])
        continue
    if any(not math.isfinite(c["median_ms"]) or c["median_ms"] <= 0 for c in (previous, current)):
        raise SystemExit("Medians must be finite and positive: " + current["name"])
    ratio = current["median_ms"] / previous["median_ms"]
    print(f"{current['name']}: {previous['median_ms']:.3f} -> {current['median_ms']:.3f} ms ({ratio:.2f}x)")
    compared += 1
    if ratio > args.max_ratio:
        failures.append(current["name"])
if not compared:
    raise SystemExit("No equivalent workloads")
print(f"Compared {compared} workloads; above {args.max_ratio:g}x: {failures}")
if incompatible:
    print("Comparison qualifications: " + ", ".join(incompatible))
if args.check and (failures or incompatible):
    raise SystemExit(1)

"""Opt-in installed facade measurement; setup/copies/planning/execution included."""

import argparse
import hashlib
import json
import platform
from statistics import median
from time import perf_counter_ns

from gr_diagonal import Diagonal

parser = argparse.ArgumentParser()
parser.add_argument("output")
args = parser.parse_args()
engine = Diagonal()
cases = []
for count in (16, 4096, 65536):
    left = [(index % 17) - 8 for index in range(count)]
    right = [(index % 23) - 11 for index in range(count)]
    expected = [a * b for a, b in zip(left, right)]
    assert engine.apply(left, right).values == expected
    samples = []
    for _ in range(5):
        start = perf_counter_ns()
        result = engine.apply(left, right)
        samples.append((perf_counter_ns() - start) / 1e6)
        assert result.values == expected
    cases.append(
        {
            "elements": count,
            "samples_ms": samples,
            "median_ms": median(samples),
            "result_sha256": hashlib.sha256(json.dumps(expected).encode()).hexdigest(),
            "package": result.report["package"],
        }
    )
with open(args.output, "w", encoding="utf-8") as stream:
    json.dump(
        {
            "format": "gr.diagonal.benchmark/1",
            "python": platform.python_version(),
            "platform": platform.platform(),
            "native_library": str(engine.path),
            "scope": "synchronous facade total: Python copy, C transport, native construction/compile, one-worker runtime, result materialization/copy; checks outside timing",
            "warmups": 1,
            "repeats": 5,
            "cases": cases,
        },
        stream,
        indent=2,
    )

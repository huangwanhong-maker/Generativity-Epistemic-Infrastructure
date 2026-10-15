"""Concurrent public-SDK SQLite transactions with explicit application rebasing.

Synthetic writers own disjoint record IDs. A conflict discards that logical
transaction, reads a new base and starts a NEW transaction ID. This is a workload
policy, never an automatic retry/rebase policy in GR-OS. WAL/FULL stays enabled.
The default fixed mode holds initial state and total accepted work constant.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import ctypes
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import statistics
import tempfile
import threading
import time

import gr_os


def memory():
    if os.name != "nt":
        return {"scope": "process memory unavailable in this runner"}
    class Counters(ctypes.Structure):
        _fields_ = [("cb", ctypes.c_uint32), ("PageFaultCount", ctypes.c_uint32)] + [
            (name, ctypes.c_size_t) for name in (
                "PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage",
                "QuotaPagedPoolUsage", "QuotaPeakNonPagedPoolUsage", "QuotaNonPagedPoolUsage",
                "PagefileUsage", "PeakPagefileUsage")]
    counters = Counters()
    counters.cb = ctypes.sizeof(counters)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GetCurrentProcess.restype = ctypes.c_void_p
    psapi = ctypes.WinDLL("psapi", use_last_error=True)
    psapi.GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.POINTER(Counters), ctypes.c_uint32]
    if not psapi.GetProcessMemoryInfo(kernel.GetCurrentProcess(), ctypes.byref(counters), counters.cb):
        raise ctypes.WinError(ctypes.get_last_error())
    return {"working_set_bytes": counters.WorkingSetSize,
            "peak_working_set_bytes": counters.PeakWorkingSetSize,
            "scope": "whole Python/native process; peak is cumulative across cases, not an allocation quota"}


def percentile(samples, q):
    return sorted(samples)[max(0, math.ceil(len(samples) * q) - 1)]


def run_case(path, writers, batch, commits, tracked, mode, admission="optimistic"):
    fixed = mode == "fixed"
    commits_per_writer = 32 // writers if fixed else commits
    measured_commits = writers * commits_per_writer
    seed = None
    def make_record(group, generation):
        return gr_os.construct({
            "format": "gr.record/1", "id": f"bench:{group[0]}:{group[1]}",
            "type": "gr.assertion@1",
            "payload": {"claim": "synthetic disjoint writer", "generation": generation},
            "attribution": {"agent": f"bench:{'group' if fixed else 'writer'}:{group[0]}",
                            "method": "synthetic"},
        })
    with gr_os.Storage(path) as setup:
        setup.create("bench")
        assert setup.capabilities["durability"] == "local-full-sync"
        if fixed:
            # All writer-count conditions begin with the same current inventory
            # and one identical accepted seed, outside the timed interval.
            initial = [make_record((group, r), -1) for group in range(4) for r in range(batch)]
            with gr_os.Transaction(setup, "bench", expected_revision="0",
                    transaction_id="bench:seed", actor="bench:seed",
                    reason="fixed-work initial state", track_history=tracked,
                    requirements={"persistent": True, "atomic_commit": True}) as tx:
                tx.stage(initial)
                accepted = tx.commit()
                assert accepted["state"] == "committed"
                seed = ("bench:seed", accepted["request_digest"], accepted["publication"]["revision"])
    start = 0
    def begin_timing():
        nonlocal start
        start = time.perf_counter_ns()
    # Opening independent connections is setup, not part of publication throughput.
    barrier = threading.Barrier(writers + 1, action=begin_timing)
    def writer(index):
        with gr_os.Storage(path) as store:
            timings, conflicts, receipts = [], 0, []
            barrier.wait(timeout=30)
            for generation in range(commits_per_writer):
                started = time.perf_counter_ns()
                step = generation * writers + index if fixed else generation
                group = step % 4 if fixed else index
                records = [make_record((group, r), step) for r in range(batch)]
                for attempt in range(1000):
                    transaction_id = f"w{index}:g{generation}:a{attempt}"
                    with gr_os.Transaction(store, "bench",
                            expected_revision="" if admission == "serialized" else store.head("bench"),
                            admission=admission,
                            transaction_id=transaction_id, actor=f"bench:writer:{index}",
                            reason="bounded concurrent throughput fixture", track_history=tracked,
                            requirements={"persistent": True, "atomic_commit": True}) as tx:
                        tx.stage(records)
                        try:
                            result = tx.commit()
                        except gr_os.NativeError as error:
                            if error.code != "conflict":
                                raise
                            conflicts += 1
                            continue
                    assert result["state"] == "committed"
                    receipts.append((transaction_id, result["request_digest"],
                                     result["publication"]["revision"]))
                    timings.append((time.perf_counter_ns() - started) / 1e6)
                    break
                else:
                    raise RuntimeError("Benchmark retry budget exhausted")
            return timings, conflicts, receipts
    with ThreadPoolExecutor(max_workers=writers) as pool:
        futures = [pool.submit(writer, index) for index in range(writers)]
        barrier.wait(timeout=30)
        results = [future.result(timeout=120) for future in futures]
        elapsed = (time.perf_counter_ns() - start) / 1e9
    with gr_os.Storage(path, read_only=True) as verify:
        snapshot = verify.snapshot("bench")
        assert len(snapshot["records"]) == (4 if fixed else writers) * batch
        expected_ids = {f"bench:{group}:{r}" for group in range(4 if fixed else writers)
                        for r in range(batch)}
        assert {r["document"]["id"] for r in snapshot["records"]} == expected_ids
        for record in snapshot["records"]:
            group = int(record["document"]["id"].split(":")[1])
            expected = 28 + group if fixed else commits_per_writer - 1
            assert record["document"]["payload"]["generation"] == expected
        receipt_groups = [result[2] for result in results]
        if seed:
            receipt_groups.append([seed])
        for receipts in receipt_groups:
            for transaction_id, digest, revision in receipts:
                receipt = verify.receipt("bench", transaction_id)
                assert receipt is not None
                assert receipt["request_digest"] == digest and receipt["revision"] == revision
        ledger = verify.history("bench", limit=1000)
        assert len(ledger["entries"]) == measured_commits + int(fixed) and not ledger["next_cursor"]
        assert bool(snapshot.get("runtime_history")) == tracked
    latencies = [latency for result in results for latency in result[0]]
    return {"seconds": elapsed, "commits_per_second": measured_commits / elapsed,
            "records_per_second": measured_commits * batch / elapsed,
            "measured_commits": measured_commits, "verified_ledger_entries": len(ledger["entries"]),
            "seed_request_digest": seed[1] if seed else None,
            "seed_revision": seed[2] if seed else None,
            "conflicts": sum(result[1] for result in results),
            "commit_latency_ms": latencies, "median_latency_ms": statistics.median(latencies),
            "p95_latency_ms": percentile(latencies, .95), "memory_after": memory()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--mode", choices=("fixed", "growing"), default="fixed")
    parser.add_argument("--admission", choices=("optimistic", "serialized"), default="optimistic")
    parser.add_argument("--commits", type=int, default=8,
                        help="Accepted commits per writer in growing mode; fixed mode always measures 32 total")
    args = parser.parse_args()
    if not 3 <= args.repeats <= 10 or not 2 <= args.commits <= 20:
        parser.error("repeats must be 3..10 and commits 2..20 per writer")
    cases = []
    seed_identities = {}
    with tempfile.TemporaryDirectory(prefix="gr-runtime-transactions-") as temporary:
        root = Path(temporary)
        run_case(root / "warmup.sqlite", 2, 1, 2, True, args.mode, args.admission)
        for tracked, batch in [(False, 1), (False, 8), (True, 4)]:
            for writers in [1, 2, 4]:
                samples = [run_case(root / f"case-{tracked}-{batch}-{writers}-{repeat}.sqlite",
                                    writers, batch, args.commits, tracked, args.mode, args.admission)
                           for repeat in range(args.repeats)]
                if args.mode == "fixed":
                    for sample in samples:
                        identity = (sample["seed_request_digest"], sample["seed_revision"])
                        assert seed_identities.setdefault((tracked, batch), identity) == identity
                cases.append({"writers": writers, "batch_records": batch, "track_history": tracked,
                              "commits_per_writer": 32 // writers if args.mode == "fixed" else args.commits,
                              "measured_commits": 32 if args.mode == "fixed" else writers * args.commits,
                              "initial_records": 4 * batch if args.mode == "fixed" else 0,
                              "final_records": (4 if args.mode == "fixed" else writers) * batch,
                              "samples": samples,
                              "median_commits_per_second": statistics.median(s["commits_per_second"] for s in samples),
                              "median_records_per_second": statistics.median(s["records_per_second"] for s in samples),
                              "median_conflicts": statistics.median(s["conflicts"] for s in samples)})
                print(f"writers={writers} batch={batch} history={tracked}: "
                      f"{cases[-1]['median_commits_per_second']:.2f} commits/s", flush=True)
    result = {"suite": "gr.runtime.transactions/2", "workload_mode": args.mode,
              "admission": args.admission,
              "platform": platform.platform(),
              "python": platform.python_version(), "logical_cpus": os.cpu_count(),
              "native_sha256": hashlib.sha256(Path(os.environ["GR_OS_LIBRARY"]).read_bytes()).hexdigest(),
              "runner_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "workload": ("Fixed work: four groups, 4*batch current records primed in one identical untimed "
                           "commit; 32 measured commits total for 1/2/4 writers; group=(generation*writers+index)%4; "
                           "payload generation=global step; final generations=28+group; final ledger=33. "
                           "Concurrent writers own disjoint groups; one writer rotates all four, two each own two, "
                           "four each own one. Record attribution is group-stable across writer counts."
                           if args.mode == "fixed" else
                           "Growing work: commits per writer held fixed; total work, current set and history "
                           "increase with writer count. Not an isolated contention comparison."),
              "method": "one global warmup; fresh SQLite database per measured sample; independent connections; "
                        "whole-workspace CAS; WAL/FULL; end-to-end record construction, staging, commit and "
                        "explicit conflict rebase latency; connection opening excluded by a start barrier; "
                        "final connection closing/checkpoint costs included; final snapshots and exact receipts "
                        "checked outside timing; " +
                        ("fixed 32 measured commits, identical seeded inventory and 33 final ledger entries "
                         "for every writer count" if args.mode == "fixed" else
                         "growing total accepted work/current inventory/history with writer count"),
              "limitations": "Small local workloads. Python/JSON transport and GIL are included. No group commit, "
                             "power-loss qualification, tail-latency SLA or production-scale claim.",
              "cases": cases}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()

"""Public Python/runtime and native CLI vertical slices; no private test hooks."""

import asyncio
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import unittest

import gr_os as gr


def graph(data=b"runtime"):
    return gr.Graph(
        [
            gr.Node("left", "gr.bytes.identity@1", [gr.Value.bytes(data)]),
            gr.Node("right", "gr.bytes.identity@1", [gr.Value.bytes(b"-graph")]),
            gr.Node(
                "joined",
                "gr.bytes.concat@1",
                [gr.OutputRef("left"), gr.OutputRef("right")],
            ),
            gr.Node("digest", "gr.bytes.sha256@1", [gr.OutputRef("joined")]),
        ],
        [gr.OutputRef("joined"), gr.OutputRef("digest")],
    )


def document(id="sdk:artifact"):
    return {
        "format": "gr.record/1",
        "id": id,
        "type": "gr.entity@1",
        "payload": {},
        "attribution": {"agent": "sdk-test", "method": "integration"},
    }


class RuntimeSDKTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native = gr.Native()

    def test_fork_join_plan_lifetime_and_catalog(self):
        with gr.Runtime(native=self.native, workers=2) as runtime:
            self.assertIn("catalog", runtime.capabilities())
            plan = runtime.plan(graph())
            job = runtime.submit(plan, serial=True)
            plan.close()
            result = job.result(5).raise_for_status()
            self.assertEqual(result.outputs[0].data, b"runtime-graph")
            self.assertEqual(
                result.outputs[1].data,
                hashlib.sha256(b"runtime-graph").hexdigest().encode(),
            )
        self.assertEqual(job.result(0).status, "succeeded")
        job.close()
        job.close()

    def test_result_retention_capacity_and_release(self):
        with gr.Runtime(native=self.native, workers=1, max_live_jobs=1) as runtime:
            with runtime.plan(graph()) as plan:
                first = runtime.submit(plan)
                first.result(5).raise_for_status()
                with self.assertRaises(gr.NativeError):
                    runtime.submit(plan)
                first.close()
                with runtime.submit(plan) as second:
                    second.result(5).raise_for_status()

    def test_policy_denial_and_strict_options(self):
        with gr.Runtime(
            native=self.native, policy={"allow_all_pure": False}
        ) as runtime:
            with runtime.plan(graph()) as plan:
                with self.assertRaises(gr.NativeError):
                    runtime.submit(plan)
        with gr.Runtime(native=self.native) as runtime:
            with runtime.plan(graph()) as plan:
                for options in (
                    {"timeout_mss": 10},
                    {"serial": 1},
                    {"priority": True},
                    {"priority": 2**64 - 1},
                ):
                    with (
                        self.subTest(options=options),
                        self.assertRaises(gr.NativeError),
                    ):
                        runtime.submit(plan, **options)
            bad = graph()._encode()
            bad["nodes"][0]["typo"] = 1
            with self.assertRaises(gr.NativeError):
                runtime.plan(bad)

    def test_deadline_structured_result_and_cancel_after_commit(self):
        with gr.Runtime(native=self.native) as runtime:
            result = runtime.run(graph(), timeout_ms=0)
            self.assertEqual(result.status, "deadline_exceeded")
            with self.assertRaises(gr.NativeError):
                result.raise_for_status()
            with runtime.plan(graph()) as plan, runtime.submit(plan) as job:
                job.result(5).raise_for_status()
                job.cancel()
                self.assertEqual(job.result(0).status, "succeeded")

    def test_async_wait(self):
        async def run():
            with (
                gr.Runtime(native=self.native) as runtime,
                runtime.plan(graph()) as plan,
            ):
                with runtime.submit(plan) as job:
                    result = await job
                    self.assertEqual(result.status, "succeeded")

        asyncio.run(run())

    def test_large_wait_timeout_is_bounded_per_native_poll(self):
        with gr.Runtime(native=self.native) as runtime:
            with runtime.plan(graph()) as plan, runtime.submit(plan) as job:
                job.result(5).raise_for_status()
                self.assertEqual(job.result(1e308).status, "succeeded")
                with self.assertRaises(ValueError):
                    job.result(10**1000)

    def test_same_plan_concurrent_submissions(self):
        with (
            gr.Runtime(native=self.native, workers=3) as runtime,
            runtime.plan(graph()) as plan,
        ):
            errors = []

            def run():
                try:
                    with runtime.submit(plan, max_retained_bytes=1024 * 1024) as job:
                        self.assertEqual(
                            job.result(5).outputs[0].data, b"runtime-graph"
                        )
                except BaseException as e:
                    errors.append(e)

            threads = [threading.Thread(target=run) for _ in range(8)]
            for thread in threads:
                thread.start()
            for thread in threads:
                thread.join()
            self.assertFalse(errors, errors)

    def test_standalone_transaction_store_lease_and_rollback(self):
        store = gr.Storage(backend="memory", native=self.native)
        store.create("tx")
        transaction = gr.Transaction(
            store,
            "tx",
            expected_revision="0",
            transaction_id="sdk:tx",
            actor="sdk",
            reason="lease test",
        )
        record = gr.construct(document(), native=self.native)
        transaction.stage([record])
        store.close()
        accepted = transaction.commit()
        self.assertEqual(accepted["state"], "committed")
        self.assertEqual(transaction.snapshot()["records"][0], record)
        self.assertEqual(transaction.recover()["state"], "committed")
        transaction.close()
        with gr.Storage(backend="memory", native=self.native) as store:
            store.create("rollback")
            with gr.Transaction(
                store,
                "rollback",
                expected_revision="0",
                transaction_id="r",
                actor="sdk",
                reason="rollback",
            ) as tx:
                tx.stage([record])
                tx.rollback()
                self.assertEqual(store.head("rollback"), "0")
                with self.assertRaises(gr.NativeError):
                    tx.stage([record])

    def test_job_artifact_transaction_history_reopen(self):
        with tempfile.TemporaryDirectory(prefix="gr-runtime-sdk-") as tmp:
            path = Path(tmp) / "runtime.sqlite"
            with gr.Storage(path, native=self.native) as store:
                store.create("published")
                with gr.Runtime(
                    native=self.native, policy={"writable_workspaces": ["published"]}
                ) as runtime:
                    publication = gr.Publication(
                        store,
                        {
                            "workspace": "published",
                            "expected_revision": "0",
                            "transaction_id": "sdk:publish",
                            "actor": "sdk",
                            "reason": "publish computed bytes",
                            "track_history": True,
                        },
                        [gr.ArtifactBinding(gr.OutputRef("joined"), document())],
                    )
                    result = runtime.run(
                        graph(), publication=publication
                    ).raise_for_status()
                    self.assertIsNotNone(result.publication)
                    snapshot = store.snapshot("published")
                    self.assertEqual(len(snapshot["records"]), 1)
                    digest = snapshot["records"][0]["document"]["artifacts"][0][
                        "sha256"
                    ]
                    self.assertEqual(store.blob("published", digest), b"runtime-graph")
                    revision = store.head("published")
                    self.assertIsNotNone(store.receipt("published", "sdk:publish"))
            with gr.Storage(path, native=self.native) as reopened:
                self.assertEqual(reopened.head("published"), revision)
                self.assertEqual(reopened.blob("published", digest), b"runtime-graph")
                self.assertEqual(len(reopened.snapshot("published")["records"]), 1)

    def test_denied_publication_has_no_effects(self):
        with gr.Storage(backend="memory", native=self.native) as store:
            store.create("denied")
            publication = gr.Publication(
                store,
                {
                    "workspace": "denied",
                    "expected_revision": "0",
                    "transaction_id": "denied",
                    "actor": "sdk",
                    "reason": "denial",
                },
            )
            with gr.Runtime(native=self.native) as runtime:
                with self.assertRaises(gr.NativeError):
                    runtime.run(graph(), publication=publication)
            self.assertEqual(store.head("denied"), "0")
            self.assertIsNone(store.receipt("denied", "denied"))

    def test_record_validation_is_catalog_operation(self):
        record = gr.construct(document(), native=self.native)
        with gr.Runtime(native=self.native) as runtime:
            result = runtime.run(
                gr.Graph(
                    [
                        gr.Node(
                            "verify",
                            "gr.record.verify@1",
                            [gr.Value.json("gr.json@1", record)],
                        )
                    ],
                    [gr.OutputRef("verify")],
                )
            )
            result.raise_for_status()
            self.assertTrue(result.outputs[0].data["valid"])


class ScriptedRuntimeNative:
    """Wire-response double for Python ownership; native fault tests are separate."""

    capabilities = {"features": ["runtime_jobs"]}

    def __init__(self, status="outcome_unknown"):
        self.status = status
        self.ready = True
        self.closed = []

    def call(self, operation, **parameters):
        if operation == "runtime.open":
            return {"handle": "runtime:test", "catalog": {}}
        if operation == "runtime.plan":
            return {"handle": "plan:test", "plan": {}}
        if operation == "runtime.submit":
            return {"handle": "job:test"}
        if operation == "job.wait":
            return {
                "ready": self.ready,
                "result": {
                    "id": "job:test",
                    "status": self.status,
                    "outputs": [],
                    "publication": {"state": self.status},
                    "error": None,
                    "diagnostics": {},
                },
            }
        if operation == "job.recover":
            self.status = "succeeded"
            return {"state": "committed", "retry_requested": parameters["retry"]}
        if operation.endswith(".close"):
            self.closed.append(parameters["handle"])
            return {"closed": True}
        raise AssertionError(operation)


class RecoveryOwnershipTests(unittest.TestCase):
    def test_uncertain_run_keeps_recovery_until_result_close(self):
        native = ScriptedRuntimeNative()
        with gr.Runtime(native=native) as runtime:
            result = runtime.run(graph())
        self.assertNotIn("job:test", native.closed)
        self.assertEqual(result.status, "outcome_unknown")
        self.assertIsNotNone(result.recovery)
        recovered = result.recover_publication(retry=True)
        self.assertTrue(recovered["retry_requested"])
        self.assertEqual(result.recovery.result().status, "succeeded")
        result.close()
        result.close()
        self.assertEqual(native.closed.count("job:test"), 1)

    def test_chained_raise_preserves_uncertain_result(self):
        native = ScriptedRuntimeNative()
        with gr.Runtime(native=native) as runtime:
            with self.assertRaises(gr.NativeError) as caught:
                runtime.run(graph()).raise_for_status()
        self.assertNotIn("job:test", native.closed)
        caught.exception.result.recover_publication()
        caught.exception.result.close()
        self.assertIn("job:test", native.closed)

    def test_successful_run_releases_native_job(self):
        native = ScriptedRuntimeNative("succeeded")
        with gr.Runtime(native=native) as runtime:
            result = runtime.run(graph())
        self.assertIsNone(result.recovery)
        self.assertIn("job:test", native.closed)

    def test_async_cancellation_is_not_masked_by_concurrent_close(self):
        async def run():
            native = ScriptedRuntimeNative()
            native.ready = False
            job = gr.Job(native, {"handle": "job:test"})
            task = asyncio.create_task(job.wait_async())
            await asyncio.sleep(0)
            job.close()
            task.cancel()
            with self.assertRaises(asyncio.CancelledError):
                await task

        asyncio.run(run())


@unittest.skipUnless(os.environ.get("GR_OS_CLI"), "Native CLI path required")
class CLITests(unittest.TestCase):
    def run_cli(self, command, request=None):
        return subprocess.run(
            [os.environ["GR_OS_CLI"], command],
            input=None if request is None else json.dumps(request),
            text=True,
            capture_output=True,
            timeout=15,
        )

    def test_capabilities_and_run(self):
        p = self.run_cli("capabilities")
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertIn("runtime_jobs", json.loads(p.stdout)["result"]["features"])
        p = self.run_cli("run", {"graph": graph()._encode()})
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(json.loads(p.stdout)["result"]["status"], "succeeded")

    def test_strict_requests_terminal_status_and_session(self):
        p = subprocess.run(
            [os.environ["GR_OS_CLI"], "call"],
            input='{"abi":1,"op":"sdk.capabilities","op":"bad"}',
            text=True,
            capture_output=True,
            timeout=15,
        )
        self.assertEqual(p.returncode, 1)
        self.assertEqual(json.loads(p.stdout)["error"]["code"], "invalid_request")
        p = self.run_cli(
            "run", {"graph": graph()._encode(), "options": {"timeout_ms": 0}}
        )
        self.assertEqual(p.returncode, 3)
        self.assertEqual(json.loads(p.stdout)["result"]["status"], "deadline_exceeded")
        requests = [
            {"abi": 1, "op": "runtime.open"},
            {"abi": 1, "op": "runtime.capabilities", "handle": "runtime:1"},
            {"abi": 1, "op": "runtime.close", "handle": "runtime:1"},
        ]
        p = subprocess.run(
            [os.environ["GR_OS_CLI"], "session"],
            input="\n".join(map(json.dumps, requests)) + "\n",
            text=True,
            capture_output=True,
            timeout=15,
        )
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(len(p.stdout.splitlines()), 3)
        self.assertTrue(all(json.loads(line)["ok"] for line in p.stdout.splitlines()))
        # EOF drains a runtime even without explicit close; escaped op spelling
        # is valid JSON and must still participate in resource ownership.
        commands = [
            r'{"abi":1,"op":"runtime\u002eopen"}',
            json.dumps(
                {
                    "abi": 1,
                    "op": "runtime.plan",
                    "handle": "runtime:1",
                    "graph": graph()._encode(),
                }
            ),
            json.dumps(
                {
                    "abi": 1,
                    "op": "runtime.submit",
                    "handle": "runtime:1",
                    "plan": "plan:2",
                }
            ),
        ]
        p = subprocess.run(
            [os.environ["GR_OS_CLI"], "session"],
            input="\n".join(commands) + "\n",
            text=True,
            capture_output=True,
            timeout=15,
        )
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        self.assertEqual(len(p.stdout.splitlines()), 3)


if __name__ == "__main__":
    unittest.main()

"""Cross-boundary integration checks; only fresh temporary stores are used."""

from concurrent.futures import ThreadPoolExecutor
import ctypes
import gc
import hashlib
import json
from pathlib import Path
from tempfile import TemporaryDirectory
from threading import Barrier
import unittest

from gr_os import (
    Native,
    NativeError,
    Storage,
    apply,
    construct,
    execute,
    graph,
    match,
    project,
    validate,
    verify,
)
from gr_os import (
    Value,
    contracts,
    execute_typed,
    history_append,
    history_reconstruct,
    history_merge,
    parse_metadata,
)


def document(identifier="record:1", **changes):
    value = {
        "format": "gr.record/1",
        "id": identifier,
        "type": "gr.entity@1",
        "payload": {"label": "日本語 café"},
        "attribution": {"agent": "test:alice", "method": "synthetic-test"},
    }
    value.update(changes)
    return value


class BindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native = Native()

    def assert_native_error(self, code, function, *args, **kwargs):
        with self.assertRaises(NativeError) as caught:
            function(*args, **kwargs)
        self.assertEqual(caught.exception.code, code)

    def test_unicode_integrity_and_unknown_payload_preservation(self):
        source = document(payload={"text": "日本語 café", "integer": 9007199254740991})
        record = construct(source)
        canonical = json.dumps(
            source, ensure_ascii=False, sort_keys=True, separators=(",", ":")
        ).encode("utf-8")
        self.assertEqual(
            record["version"], "sha256:" + hashlib.sha256(canonical).hexdigest()
        )
        self.assertTrue(verify(record)["valid"])
        changed = {**record, "document": {**source, "payload": {"text": "changed"}}}
        self.assertFalse(verify(changed)["valid"])
        opaque = construct(
            document(
                type="example.unavailable@7", payload={"preserve": [1, True, None]}
            )
        )
        report = verify(opaque)
        self.assertTrue(report["valid"])
        self.assertFalse(report["semantic_complete"])
        self.assertEqual(opaque["document"]["payload"], {"preserve": [1, True, None]})

    def test_numbers_are_rejected_without_cross_language_rounding(self):
        for value in (1.0, 1e-7, 9007199254740992, -9007199254740992):
            with self.subTest(value=value), self.assertRaises(NativeError):
                construct(document(payload={"value": value}))

    def test_core_graph_match_and_temporal_selection(self):
        a = construct(document("a", type="gr.event@1"))
        b = construct(document("b", type="gr.event@1"))
        relation = construct(
            document(
                "r",
                type="gr.relation@1",
                participants=[
                    {"role": "origin", "reference": {"scope": "record", "id": "a"}},
                    {
                        "role": "destination",
                        "reference": {"scope": "record", "id": "b"},
                    },
                    {
                        "role": "outside",
                        "reference": {"scope": "subject", "id": "external:subject"},
                    },
                ],
            )
        )
        order = construct(
            document("order", type="gr.order@1", order={"before": "a", "after": "b"})
        )
        snapshot = apply({"revision": "0", "records": []}, [relation, order, b, a])
        self.assertTrue(validate(snapshot)["valid"])
        self.assertEqual(len(graph(snapshot)["incidences"]), 3)
        self.assertEqual(len(match(snapshot, {"type": "gr.event@1"})["records"]), 2)
        self.assertTrue(project(snapshot)["consistent"])
        self.assertTrue(project(snapshot, {"after": ["a"]})["consistent"])
        reverse = construct(
            document("reverse", type="gr.order@1", order={"before": "b", "after": "a"})
        )
        contradictory = apply(snapshot, [reverse])
        self.assertFalse(project(contradictory)["consistent"])
        self.assertTrue(project(contradictory, {"order_ids": ["order"]})["consistent"])

    def test_memory_and_sqlite_round_trip_retry_and_history(self):
        with TemporaryDirectory(prefix="gr-python-") as temporary:
            for backend in ("memory", "sqlite"):
                with self.subTest(backend=backend):
                    path = (
                        Path(temporary) / "roundtrip.sqlite"
                        if backend == "sqlite"
                        else None
                    )
                    with Storage(path, backend=backend) as store:
                        store.create("w")
                        record = construct(document())
                        kwargs = dict(
                            expected_revision="0",
                            transaction_id="t1",
                            actor="test:alice",
                            reason="Synthetic test",
                        )
                        first = store.commit("w", [record], **kwargs)
                        second = store.commit(
                            "w",
                            [construct(document("second"))],
                            expected_revision=first["revision"],
                            transaction_id="t2",
                            actor="test:alice",
                            reason="Second synthetic record",
                        )
                        replay = store.commit("w", [record], **kwargs)
                        self.assertTrue(replay["replayed"])
                        self.assertEqual(replay["revision"], first["revision"])
                        self.assertEqual(replay["current_revision"], second["revision"])
                        self.assertEqual(
                            len(store.snapshot("w", first["revision"])["records"]), 1
                        )
                        self.assertEqual(len(store.snapshot("w")["records"]), 2)
                        page = store.history("w", limit=1)
                        self.assertEqual(
                            page["entries"][0]["revision"], second["revision"]
                        )
                        older = store.history("w", before=page["next_cursor"], limit=1)
                        self.assertEqual(
                            older["entries"][0]["revision"], first["revision"]
                        )
                        self.assert_native_error(
                            "transaction_conflict",
                            store.commit,
                            "w",
                            [record],
                            **{**kwargs, "actor": "test:bob"},
                        )
                        self.assert_native_error(
                            "conflict",
                            store.commit,
                            "w",
                            [record],
                            **{**kwargs, "transaction_id": "stale"},
                        )
                    self.assert_native_error("invalid_handle", store.head, "w")
                    if backend == "sqlite":
                        with Storage(path, read_only=True) as reopened:
                            self.assertEqual(reopened.head("w"), second["revision"])
                            self.assert_native_error(
                                "read_only", reopened.create, "forbidden"
                            )

    def test_artifact_closure_integrity_and_atomic_rejection(self):
        material = bytes(range(256)) + b"\x00\xffbinary"
        digest = "sha256:" + hashlib.sha256(material).hexdigest()
        artifact = {
            "sha256": digest,
            "byte_length": len(material),
            "media_type": "application/octet-stream",
        }
        with Storage(backend="memory") as store:
            store.create("w")
            record = construct(document(artifacts=[artifact]))
            kwargs = dict(
                expected_revision="0",
                transaction_id="artifact",
                actor="test:alice",
                reason="Retain synthetic artifact",
            )
            with self.assertRaises(NativeError):
                store.commit("w", [record], **kwargs)
            self.assertEqual(store.head("w"), "0")
            incorrect = construct(
                document(artifacts=[{**artifact, "byte_length": len(material) + 1}])
            )
            self.assert_native_error(
                "artifact_size",
                store.commit,
                "w",
                [incorrect],
                blobs=[material],
                **kwargs,
            )
            self.assertEqual(store.head("w"), "0")
            receipt = store.commit("w", [record], blobs=[material], **kwargs)
            self.assertEqual(store.blob("w", digest), material)
            store.create("other")
            self.assert_native_error("not_found", store.blob, "other", digest)
            store.commit(
                "w",
                [construct(document("second"))],
                expected_revision=receipt["revision"],
                transaction_id="retain-closure",
                actor="test:alice",
                reason="Retain prior closure",
            )
            self.assertEqual(store.blob("w", digest), material)

    def test_pinned_old_record_hydration(self):
        with Storage(backend="memory") as store:
            store.create("w")
            original = construct(document("target", payload={"generation": 1}))
            first = store.commit(
                "w",
                [original],
                expected_revision="0",
                transaction_id="old",
                actor="test:alice",
                reason="Original",
            )
            updated = construct(document("target", payload={"generation": 2}))
            second = store.commit(
                "w",
                [updated],
                expected_revision=first["revision"],
                transaction_id="new",
                actor="test:alice",
                reason="Update",
            )
            relation = construct(
                document(
                    "relation",
                    type="gr.relation@1",
                    participants=[
                        {
                            "role": "historical-target",
                            "reference": {
                                "scope": "record",
                                "id": "target",
                                "version": original["version"],
                            },
                        }
                    ],
                )
            )
            store.commit(
                "w",
                [relation],
                expected_revision=second["revision"],
                transaction_id="pinned",
                actor="test:alice",
                reason="Historical reference",
            )
            snapshot = store.snapshot("w")
            self.assertTrue(validate(snapshot)["valid"])
            self.assertIn(original, snapshot["archive"])
            self.assertEqual(store.record("w", "target", original["version"]), original)
            self.assertEqual(len(graph(snapshot)["nodes"]), 2)

    def test_invalid_candidate_does_not_reach_storage(self):
        with Storage(backend="memory") as store:
            store.create("w")
            missing = construct(
                document(references=[{"scope": "record", "id": "absent"}])
            )
            with self.assertRaises(NativeError):
                store.commit(
                    "w",
                    [missing],
                    expected_revision="0",
                    transaction_id="bad",
                    actor="test:alice",
                    reason="Missing target test",
                )
            self.assertEqual(store.head("w"), "0")
            mandatory = construct(document(type="example.unknown@1", required=True))
            with self.assertRaises(NativeError):
                store.commit(
                    "w",
                    [mandatory],
                    expected_revision="0",
                    transaction_id="unknown",
                    actor="test:alice",
                    reason="Unknown mandatory type test",
                )
            self.assertEqual(store.history("w")["entries"], [])

    def test_competing_sqlite_handles_do_not_lose_accepted_write(self):
        with TemporaryDirectory(prefix="gr-python-race-") as temporary:
            path = Path(temporary) / "race.sqlite"
            with Storage(path) as left, Storage(path) as right:
                left.create("w")
                barrier = Barrier(2)

                def writer(pair):
                    number, store = pair
                    record = construct(document(f"writer:{number}"))
                    barrier.wait(timeout=10)
                    try:
                        return store.commit(
                            "w",
                            [record],
                            expected_revision="0",
                            transaction_id=f"write:{number}",
                            actor=f"test:{number}",
                            reason="Concurrent synthetic write",
                        )
                    except NativeError as error:
                        return error.code

                with ThreadPoolExecutor(max_workers=2) as pool:
                    results = list(pool.map(writer, [(1, left), (2, right)]))
                self.assertEqual(sum(isinstance(result, dict) for result in results), 1)
                self.assertEqual(sum(result == "conflict" for result in results), 1)
                self.assertEqual(len(left.snapshot("w")["records"]), 1)
                self.assertEqual(left.head("w"), right.head("w"))

    def test_execution_exact_bytes_and_failures(self):
        inputs = [b"", b"abc", bytes(range(256)), b"\x00binary\xff"]
        sequential = execute("sha256", inputs)
        threaded = execute("sha256", inputs, backend="cpu.threaded", max_threads=2)
        expected = [
            hashlib.sha256(value).hexdigest().encode("ascii") for value in inputs
        ]
        self.assertEqual(sequential.outputs, expected)
        self.assertEqual(threaded.outputs, expected)
        self.assertEqual(execute("identity", inputs).outputs, inputs)
        self.assert_native_error(
            "execution_input_limit", execute, "identity", inputs, max_input_bytes=1
        )
        self.assert_native_error(
            "execution_output_limit", execute, "sha256", [b"abc"], max_output_bytes=1
        )
        self.assert_native_error(
            "execution_cancelled", execute, "identity", inputs, cancelled=True
        )
        self.assert_native_error(
            "execution_deadline", execute, "identity", inputs, deadline_ms=0
        )
        self.assert_native_error(
            "unsupported_device", execute, "identity", inputs, device_id="gpu:0"
        )
        self.assert_native_error(
            "unsupported_operation_version", execute, "identity", inputs, version="999"
        )
        self.assert_native_error(
            "invalid_request", execute, "identity", inputs, max_threads=-1
        )
        self.assert_native_error(
            "execution_batch_limit", execute, "identity", [b"", b""], max_batch_items=1
        )
        self.assert_native_error(
            "invalid_request", execute, "identity", inputs, max_thread=2
        )

    def test_c_boundary_invalid_requests_and_allocated_error_responses(self):
        requests = [
            b'{"abi":1,"op":"execution.devices","op":"store.open"}',
            b'{"abi":1.0,"op":"execution.devices"}',
            b"{broken json",
            b'{"abi":2,"op":"execution.devices"}',
            b'{"abi":1,"op":"execution.execute","operation":"identity","inputs_base64":["AB=="]}',
            b'{"abi":1,"op":"execution.devices"}\x00',
        ]
        for request in requests:
            with self.subTest(request=request):
                pointer, length = ctypes.c_void_p(), ctypes.c_size_t()
                status = self.native._lib.gr_call_json(
                    request, len(request), ctypes.byref(pointer), ctypes.byref(length)
                )
                try:
                    self.assertEqual(status, 1)
                    self.assertTrue(pointer.value)
                    envelope = json.loads(ctypes.string_at(pointer, length.value))
                    self.assertFalse(envelope["ok"])
                    self.assertIn("code", envelope["error"])
                finally:
                    self.native._lib.gr_free(pointer)
        self.assertEqual(self.native._lib.gr_call_json(None, 0, None, None), 2)
        self.native._lib.gr_free(None)

    def test_verified_binary_reader_lifetime_cancel_and_ownership(self):
        data = bytes(range(256)) * 513
        digest = "sha256:" + hashlib.sha256(data).hexdigest()
        for backend in ("memory", "sqlite"):
            with TemporaryDirectory(prefix="gr-sdk-reader-") as directory:
                store = Storage(
                    Path(directory) / "reader.sqlite" if backend == "sqlite" else None,
                    backend=backend,
                )
                store.create("w")
                receipt = store.commit(
                    "w",
                    [],
                    expected_revision="0",
                    transaction_id="blob:1",
                    actor="test",
                    reason="binary test",
                    blobs=[data],
                    required_blob_digests=[digest],
                )
                self.assertEqual(
                    store.receipt("w", "blob:1")["revision"], receipt["revision"]
                )
                self.assertIsNone(store.receipt("w", "missing"))
                self.assertEqual(store.blob_info("w", digest)["byte_length"], len(data))
                reader = store.open_blob("w", digest, chunk_bytes=4096)
                store.close()
                with reader:
                    self.assertEqual(reader.metadata["integrity"], "sha256-verified")
                    self.assertEqual(b"".join(reader.chunks()), data)
                    output = bytearray(17)
                    self.assertEqual(reader.readinto(output, 255), 17)
                    self.assertEqual(output, data[255:272])
                    self.assertEqual(reader.readinto(output, len(data)), 0)
                    with self.assertRaises(TypeError):
                        reader.readinto(b"immutable")
                    reader.cancel()
                    with self.assertRaises(NativeError):
                        reader.readinto(output)
                self.assertTrue(reader.closed)

    def test_resource_finalization_and_concurrent_idempotent_close(self):
        store = Storage(backend="memory")
        token = store._resource_handle()
        del store
        gc.collect()
        self.assert_native_error(
            "invalid_handle", self.native.call, "store.capabilities", handle=token
        )
        store = Storage(backend="memory")
        with ThreadPoolExecutor(max_workers=8) as pool:
            list(pool.map(lambda _: store.close(), range(32)))
        self.assertTrue(store.closed)

    def test_invalid_options_are_rejected_before_consuming_generators(self):
        def forbidden():
            self.fail("Invalid options consumed an input generator")
            yield b"unreachable"

        for value in (None, True, -1, 2**64):
            self.assert_native_error(
                "invalid_request",
                execute,
                "identity",
                forbidden(),
                max_batch_items=value,
            )
            self.assert_native_error(
                "invalid_request",
                execute_typed,
                "gr.bytes.identity@1",
                "gr.cpu.identity.copy@1",
                forbidden(),
                max_batch_items=value,
            )
        for offset in (-1, 2**64, True):
            with self.assertRaises(ValueError):
                self.native.read_blob("irrelevant", offset, bytearray(1))

    def test_typed_operations_have_explicit_compatible_implementations(self):
        self.assertIn("operations", contracts())
        inputs = [[Value.bytes(b"abc")], [Value.bytes(b"")]]
        expected = [hashlib.sha256(x).hexdigest().encode() for x in (b"abc", b"")]
        for provider in ("gr.cpu.sha256.oneshot@1", "gr.cpu.sha256.incremental@1"):
            result = execute_typed(
                "gr.bytes.sha256@1",
                provider,
                inputs,
                backend="cpu.threaded",
                max_threads=2,
            )
            self.assertEqual([row[0].data for row in result.outputs], expected)
            self.assertEqual(result.metadata["implementation_id"], provider)
        result = execute_typed(
            "gr.bytes.concat@1",
            "gr.cpu.concat@1",
            [[Value.bytes(b"left"), Value.bytes(b"right")]],
        )
        self.assertEqual(result.outputs[0][0].data, b"leftright")
        with self.assertRaises(NativeError):
            execute_typed(
                "gr.bytes.concat@1", "gr.cpu.sha256.oneshot@1", [[Value.bytes(b"one")]]
            )

    def test_strict_metadata_and_nonlinear_recording_history(self):
        self.assertEqual(parse_metadata('{"z":-0,"a":"café"}'), {"z": 0, "a": "café"})
        self.assert_native_error("duplicate_key", parse_metadata, '{"a":1,"\\u0061":2}')
        bundle = {"format": "gr.history/1", "nodes": []}
        attribution = {"agent": "test", "method": "synthetic", "reason": "history test"}
        base_record = construct(document("shared", payload={"value": 0}))
        bundle = history_append(
            bundle, {"revision": "0", "records": [base_record]}, [], attribution
        )
        base = bundle["nodes"][-1]["version"]
        left_record = construct(document("shared", payload={"value": 1}))
        right_record = construct(document("shared", payload={"value": 2}))
        bundle = history_append(
            bundle, {"revision": "0", "records": [left_record]}, [base], attribution
        )
        left = bundle["nodes"][-1]["version"]
        bundle = history_append(
            bundle, {"revision": "0", "records": [right_record]}, [base], attribution
        )
        right = bundle["nodes"][-1]["version"]
        conflict = history_merge(bundle, base, left, right, {}, attribution)
        self.assertFalse(conflict["merged"])
        resolved = history_merge(
            bundle, base, left, right, {"shared": left_record}, attribution
        )
        self.assertTrue(resolved["merged"])
        self.assertEqual(
            history_reconstruct(resolved["history"], resolved["revision"])["records"],
            [left_record],
        )

    def test_replacement_at_maximum_snapshot_capacity(self):
        # Independent canonical preparation keeps this boundary regression bounded.
        records = []
        for index in range(10000):
            source = document(f"capacity:{index}", payload={})
            version = (
                "sha256:"
                + hashlib.sha256(
                    json.dumps(
                        source,
                        sort_keys=True,
                        ensure_ascii=False,
                        separators=(",", ":"),
                    ).encode()
                ).hexdigest()
            )
            records.append({"document": source, "version": version})
        with Storage(backend="memory") as store:
            store.create("w")
            revision = "0"
            for start in range(0, 10000, 1000):
                revision = store.commit(
                    "w",
                    records[start : start + 1000],
                    expected_revision=revision,
                    transaction_id=f"fill:{start}",
                    actor="test",
                    reason="capacity boundary",
                )["revision"]
            changed = construct(document("capacity:0", payload={"changed": True}))
            result = store.commit(
                "w",
                [changed],
                expected_revision=revision,
                transaction_id="replace",
                actor="test",
                reason="replace at capacity",
            )
            self.assertNotEqual(result["revision"], revision)
            self.assertEqual(len(store.snapshot("w")["records"]), 10000)


if __name__ == "__main__":
    unittest.main()

"""Owned native runtime, immutable plans, jobs, and logical transactions.

Cancellation requests cooperative stopping; it does not assert rollback. Runtime
close joins native workers. Always use explicit/context-managed close for runtime
objects: finalization is a fallback and can block on a trusted native callback.
"""

from __future__ import annotations
import asyncio
from contextlib import suppress
from dataclasses import dataclass, field, replace
import math
import json
import time
from typing import Any, Mapping, Sequence
from ._native import Native, NativeError, _Resource, _native
from .execution import Value
from .storage import Storage, _encode_stage


@dataclass(frozen=True)
class OutputRef:
    node: str
    output: int = 0

    def _encode(self) -> dict:
        if type(self.output) is not int or self.output < 0:
            raise ValueError("output must be a nonnegative integer")
        return {"node": self.node, "output": self.output}


@dataclass(frozen=True)
class Node:
    id: str
    operation: str
    inputs: Sequence[Value | OutputRef]
    after: Sequence[str] = ()
    selection: Mapping[str, Any] = field(default_factory=dict)
    max_output_bytes: int = 64 * 1024 * 1024
    max_input_bytes: int = 64 * 1024 * 1024

    def _encode(self, native: Native | None = None) -> dict:
        if len(self.inputs) > 1024:
            raise NativeError("request_limit", "Node arity exceeds 1024")
        inputs = []
        for value in self.inputs:
            if isinstance(value, OutputRef):
                inputs.append({"ref": value._encode()})
            elif isinstance(value, Value):
                inputs.append({"literal": value._encode(native)})
            else:
                raise TypeError("Node inputs must be Value or OutputRef")
        return {
            "id": self.id,
            "operation": self.operation,
            "inputs": inputs,
            "after": list(self.after),
            "selection": dict(self.selection),
            "max_output_bytes": self.max_output_bytes,
            "max_input_bytes": self.max_input_bytes,
        }


@dataclass(frozen=True)
class Graph:
    nodes: Sequence[Node]
    outputs: Sequence[OutputRef]
    context: Mapping[str, Any] = field(default_factory=dict)

    def _encode(self, native: Native | None = None) -> dict:
        if len(self.nodes) > 4096:
            raise NativeError("request_limit", "Graph exceeds 4096 nodes")
        used = 0
        operands = 0
        for node in self.nodes:
            operands += len(node.inputs)
            if operands > 65536:
                raise NativeError("request_limit", "Graph exceeds 65536 operands")
            for value in node.inputs:
                if isinstance(value, Value) and value.representation != "buffers":
                    used += (
                        memoryview(value.data).nbytes
                        if value.representation == "bytes"
                        else len(
                            json.dumps(
                                value.data,
                                ensure_ascii=False,
                                allow_nan=False,
                                separators=(",", ":"),
                            ).encode("utf-8")
                        )
                    )
                    if used > 12 * 1024 * 1024:
                        raise NativeError(
                            "request_limit",
                            "Graph literals exceed the JSON transfer budget",
                        )
        return {
            "nodes": [node._encode(native) for node in self.nodes],
            "outputs": [output._encode() for output in self.outputs],
            **({"context": dict(self.context)} if self.context else {}),
        }


@dataclass(frozen=True)
class ArtifactBinding:
    output: OutputRef
    record_document: Mapping[str, Any]
    media_type: str = "application/octet-stream"

    def _encode(self) -> dict:
        return {
            "output": self.output._encode(),
            "record_document": self.record_document,
            "media_type": self.media_type,
        }


@dataclass(frozen=True)
class Publication:
    """Atomic local publication admitted with the job's pinned base revision.

    transaction contains workspace, expected_revision, transaction_id, actor,
    reason and optional track_history/requirements. The runtime must explicitly
    permit that workspace. These declarations do not authenticate the caller.
    """

    store: Storage
    transaction: Mapping[str, Any]
    artifacts: Sequence[ArtifactBinding] = ()
    records: Sequence[Mapping[str, Any]] = ()
    blobs: Sequence[bytes] = ()
    required_blob_digests: Sequence[str] = ()
    history: Mapping[str, Any] | None = None

    def _encode(self, native: Native) -> dict:
        if self.store.native is not native:
            raise ValueError("Runtime and store must use the same Native instance")
        result = {
            "store_handle": self.store._resource_handle(),
            "transaction": dict(self.transaction),
            "artifacts": [a._encode() for a in self.artifacts],
            "stage": _encode_stage(
                self.records, self.blobs, self.required_blob_digests
            ),
        }
        if self.history is not None:
            result["history"] = dict(self.history)
        return result


@dataclass(frozen=True)
class JobResult:
    """A status snapshot; uncertain synchronous results own a recovery Job.

    Close that result explicitly after receipt recovery or deliberate abandonment.
    Recovery does not mutate this snapshot; read recovery.result() for new status.
    """

    id: str
    status: str
    outputs: list[Value]
    publication: Any
    error: Any
    diagnostics: dict
    recovery: Job | None = field(default=None, repr=False, compare=False)

    @staticmethod
    def _decode(value: dict, native: Native | None = None) -> JobResult:
        outputs = [Value._decode(v, native) for v in value["outputs"]]
        recovery = None
        if value.get("recovery_handle") is not None:
            if native is None:
                raise ValueError("Native is required to own a recovery handle")
            recovery = Job(native, {"handle": value["recovery_handle"]})
        return JobResult(
            value["id"],
            value["status"],
            outputs,
            value["publication"],
            value["error"],
            value["diagnostics"],
            recovery,
        )

    def recover_publication(self, *, retry: bool = False) -> dict:
        if self.recovery is None:
            raise ValueError(
                "This result does not own a recovery job; use its original Job"
            )
        return self.recovery.recover_publication(retry=retry)

    def close(self) -> None:
        for value in self.outputs:
            value.close()
        if self.recovery is not None:
            self.recovery.close()

    def __enter__(self) -> JobResult:
        return self

    def __exit__(self, *_: Any) -> None:
        self.close()

    def raise_for_status(self) -> JobResult:
        if self.status != "succeeded":
            error = self.error if isinstance(self.error, dict) else {}
            failure = NativeError(
                error.get("code", self.status),
                error.get("message", "Runtime job did not succeed"),
                {
                    "job_status": self.status,
                    "publication": self.publication,
                    "error": self.error,
                    "diagnostics": self.diagnostics,
                },
            )
            # Chained run(...).raise_for_status() must not discard the sole
            # recovery lease when an uncertain result is otherwise temporary.
            failure.result = self
            raise failure
        return self


class Plan(_Resource):
    def __init__(self, native: Native, result: dict):
        self._own(native, "plan.close", result["handle"])
        self.description = result["plan"]


def _timeout(value: float | None) -> float | None:
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, (float, int)):
        raise ValueError("timeout must be finite nonnegative seconds or None")
    try:
        seconds = float(value)
    except OverflowError as error:
        raise ValueError(
            "timeout must be finite nonnegative seconds or None"
        ) from error
    if not math.isfinite(seconds) or seconds < 0:
        raise ValueError("timeout must be finite nonnegative seconds or None")
    return time.monotonic() + seconds


class Job(_Resource):
    """A result/control lease, independently usable after Runtime.close().

    Close releases this result lease. It does not assert cancellation. wait's
    timeout bounds the caller's wait; timeout_ms at submission is the job deadline.
    Completed results consume admission capacity until their leases are closed.
    """

    def __init__(self, native: Native, result: dict):
        self._own(native, "job.close", result["handle"])

    def status(self) -> dict:
        return self._call("job.status")

    def cancel(self) -> dict:
        return self._call("job.cancel")

    def recover_publication(self, *, retry: bool = False) -> dict:
        return self._call("job.recover", retry=retry)

    def wait(self, timeout: float | None = None) -> JobResult | None:
        deadline = _timeout(timeout)
        while True:
            remaining = (
                None if deadline is None else max(0, deadline - time.monotonic())
            )
            wait_ms = 100 if remaining is None else int(min(0.1, remaining) * 1000)
            response = self._call("job.wait", timeout_ms=wait_ms)
            if response["ready"]:
                return JobResult._decode(response["result"], self.native)
            if deadline is not None and time.monotonic() >= deadline:
                return None

    def result(self, timeout: float | None = None) -> JobResult:
        result = self.wait(timeout)
        if result is None:
            raise TimeoutError(
                "Native job is still pending; no cancellation was requested"
            )
        return result

    async def wait_async(self, timeout: float | None = None) -> JobResult | None:
        deadline = _timeout(timeout)
        try:
            while True:
                response = self._call("job.wait", timeout_ms=0)
                if response["ready"]:
                    return JobResult._decode(response["result"], self.native)
                if deadline is not None and time.monotonic() >= deadline:
                    return None
                await asyncio.sleep(0.01)
        except asyncio.CancelledError:
            with suppress(Exception):
                self.cancel()
            raise

    def __await__(self):
        return self.wait_async().__await__()


class Runtime(_Resource):
    def __init__(
        self,
        *,
        native: Native | None = None,
        modules: Sequence[str] = (),
        **config: Any,
    ):
        native = _native(native)
        if "runtime_jobs" not in native.capabilities.get("features", []):
            raise RuntimeError("Runtime requires GR-OS 0.3 or later")
        result = native.call("runtime.open", config=config, modules=list(modules))
        self._own(native, "runtime.close", result["handle"])
        self.catalog = result["catalog"]

    def capabilities(self) -> dict:
        return self._call("runtime.capabilities")

    def plan(self, graph: Graph | Mapping[str, Any]) -> Plan:
        encoded = (
            graph._encode(self.native) if isinstance(graph, Graph) else dict(graph)
        )
        return Plan(self.native, self._call("runtime.plan", graph=encoded))

    def submit(
        self, plan: Plan, *, publication: Publication | None = None, **options: Any
    ) -> Job:
        if plan.native is not self.native:
            raise ValueError("Runtime and plan must use the same Native instance")
        parameters = {"plan": plan._resource_handle(), "options": options}
        if publication is not None:
            parameters["publication"] = publication._encode(self.native)
        return Job(self.native, self._call("runtime.submit", **parameters))

    def run(
        self,
        graph: Graph | Mapping[str, Any] | Plan,
        *,
        publication: Publication | None = None,
        **options: Any,
    ) -> JobResult:
        owned = not isinstance(graph, Plan)
        plan = self.plan(graph) if owned else graph
        try:
            job = self.submit(plan, publication=publication, **options)
            retained = False
            try:
                result = job.result()
                if result.status == "outcome_unknown":
                    result = replace(result, recovery=job)
                    retained = True
                return result
            except BaseException:
                with suppress(Exception):
                    job.cancel()
                raise
            finally:
                if not retained:
                    job.close()
        finally:
            if owned:
                plan.close()


class Transaction(_Resource):
    """Pinned logical workspace transaction. Store close does not invalidate it.

    Explicit rollback discards only unpublished staging. close drops this handle;
    it cannot undo an accepted commit or a publication with uncertain outcome.
    """

    def __init__(
        self,
        store: Storage,
        workspace: str,
        *,
        expected_revision: str,
        transaction_id: str,
        actor: str,
        reason: str,
        **options: Any,
    ):
        result = store._call(
            "transaction.begin",
            workspace=workspace,
            expected_revision=expected_revision,
            transaction_id=transaction_id,
            actor=actor,
            reason=reason,
            **options,
        )
        self._own(store.native, "transaction.close", result["handle"])

    def status(self) -> dict:
        return self._call("transaction.status")

    def snapshot(self) -> dict:
        return self._call("transaction.snapshot")

    def stage(self, records=(), *, blobs=(), required_blob_digests=()) -> dict:
        return self._call(
            "transaction.stage", **_encode_stage(records, blobs, required_blob_digests)
        )

    def stage_history(self, history: dict, head: str) -> dict:
        return self._call("transaction.stage_history", history=history, head=head)

    def commit(self, **control: Any) -> dict:
        return self._call("transaction.commit", **control)

    def retry(self, **control: Any) -> dict:
        return self._call("transaction.retry", **control)

    def recover(self) -> dict:
        return self._call("transaction.recover")

    def rollback(self) -> dict:
        return self._call("transaction.rollback")

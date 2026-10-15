# Semantic operation and host execution contracts

Status: experimental software contract, v0.4. This document describes the implemented Layer 5 and Layer 6E boundary. It does not declare adoption of a programme Standard or equivalence with legacy GRRP/GSP implementations. [Layer 4 prepared planning](runtime-planner.md) composes this boundary with asynchronous scheduling.

## Responsibilities and identity

`gr_semantics` provides the public headers `gr/kernel/types.hpp` and `gr/kernel/operations.hpp`. It depends on shared JSON, canonicalization, hashing and errors. `gr_execution` depends on those semantic contracts; it does not depend on record storage or the record-history kernel.

Three registries have different responsibilities:

| Registry | Description | Executable responsibility |
| --- | --- | --- |
| `kernel::ValueTypeRegistry` | Meaning, representation and declarative constraints of an operand/result type | Representation/constraint validation; optional separately installed native validator |
| `kernel::OperationRegistry` | Exact argument/result types, pinned type descriptions, meaning, preconditions, invariants, effects and determinism | Optional separately installed input/output checks |
| `execution::ImplementationRegistry` | Physical provider, exact semantic contract fingerprint, backend/device/memory compatibility and determinism promise | Separately installed native implementation callback |

Descriptions contain no callbacks. Registration copies a description and forbids replacement of an existing identity. Description queries return copies. Registry mutation is synchronized, and user callbacks execute outside registry locks. Native validators must be pure and safe for concurrent calls. A registered native callback is trusted code; registration is neither code verification nor a sandbox.

Qualified identities have the form `namespace.name@version`, with nonempty namespace components and an explicit version. Matching is exact. These registries perform no version-range inference, alias inference, opcode assignment or automatic provider selection; Layer 4 owns compatible provider selection. Descriptors have fingerprints computed from their `gr.canonical-json/1` serialization. An operation also pins the fingerprints of every unique input/output type. `pin_types(descriptor, types)` constructs that complete binding set before registration. Execution refuses a supplied type registry that uses the same type identity with a different description, including for an empty batch.

These fingerprints cover descriptions, not native validator or provider machine code. A provider can falsely claim a contract. The contract's stated meaning, runtime checks, tests, review and distribution provenance remain separate forms of evidence. Changed validation semantics need a new declared contract version; changing native code without changing a descriptor does not become trustworthy merely because the descriptor fingerprint remains equal.

The existing record-profile registry has a separate purpose: validating retained record envelopes and interpreting declared record capabilities. An operand type is not automatically a record class, an archival identity or a scientific ontology.

## Values, signatures and supported constraints

`kernel::Value` has an exact type identity and immutable shared ownership of `Json`, `Bytes` or a named `BufferSet`. Copies share payloads; native access uses `as_json`/`as_bytes`/`as_buffers` rather than mutable public fields. `Invocation` is an ordered vector of values. Operation signatures declare an ordered vector of input type identities and output type identities, up to 64 each. Multi-input and multi-result calls are supported. No implicit type coercion occurs.

The built-in types remain `gr.bytes@1`, `gr.json@1` and `gr.sha256-hex@1`. JSON values use the existing bounded canonical domain; arbitrary floating-point values are not introduced into canonical JSON. The new buffer carrier supports separately registered array or other data profiles, without adding universal tensor semantics. [Buffer and numerical contracts](buffers.md) define physical ownership, layout, encoding and optional operation guarantees.

Declarative byte constraints support `min_bytes`, `max_bytes`, `byte_length` and `alphabet` (allowed bytes from a UTF-8 string). JSON constraints support `kind`, `required_keys`, `min_items`, `max_items`, `min_value` and `max_value`. Numeric bounds apply to exact integers, item counts to arrays and required keys to objects. Unsupported keys, malformed constraints, conflicting kinds and contradictory bounds are rejected at registration. Nested/domain-specific rules can use separately installed validators and must be stated in the type meaning. These limited constraints are not a general schema language.

Buffer constraints support `min_buffers`, `max_buffers`, `required_buffers`, `max_logical_bytes` and `max_retained_bytes`. Exact element encodings, shape relationships and domain-specific content checks belong to the separately installed logical type validator. Numerical operation descriptors are optional and fingerprinted: omitting the field preserves old descriptor identities. Declaring precision, overflow, reduction order or reference tolerances does not prove provider correctness or permit implicit numerical relaxation.

The built-in operation contracts are:

| Operation | Signature | Runtime postcondition |
| --- | --- | --- |
| `gr.bytes.identity@1` | bytes → bytes | Exact byte equality |
| `gr.bytes.concat@1` | bytes, bytes → bytes | Exact ordered concatenation |
| `gr.bytes.sha256@1` | bytes → SHA-256 hexadecimal bytes | Output representation and shape; algorithm correctness is assessed separately |

Effects can describe pure computation, snapshot reads, history mutation or external effects. Determinism can describe exact, declared approximate or nondeterministic results. The current host backend executes only **pure, exact** contracts. Other descriptions can be registered and inspected, but execution rejects them before invoking a provider. This vocabulary does not confer permission for any effect.

## Physical execution

The caller explicitly supplies the semantic operation, physical implementation and backend:

```cpp
gr::kernel::ValueTypeRegistry types;
gr::kernel::OperationRegistry operations;
gr::execution::ImplementationRegistry implementations;
gr::execution::install_cpu_implementations(implementations, operations);

gr::execution::ThreadedCpuBackend backend;
gr::execution::ExecutionOptions options;
options.max_threads = 4;
const auto result = backend.execute(
    types, operations, implementations,
    "gr.bytes.sha256@1", "gr.cpu.sha256.incremental@1",
    {{gr::kernel::Value::bytes("gr.bytes@1", {0, 1, 255})}}, options);
```

The two backends are `cpu.sequential` and `cpu.threaded`. Device discovery reports the host CPU as `cpu:0` and the hardware concurrency reported by the platform. It does not infer GPU availability, NUMA placement, accelerator memory or reserved machine capacity.

The implemented binding is `gr.host-values/1`: synchronous borrowed, const inputs valid until the call returns, and owned outputs. Providers must not retain input references. Immutable buffer owners can pass between native values without copying; Python's optional read-only exporter has a separate retained lease. There are no claimed device transfers or out-of-process isolation. Invocation argument order and batch result order are stable; completion order need not be.

Provider compatibility requires the exact operation identity and descriptor fingerprint, determinism, selected backend, CPU device kind and host memory. All batch input signatures, values and aggregate input limits are checked before the first physical callback. Output signatures, values and native postconditions are checked after each callback. A failure throws an error and returns no partial batch. Every started worker is joined before returning or throwing. A failure does not roll back side effects from a provider that violated its pure-code obligation.

Both SHA-256 providers target the same operation. `gr.cpu.sha256.oneshot@1` hashes a whole buffer; `gr.cpu.sha256.incremental@1` processes 4,096-byte chunks and polls cancellation between chunks. Their provider code paths are separately written but share the pinned PicoSHA2 cryptographic primitive. They are not two independently developed SHA-256 algorithms. The other providers are `gr.cpu.identity.copy@1` and `gr.cpu.concat@1`.

A provider declaring `thread_safe=false` is rejected by the threaded backend. Sequential calls using the same registered provider serialize callback execution across concurrent callers. This serialization applies within one registry entry, not to separately registered callbacks sharing undeclared global state. Providers must not re-enter their own non-thread-safe registration while holding its execution lock.

## Resource and cancellation boundaries

Defaults are 65,536 batch items, 64 MiB aggregate input payload, 64 MiB aggregate output payload and one compute worker. Zero-byte values still consume the item budget. JSON payload size is its canonical serialized byte size; byte payload size is its length. Each buffer value charges the larger of logical element volume and distinct retained allocation bytes, plus its layout/metadata. Aliases across values can be charged again. These limits exclude container overhead, operation descriptions, temporary serialization and native allocations outside the buffer allocator. Output caps are checked after the callback returns and cannot stop a callback allocating excessive memory. An explicitly supplied `AllocationBudget` additionally enforces native buffer allocation lifetime through final owner release.

`max_threads` bounds compute workers in one call. A supplied deadline can use one additional monitor thread. Separate callers have separate budgets; there is no process-wide scheduler or reservation service. A nonempty sequential call executes on the calling thread. An empty call uses no compute worker.

`std::stop_token` cancellation and steady-clock deadlines are cooperative. The deadline monitor requests the same stop signal observed by providers. Built-ins check at their documented boundaries; native validators and a noncooperating callback cannot be forcibly interrupted. The backend always joins workers, so it cannot promise a hard return deadline. A caller cancellation reports `execution_cancelled`; an expired deadline reports `execution_deadline`. Capability and result metadata expose these scopes instead of claiming hard isolation.

## Extension and conformance

A C++ consumer can register a domain type, state and pin a semantic operation, then independently register multiple physical providers. The installed-consumer example and `tests/conformance/operations.cpp` exercise this path without changing kernel or execution source. The conformance fixture uses a JSON pair type and two implementations of a multi-input/multi-result operation. Tests also exercise changed same-ID type/operation descriptions, unsupported effects, malformed values and provider mismatch before execution.

SHA-256 tests use fixed expected hashes generated independently with Python's `hashlib`, including block and provider-chunk boundaries. Seeded generated inputs compare providers and backends. A deliberately lying provider returns a well-formed but incorrect digest: runtime representation checks accept its shape, while the independent expected-output suite detects the error. Passing these bounded suites is evidence for those tested contracts and inputs, not universal semantic equivalence or certification.

The unit suite separately checks ownership, ordering, resource limits, callback concurrency, cross-caller serialization, exceptions with joined workers, cancellation and deadline behavior. Extension descriptors do not self-certify conformance or grant permission to run untrusted code.

## Migration and next boundaries

The v0.1 C++ execution `OperationRegistry` that combined meanings and callbacks is intentionally replaced. New native consumers use the three registries above. The adapter can retain the old Python `identity` and `sha256` conveniences by mapping them to explicitly pinned semantic/provider identities; the typed API exposes the complete selections. No second legacy registry is retained.

Record-history transformations remain direct kernel functions with their own contracts. They are not forced through byte computation or assigned placeholder opcodes. Layer 4 now adds a catalog, prepared typed DAG planner, scheduler, resource admission and transaction coordinator. Its catalog also installs a bounded record-verification semantic operation. Prepared L5/L6E handles capture exact descriptors and trusted validators/providers once while retaining operand/pre/postcondition checks. The direct batch API still works independently.

An automatic algebraic optimizer, bytecode VM, general DSL, dynamic binary plugin ABI, remote execution and scientific numerical equivalence rules remain unimplemented. Future additions require a concrete consumer and explicit contracts.

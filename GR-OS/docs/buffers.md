# Host buffers, ownership and numerical contracts

**Class:** Experimental implementation contract. **Version:** GR-OS 0.4. This is not an adopted programme Standard or a claim of general numerical-framework conformance.

## Boundaries

`gr_buffer` supplies `gr/memory/buffer.hpp` and depends only on common error, JSON and hashing support. It owns physical host allocations and checked views. Layer 5 adds a `Buffers` value representation, semantic type constraints and optional numerical operation contracts. Layer 6E executes registered providers under those contracts; Layer 4 retains values across graph planning, scheduling and completion. Neither buffer geometry nor a physical encoding ID determines a mathematical tensor, graph, unit of measurement, missing-value rule or scientific interpretation.

A `BufferSet` contains named views and bounded metadata. One profile may assign a single view to a dense column. Other profiles can assign views to offsets, values, validity bits, graph incidence or sparse indices. Their consistency and mathematical meaning belong to registered logical value types and operations. Unknown physical encodings can be retained; accepting their layout is not semantic validation of their contents. No general numerical promotion, implicit conversion or unfamiliar mathematical ontology is installed in the kernel.

## Allocation and lifetime

`Buffer` is an immutable shared owner of an aligned host allocation. `Buffer::copy` copies caller bytes. `MutableBuffer::allocate` zero-initializes storage, including bytes not subsequently written, and returns a move-only builder. Its move-only `MutableView` provides one exclusive scoped mutable borrow. Another borrow or `freeze()` fails while that lease exists. Freezing consumes the builder without copying its allocation. A mutable lease can outlive its builder and still owns valid memory.

Raw spans returned by a lease are borrowed C++ references: retaining one after the lease ends violates the API contract. C++ cannot revoke escaped pointers or prevent arbitrary `const_cast`/foreign native writes. Thread-safe shared immutable reads are supported; builder methods require external synchronization. Native callbacks remain trusted code.

Copies of `Buffer`, `BufferView`, `BufferSet` and `Value` retain ownership. Closing an SDK handle, destroying a catalog or evicting a plan does not invalidate another owning view. Slices retain the complete source allocation. `owner_id()` exists only for process-local accounting; descriptors and content identities never serialize addresses.

`AllocationBudget` atomically charges requested allocation bytes before allocation and releases the charge after the final mutable or immutable owner dies. Allocation failure restores the charge. Several allocators/importers can share one budget. This charge survives aliases extracted from a runtime result; it differs from the runtime's result-lease quota. It excludes allocation headers, alignment allocator overhead, JSON containers, native callback temporaries outside this allocator and process memory. Without an explicitly supplied budget, native callers retain responsibility for allocation limits.

## Typed views and bounds

`ElementDescriptor` names a versioned physical encoding, item size, alignment, explicit little/big/not-applicable byte order and bounded parameters. It does not certify that an arbitrary encoding ID matches the supplied bytes. Known-type adapters and semantic validators must check the complete descriptor they interpret. Alignment is an actual guarantee checked against origin and strides, not permission to reinterpret unrelated C++ object storage. Providers can read scalar encodings with byte operations or `memcpy`; they must still respect C++ object lifetime and aliasing rules.

`BufferView` retains its owner and fixes shape, signed byte strides and byte offset. Construction checks:

- At most 64 dimensions; valid element size/alignment and bounded descriptors.
- Checked shape products and logical byte size, with no wrapping integer arithmetic.
- Reachable minimum and maximum addresses for positive, negative and zero strides.
- Consistent rank and actual alignment at every reachable element origin.
- Explicit empty layouts and rank-zero scalars. Empty layouts need no accessible element; their offset may equal owner size.

Negative strides and broadcast/overlapping read-only views are supported. The minimum signed stride is rejected because its magnitude exceeds the supported signed host offset range. Mutable strided/overlapping views are not exposed. `slice(axis,start,count,step)` uses explicit nonnegative start/count and a nonzero signed step. It does not apply Python's implicit negative-index normalization.

`contiguous_bytes()` rejects non-C-contiguous layouts. `element_bytes(indices)` checks coordinates. `copy_contiguous()` copies logical elements in row-major index order, with cooperative stop checks; it does not swap byte order, promote types or normalize NaNs. Empty copies preserve their explicit shape. No foreign/device pointer adoption, indirect pointer-based layout or fabricated accelerator transfer is implemented.

## Identity and accounting

Allocation digests hash all owned bytes and are cached after freezing. View identity includes its physical descriptor, offset, shape/strides and allocation content digest. Set identity includes ordered names, view identities and metadata. These identities exclude addresses and sharing topology. A changed backing allocation outside the selected view can therefore change its physical identity; this is not mathematical equality. Creating a packed copy deliberately creates a different physical representation.

Three quantities are exposed:

| Quantity | Meaning |
| --- | --- |
| `logical_bytes()` | Sum of view element counts multiplied by element sizes; includes repeated broadcast elements |
| `retained_bytes()` | Full allocation bytes, counting each distinct owner once within a set |
| `accounted_bytes()` / buffer `Value::byte_size()` | Maximum of those quantities, plus bounded serialized layout/metadata bytes |

The conservative charge handles both a tiny slice retaining a large allocation and a huge broadcast view backed by a tiny allocation. Aliases across separate values or graph nodes may be charged again. This is deliberate bounded accounting, not a measurement of unique process heap usage. Existing byte and JSON values retain their previous charge. JSON remains in the bounded integer-based canonical domain; floating-point arrays live in explicitly typed buffers.

Prepared CPU execution and runtime admission use the new value charge without changing callback ownership. Planning hashes buffer descriptors/content, never raw process pointers. Existing JSON/octet literal graph hashes and operation descriptor serialization remain unchanged. Keeping a prepared plan avoids repeating allocation content hashing; immutable allocation digests are cached.

## Numerical declarations

`gr/kernel/numerical.hpp` defines an optional `NumericalContract` on an operation descriptor. When absent it adds no serialization field, preserving existing operation fingerprints. When present it is part of the exact operation fingerprint pinned by providers. It declares physical storage and accumulation formats, integer overflow behavior, rounding, nonfinite handling, reduction order, reassociation permission and repeatability scope. A referenced approximation contract includes finite nonnegative decimal-string absolute/relative tolerances and a versioned reference identity.

The declared tolerance comparison is `abs(actual-reference) <= absolute + relative*abs(reference)`. It is not an executable proof, a universal error estimator or a substitute for specifying a reference computation. No automatic tolerance-based acceptance is added to the executor. Optional trusted postconditions and independently generated conformance cases assess concrete operations. A provider's fingerprint only identifies its claim.

Accuracy relative to a mathematical reference and repeatability of physical output bits are different. The existing coarse determinism vocabulary remains for compatibility; numerical descriptors add explicit repeatability scope. Current execution still accepts pure exact operations only and requires their numerical repeatability across compatible providers. Declared approximate/nondeterministic descriptions are inspectable but are rejected before callbacks. Future numerical execution needs an explicit acceptance policy, without silently relaxing existing exact contracts.

`checked_add` and `checked_multiply` implement signed int64 arithmetic with explicit `numeric_overflow` errors, including minimum-integer edge cases, without signed-overflow undefined behavior. Ordered checked reduction cannot be reassociated freely: `(MAX + 1) + (-1)` fails while `MAX + (1 + (-1))` succeeds. Floating-point contraction, signed-zero/NaN payload conventions, execution environment and particular fixed reduction trees need operation-specific definitions before claiming corresponding equivalence. This increment does not install a general floating-point engine.

## Ecosystem interfaces

The [Python buffer protocol](https://docs.python.org/3/c-api/buffer.html) supplies format, shape, byte strides and a retained exporter reference released with the acquired view. Its read-only flag governs that view; it does not establish that no other alias can mutate the allocation. Arbitrary Python imports therefore copy before asynchronous use. An optional genuine read-only Python exporter retains a native allocation lease for outstanding consumers; raw pointer lifetime must not depend on a surrounding context manager alone.

[NumPy's array interface](https://numpy.org/doc/stable/reference/arrays.interface.html) likewise separates shape, byte order, strides and read-only state, and requires consumers to retain the owning object. NumPy is an optional consumer, not a native-library dependency. Adapter tests establish only the exercised encodings/layouts.

[Arrow's columnar format](https://arrow.apache.org/docs/format/Columnar.html) distinguishes physical layout from semantic type and supports multiple buffers and nested structures. Its [C data interface](https://arrow.apache.org/docs/format/CDataInterface.html) provides explicit release callbacks and can be used without linking Arrow C++. These facts inform the named-buffer design; GR-OS does not claim that a `BufferSet` itself is an Arrow array.

[DLPack](https://dmlc.github.io/dlpack/latest/c_api.html) additionally specifies tensor/device metadata, managed lifetimes and version-dependent requirements. Its strides are in elements, unlike this API's byte strides. GR-OS v0.4 does not advertise DLPack or device-buffer support merely because it has shape and ownership fields. Future adapters must implement the relevant complete versioned contract and synchronization rules.

## Verification and limits

`tests/unit/buffers.cpp` covers builder leases, zero initialization, alignment, negative/broadcast/empty/scalar layouts, slicing/copying, arithmetic failures, stable identities, alias accounting and concurrent allocation budgets. `tests/unit/numerical.cpp` covers int64 boundaries, an independent small-integer multiplication oracle, malformed tolerances, reduction-order constraints and descriptor identity. `tests/conformance/buffer_execution.cpp` exercises direct/prepared/threaded parity, wrong types and limits, plan-owned asynchronous lifetimes, cancellation joining and final-owner budget release.

SDK exporter, installed-consumer and experimental Layer 3 tests cover their own adapters. Passing these tests does not establish all NumPy/Arrow/DLPack formats, arbitrary device safety, a process-wide memory limit or numerical correctness of every registered provider.

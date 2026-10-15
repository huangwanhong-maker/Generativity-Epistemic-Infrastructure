# Computational interoperability boundaries

Class: experimental implementation contract and adapter assessment. Version: 0.6.
External references were checked against primary project documentation on 2026-10-15.

## Native host data

Logical types and representations remain independent of any array library. Public
constructors accept bounded standard C++ spans and create immutable owned buffers.
`memory::ElementRange` retains a validated rank-one buffer view and supports
contiguous, reversed, strided and broadcast layouts. Fixed-width loads/stores use
byte copies or explicit endian conversion; they do not assert that C++ numeric
objects were constructed inside the raw byte allocation. Scalar checked access
remains available. A view's physical hash is not mathematical equivalence across
different layouts.

Importing mutable external memory requires an owned copy in this version. Exported
read-only views retain native storage after the originating language handle closes.
Copying into mutable caller storage is explicit. Allocation budgets track owned
native allocations, not Python arrays, callback temporaries or total process memory.

## NumPy

The Python SDK uses explicit dtype/byte-order interpretation and owned import,
with read-only contiguous export or an explicit copy for supported strided layouts.
Nullable columns require a separate validity policy; a NumPy array alone does not
carry GR source qualifications or null semantics. Numerical dtype support is
declared by the installed modules and tested in the SDK, not inferred from item
width. Mathematical promotion and numerical narrowing are not automatic.

This follows NumPy's distinction between buffer interpretation and copying:
`frombuffer` views the original storage, and nonnative byte order belongs in its
dtype description. Mutable/untrusted source storage needs an explicit ownership
decision. [NumPy frombuffer documentation](https://numpy.org/doc/stable/reference/generated/numpy.frombuffer.html).

Converting a qualified value to a plain array does not preserve its interpretation
merely by preserving numeric bytes. Retain the source DataValue/recipe or export an
explicit sidecar under an application-defined custody policy. The framework does
not automatically claim a later reimport is the same historical observation.

## Independently distributed packages

A native source package can register families, representations and operators in
the caller's Library and use its Function/Runtime. CMake relocation and exact SDK
dependency checks establish the tested source-build boundary. They do not establish
binary compatibility between compilers, runtimes or arbitrary future SDK releases.

A package-specific C facade can give Python access to the same native semantics
without a general callback/plugin ABI. Such a facade owns its own engine and handles;
opaque handles from another engine are not transferable. An explicit copied-data
boundary is acceptable and must remain visible. Native code has process authority;
metadata and registration do not sandbox code or authorize protected data access.

## Deferred adapters and their actual obligations

| Ecosystem | Valuable use | Contract still needed |
|---|---|---|
| Arrow C Data Interface | Columnar exchange without a mandatory Arrow C++ dependency | Schema/offset/nested/null bitmap mapping, move/release lifecycle, immutable owners and qualified metadata policy |
| SciPy sparse | Existing sparse algorithms and familiar Python CSR access | Exact index/shape types, explicit copy ownership, sorted canonical coordinates, duplicates and checked arithmetic policy |
| DLPack | Dense strided device-aware array exchange | Device/stream synchronization, capsule lifecycle, copy flags and immutability policy |

Arrow supplies a small ABI and producer release callbacks for same-process exchange;
it does not make our present named-buffer layout Arrow-compatible automatically.
Its persistence/IPC contract is separate. [Arrow C Data Interface](https://arrow.apache.org/docs/format/CDataInterface.html).

SciPy permits sparse inputs whose duplicate coordinates are summed during canonical
conversion. GR's initial exact CSR contract rejects duplicates, because silently
consolidating them may change overflow and traversal behavior. A future adapter
must choose and name the transformation. [SciPy CSR documentation](https://docs.scipy.org/doc/scipy/reference/generated/scipy.sparse.csr_array.html).

DLPack describes dense strided arrays and requires explicit cooperation over device
streams and copy/view behavior. It is not a graph, sparse-history or ownership
immutability protocol by itself. [DLPack Python specification](https://dmlc.github.io/dlpack/latest/python_spec.html).

These adapters remain deferred; no dependency or compatibility badge is added
without a working implementation and conformance evidence. NumPy and the independent
native package provide the bounded working examples for this increment.

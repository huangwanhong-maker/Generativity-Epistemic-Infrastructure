# Layer 3 values and standard operators

**Class:** Experimental implementation contract. **Version:** GR-OS v0.6. This
document describes bounded computational packages, not an adopted programme
Standard or a universal mathematical ontology.

## Logical types and representations

The native interfaces are [standard.hpp](../include/gr/data/standard.hpp) and
[sparse.hpp](../include/gr/data/sparse.hpp), using the generic
[language.hpp](../include/gr/data/language.hpp) extension contracts. Installing a
package registers logical families, representation validators and operator
specializations. The kernel and runtime do not acquire switches for these types.

`install_standard` provides exact signed int64, Boolean and finite binary64
scalar families, `optional<T>`, `column<T, nullable>` and ordered named tables.
An optional value expresses presence or missingness; it does not assign a cause,
uncertainty interval, probability or epistemic warrant. Tables have ordered rows
and distinctly named column types. Dynamic lengths are value properties, so a
new chunk length does not create a new logical type. Table schema and nullability
are type properties, with no implicit promotion or nullability coercion.

The standard representations support the three implemented scalar encodings;
the generic type constructors do not imply support for every possible child
representation. Other packages can supply compatible representations and
operators. No units, automatic dimensional analysis, strings, arbitrary nested
table storage or heterogeneous scalar union is implicitly provided by these
particular standard representations.

| Representation | Stored form |
| --- | --- |
| `gr.data.scalar-le@1` | One rank-zero `values` view; optional scalars add a rank-zero `validity` view |
| `gr.data.column-buffers@1` | Rank-one `values`, with equally shaped `validity` when nullable |
| `gr.data.table-buffers@1` | `cN.values` and optional `cN.validity` views, with explicit `rows` metadata |
| `gr.data.csr-i64@1` | Independently installed sparse representation with `indptr`, `indices`, `values` and rows/columns metadata |

Int64 uses explicitly little-endian two's-complement bytes. Finite binary64 uses
explicitly little-endian IEEE binary64 bytes and preserves signed zero; NaNs and
infinities are rejected, including in physically stored missing slots. Boolean
and validity bytes must each be zero or one. Validity uses **one byte per slot**,
not Arrow's packed bitmap. Strided and negative-stride column views are supported.
Layouts and encoding descriptors are checked before providers interpret them.

`DataValue` combines the logical/representation specification, owned kernel
payload and provenance. Payload content digests identify physical representation,
including its owner bytes and views. They are not a universal mathematical
equality relation: a packed copy and a strided view may have equal logical
elements and different physical digests. No structural common-subexpression
elimination is justified by assuming those notions are interchangeable.

## Construction and ownership

`scalar_i64`, `optional_i64`, `column_i64`, `column_bool`, `column_f64`, and
`make_table` construct native values. Full int64 scalar operands use buffers;
they are not clipped to canonical JSON's exact metadata integer domain.
`table_column` returns an owning shared column view. The `to_i64`, `to_bool` and
`to_f64` extraction helpers return C++ vectors of optional elements;
`get_scalar_i64` and `get_optional_i64` distinguish ordinary and nullable scalars.
Extraction helpers copy into caller-owned standard-library containers.

Constructors and installed providers accept `StandardOptions`. Defaults are
1,048,576 elements, 64 columns and a shared 256 MiB allocation budget. A caller
can pass the same options to construction and module installation to share one
budget. Separate default options create separate budgets; this is not a global
process-memory cap. Configured element limits are bounded at 16,777,216 and
column limits at 64. Limits affecting accepted representations appear in their
descriptors and therefore in binding fingerprints.

Computed buffer allocations retain their budget through their final owner,
including exported values surviving plan/runtime/handle destruction. Views,
table assembly, projection and slicing share allocations. Affine transformation,
comparison, filtering and reduction allocate result storage. Filter selection
indices and the reference CSR provider's decoded vectors are bounded temporary
containers outside the buffer allocation budget. Registry, validation, provenance
and ordinary C++/Python allocations also have distinct costs. L4 retains its
separate conservative payload reservation contract.

`make_table` preserves supplied column provenance in its construction metadata;
language operations attach the generic expression/source lineage on
materialization. Direct table-column extraction retains its source provenance.
Such metadata records computational derivation, not proof of a source claim or
authorization to publish a new historical fact.

## Operator contracts

All installed operations are pure with exact declared semantics. Their concrete
contracts are generated from inferred input/output specifications and pinned
attributes, then registered with the existing kernel, physical implementation
registry and runtime planner. The native implementation catalog remains sealed.

| Operator | Inputs and explicit attributes | Result |
| --- | --- | --- |
| `gr.data.slice@1` | Column; `start`, `count`, optional nonzero `step` | Same logical column type, shared data/validity views |
| `gr.data.affine@1` | Int64 column, int64 multiplier scalar, int64 addend scalar | Checked multiply followed by checked add for each present element |
| `gr.data.compare@1` | Int64 column and scalar; `relation` in lt/le/eq/ne/ge/gt | Boolean column with source nullability |
| `gr.data.filter@1` | Column and Boolean predicate column; `nulls=error/drop` | Stable selected rows |
| `gr.data.sum@1` | Int64 column; `nulls=skip/propagate`, optional `min_count` default 1 | Optional int64 scalar |
| `gr.data.table-column@1` | Table; `name` | One typed owning column |
| `gr.data.table-project@1` | Table; distinct ordered `names` | Reordered/subset table, preserving row count even with zero columns |
| `gr.data.table-filter@1` | Table and Boolean predicate column; `nulls=error/drop` | Stable row selection across every field |

Affine transformation skips missing slots, writes zero to their result data and
retains the input validity view. Comparison likewise preserves missing predicates
instead of treating them as false. Filtering requires an explicit missing-mask
policy and equal input/predicate lengths; it never silently broadcasts a mask.

Reduction first determines whether the result is missing: insufficient present
elements, or any missing element under propagation, returns missing before doing
arithmetic. Otherwise it adds present int64 elements from left to right with a
checked overflow at each prefix. An empty input with `min_count=0` yields zero;
the default `min_count=1` yields missing. The sequence `[MAX, 1, -1]` fails while
`[MAX, -1, 1]` succeeds. Reassociation and replacing checked multiply/add with one
wide intermediate would change these contracts and are not permitted rewrites.

Finite binary64 currently participates in construction, table storage, selection,
slicing and filtering. This package deliberately installs no floating arithmetic,
approximate execution, NaN/missing equivalence or complex-number operations.
Those need explicit operation contracts and acceptance policies; supporting a
storage encoding does not establish numerical correctness.

## Independent sparse mathematical extension

`install_sparse` adds the new logical `gr.data.matrix@1` family, its CSR
representation and `gr.sparse.matvec@1`. The matrix family is absent from a
standard-only library. It is a mathematical matrix with one element type; rows
and columns belong to values, and CSR is one representation rather than the
matrix's definition. The separate target depends on the standard package for
int64 and result-column contracts, and does not modify language, kernel or runtime
code to register its mathematical objects.

`csr_i64` checks row pointers, lengths, dimensions, bounds and strictly increasing
column indices in every row. Duplicates and unsorted indices are rejected;
entries are not silently summed, reordered or coerced. Explicit stored zeros are
allowed; absent entries mean exact zero, not missing data. Row count, column
count and stored-entry count each have the configured element limit. A sparse
matrix's conceptual dense extent can therefore exceed its stored element count.

Matvec requires a nonnullable int64 column of exactly the matrix's column count.
For each row it computes checked products and checked sums in ascending stored
column order, starting at zero. Empty rows yield zero. The output is a dense
little-endian nonnullable column. `gr.sparse.cpu.matvec-direct@1` reads the checked
views directly; `gr.sparse.cpu.matvec-decoded@1` first decodes temporary native
vectors. Both preserve the declared arithmetic order and support explicit
provider selection. Neither claims internally parallel row execution or SIMD.

The sparse conformance suite compares both providers with independently generated
small dense-matrix expectations, then checks malformed layouts, optional-vector
rejection, shape mismatches and multiplication/prefix overflow. The standard
suite exercises a heterogeneous nullable table pipeline, serial/parallel parity,
input templates, view lifetime, allocation rollback and numerical edge cases.

## Interoperability and limits

[NumPy's array interface documentation](https://numpy.org/doc/stable/reference/arrays.interface.html)
describes shape, byte order, strides and owner retention, and recommends modern
buffer/DLPack protocols over its legacy capsule interface. GR's existing genuine
read-only Python exporter provides an ownership boundary for supported physical
encodings. Mutable foreign inputs are copied; an immutable view flag alone does
not make a foreign allocation immutable. Adapters must report packing, byte-order
conversion and missingness conversion when required.

[Arrow's format](https://arrow.apache.org/docs/format/Columnar.html) has prescribed
type layouts, nested children and separate validity information. These GR values
are not Arrow arrays merely because both use multiple buffers. In particular,
GR's byte validity representation needs explicit bitmap conversion. An adapter
must preserve null semantics and release ownership correctly.

[SciPy CSR](https://docs.scipy.org/doc/scipy/reference/generated/scipy.sparse.csr_array.html)
defines canonical sorted/unique rows and permits duplicate consolidation during
construction. GR's checked-order contract instead requires an explicit decision
before any duplicate combination. A SciPy adapter should reject noncanonical
input or make that transformation visible; dtype conversion is also explicit.

[DLPack](https://dmlc.github.io/dlpack/latest/python_spec.html) includes version,
single-consumption ownership and device-stream requirements. No DLPack/device
contract is implemented here. Likewise,
[NetworkX adjacency conversion](https://networkx.org/documentation/stable/reference/generated/networkx.convert_matrix.from_scipy_sparse_array.html)
distinguishes edge weights, multiplicity and stored-zero edges. A mathematical
matrix or adjacency projection is not itself a qualified GR relational history.

All packages remain bounded eager host providers beneath lazy expressions. Input
templates can process explicitly supplied finite chunks; there is no automatic
stream source, backpressure engine, distributed iterator, implicit table join,
general dataframe optimizer or storage pushdown. Representation validation scans
elements or stored sparse entries when no exact immutable validation evidence is
available. Version 0.6 adds library-local weak-owner validation reuse; see
[its scope and limits](data-language.md#bounded-immutable-validation-reuse).
Validation, compilation and computation remain separately measured costs. The
optional [numerical module](numeric-data.md) adds its own explicit unsigned and
qualified floating contracts without changing these existing operation identities.
Persistence codecs and historical publication of typed results remain explicit
future contracts, distinct from process-local value handles and computation
lineage. No scientific modeling method or Layer 2 application is built into these
operators.

## Native benchmark protocol

The optional `benchmark_data <output.json>` executable measures 65,536 and
262,144 nullable table rows, plus 4,096 and 16,384 square tridiagonal CSR matrices.
Each case has one untimed warmup and five measured repetitions. The report keeps
all samples, medians, minima and maxima; its nearest-rank p95 is the maximum of
five samples and is not a tail-latency guarantee. Execute it on an idle host after
builds and tests finish.

Table phases separate owned input construction, expression inference using
already-created literals, fresh compilation with warmed immutable input hashes,
serial prepared DAG execution, and execution through an already-started runtime
with one or four workers. Compilation creates a fresh catalog and plan each time.
Prepared execution assembles and retains all intermediate invocations until the
DAG finishes; the runtime has its own intermediate release and accounting policy.
Consequently their timings include different lifetime-management costs.

The table workload checks a nullable affine transform, predicate, stable row
selection and ordered sum against independent host calculations. A separately
timed fused loop uses known bounded inputs and ordinary vectors; it is a useful
cost reference, not an implementation of the generic validation and lineage
contracts. CSR cases time construction and both direct-view and decoded-vector
providers, each checked against an independently calculated stencil result and
matching output buffer digests.

Checks, hashing of outputs, materialization of runtime lineage, and destruction of
the returned result are outside measured intervals. Internal intermediate
destruction still occurs inside execution. Reported owner observations count
requested native buffer allocations at node boundaries, including already-live
inputs; they exclude ordinary temporary vectors, metadata, allocation overhead
and process memory. Per-value retained-byte sums may count shared owners more
than once. No process-memory or whole-program speedup claim follows from these
measurements, and warmed compilation does not measure cold input hashing.

## Controlled access profiling

`benchmark_access <output.json>` isolates the new owning rank-one byte range from
the rest of the framework. It checks 256, 16,384 and 262,144 int64 elements, with
and without null validity, over contiguous, reversed and zero-stride broadcast
views. Each case uses one warmup and five samples; small cases repeat 64 scans
per sample and report per-scan time. Independent host-vector calculations check
the same ordered sum outside each timed interval.

The checked scalar path performs buffer-name lookup and full index/geometry
access per element. The batch path caches the validated owning range and retains
its immutable owner without copying payload bytes. Both paths use the same
fixed-width little-endian load and checked integer addition, isolating those
access costs. A third path explicitly copies all input views into contiguous
owned storage before scanning; its timing includes allocation and destruction,
and its reported copied payload bytes equal the views' logical extent.

This runner does not measure compilation, representation validation, transport,
or runtime scheduling. Its allocation peak includes the already-live input and
earlier layouts in the same case group; it is requested owner bytes, not process
memory. Samples are descriptive local measurements, without a tail-latency or
universal speedup claim. Run it only after builds/tests finish, with the matched
whole-pipeline benchmark retained as a separate experiment.

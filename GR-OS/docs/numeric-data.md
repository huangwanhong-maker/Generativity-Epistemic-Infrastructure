# Bounded numerical extension

Class: experimental implementation contract. Version: GR-OS 0.6. This module is
an arithmetic foundation, not a general numerical library or scientific method.

## Logical domains and physical representation

Install the standard module, then `gr::data::numeric::install`, before sealing a
library. C++ declarations are in `gr/data/numeric.hpp`; the SDK module identity is
`gr.data.numeric@1`. Existing standard type and operation descriptions remain
unchanged.

The extension adds `gr.numeric.uint64@1`, the integers from zero through
2^64−1, and `gr.numeric.complex@1` parameterized by a scalar domain. Its implemented
complex representation uses `gr.data.float64@1` components. Existing signed int64
and finite binary64 columns are also explicit supported inputs. Registration of
the general complex family does not supply every possible component-domain
representation or operation.

| Physical encoding | Layout |
|---|---|
| `gr.uint64@1` | One little-endian unsigned 64-bit slot |
| `gr.float64-ieee754@1` | One little-endian IEEE binary64 slot |
| `gr.complex128-pair@1` | Little-endian binary64 real component followed by imaginary component; 16 bytes, 8-byte alignment |

`gr.numeric.column-le@1` and `gr.numeric.scalar-le@1` are representation
identities. They are distinct from the logical element and column/optional
families. Nullable values have a separate byte-per-element 0/1 validity buffer;
complex validity applies to the pair. Even missing slots must contain finite
floating components. New providers write zero component bytes into missing slots.

Int64/binary64 column results use the existing standard column representation,
allowing existing table, filter and slice operations to consume them. Uint64 and
complex columns use the new numerical representation. The v0.5 table
representation still accepts its original element domains; it has not silently
acquired uint64/complex semantics under an unchanged descriptor. Generalizing
table composition over arbitrary representations is a separate extension.

## Operations and numerical meaning

`gr.numeric.add@1`, `multiply@1` and `equal@1` take equal-length columns of the
same logical element domain. There is no scalar broadcast, implicit signed to
unsigned conversion, automatic promotion or precision-changing cast. A result
element is missing if either input element is missing. `equal` returns Boolean
values and uses numerical component equality: +0 and −0 compare equal, despite
different physical bits. It is not approximate equivalence or universal logical
equality for all registered types.

`gr.numeric.sum@1` requires `nulls="skip"` or `"propagate"`. `min_count` defaults
to one. It first checks missingness and the present-element count; insufficient
count or propagation of any missing value returns an optional missing scalar
before arithmetic. Otherwise it accumulates present values in input order from
zero. Floating/complex initial components are positive zero. Thus an empty sum
with `min_count=0` returns zero and an all-negative-zero sum need not retain the
negative sign.

This new operation has its own identity and extends the same missing-first policy
already used by `gr.data.sum@1`. Once a present result is required, both check each
addition in input order: an overflowing prefix fails even if later values would
bring the mathematical total back into range. The original standard operation's
domain and descriptor remain unchanged; it is still signed-int64 only.

Signed and unsigned arithmetic rejects every overflowing addition/product;
there is no wrap or saturation. Floating and complex arithmetic rejects
nonfinite input components and any nonfinite arithmetic intermediate. Underflow
uses gradual subnormal results and correctly rounded signed zero. Overflow of an
intermediate complex product is rejected even if an alternative algebraic
formulation would produce a finite final answer.

The pinned `gr.binary64-ordered-finite@1` profile specifies nearest-even rounding,
binary64 precision for each operation, no contraction or reassociation, and this
complex multiplication sequence: `ac`, `bd`, `ac-bd`, `ad`, `bc`, `ad+bc`. Each
intermediate is rounded separately. Reductions do not use a parallel tree.

`Determinism::Exact` here means the specified machine result, including its bits,
under this qualified arithmetic profile. It does **not** mean exact real-number
arithmetic. For example, the specified sum of `[1e16, 1, -1e16]` is zero; summing
`[1e16, -1e16, 1]` gives one. Neither a numerical error bound relative to an exact
real reference nor a universal GPU/CPU reproducibility promise follows from
ordered evaluation.

The existing `ErrorTolerance` descriptor describes a comparison formula and a
reference identity. It is not an executed acceptance test, a proof of algorithmic
accuracy, or permission to run arbitrary approximate providers. Approximate
providers, tolerant equivalence rules, parallel floating reductions, nonfinite
domains, float32, transcendental functions, and implicit conversions remain
deferred. A future lossy conversion needs an explicit operator/policy and lineage;
NumPy's promotion choices are not silently adopted. In particular, NumPy may
promote mixed int64/uint64 to float64, which is unsuitable as an unnoticed exact
integer conversion. [NumPy promotion rules](https://numpy.org/doc/stable/reference/arrays.promotion.html)

Python construction with an explicit `dtype="float64"` or `"complex128"` is a
caller-requested conversion of Python values into that domain; large Python
integers may round during that conversion. It is separate from executing a
typed operator, where mixed domains are rejected. `from_numpy` accepts only the
documented exact-width dtypes and explicitly normalizes layout and byte order.

## Qualified floating environment

`floating_capabilities()` reports whether the build qualifies. Initial executable
floating support is restricted to genuine MSVC x64/SSE2, IEEE binary64 with
`FLT_EVAL_METHOD=0`, and an explicitly strict compiler configuration. Unsupported
builds still support representation/storage and checked integer computation, but
reject floating execution with `unsupported_numeric_environment`.

Each floating provider saves the caller's environment, selects nearest-even
rounding, masks hardware exceptions, disables FTZ/DAZ, and checks those controls.
It restores rounding, flags and MXCSR on success and C++ failure. This happens on
the executing thread; it does not change another worker's state. Pure computation
has no unrolled-back externally visible floating-environment mutation.

The numeric source and its environment conformance test use strict floating
compiler flags. Microsoft documents that `/fp:strict` preserves specified
ordering, permits environment access and disables floating contraction.
[Microsoft floating-point compiler options](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior?view=msvc-170)
For future ports, Clang's strict model also enables environment access and strict
exception semantics; its dynamic-rounding option alone is not the entire strict model.
[Clang floating-point controls](https://clang.llvm.org/docs/UsersManual.html#controlling-floating-point-behavior)
The GCC build path disables fast math and contraction and enables environment-aware
compilation. GCC explicitly describes `-frounding-math` as experimental;
compiler options alone are insufficient evidence of cross-platform conformance,
particularly on targets permitting excess precision.
[GCC optimization options](https://gcc.gnu.org/onlinedocs/gcc-15.1.0/gcc/Optimize-Options.html)

Clang and GCC builds currently report floating support as false, even if they
accept the strict flags; enabling their arithmetic requires a separate conformance
decision. The capability query reports the permitted compile-time profile plus
the stated runtime environment checks, not a certification of every compiler/version.
Only configurations actually exercised in the verification report have measured
conformance evidence; other ports need their own bit-fixture and environment
checks before being described as verified numerical platforms.

## Providers, bounds and evidence

`gr.numeric.cpu.<operation>-batch@1` uses an owning validated `ElementRange`;
`...-scalar@1` uses checked `BufferView::element_bytes` access. Both implement the
same pinned operation and arithmetic helpers. Their agreement checks two access
paths, not two independent floating arithmetic engines. Exact expected-bit
fixtures independently check halfway rounding, cancellation, subnormal results,
signed zero and component sequencing. Other tests cover integer boundaries,
missingness, shape/type refusal, overflow, initial cancellation, owner-budget
rollback and restoration from a hostile caller rounding/FTZ/DAZ environment.

Callbacks poll cancellation during bounded loops. Existing allocation budgets
track requested owned-buffer bytes, not temporary containers or process memory.
The numerical module checks its own `max_elements` bound on every input, including
standard columns created under a different, larger module limit. This bound is
pinned in the specialized operation metadata and checked before provider allocation.
The runtime can schedule independent nodes, but these providers do not create
another scheduler or claim internal SIMD/parallel reduction. Installed-consumer,
NumPy and benchmark outcomes belong in the final verification report; these
contracts alone do not claim those checks passed.

`gr/memory/access.hpp` is representation-neutral. Its owning rank-one byte range
retains immutable storage, checks random indices, supports positive, negative and
zero strides, and avoids forming invalid pointers after the final item. Fixed
physical integer loads use `memcpy`/byte assembly rather than treating raw bytes
as live C++ arithmetic objects. The existing scalar indexing API remains usable.

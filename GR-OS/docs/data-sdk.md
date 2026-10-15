# Layer 3 SDK

Status: experimental implementation, version 0.5. This is an operational software
API, not an adopted GR Standard or a claim that these data types exhaust GR theory.

The C++, Python and JSON interfaces share one native logical library, inference
implementation and lowering path. A `Type` has a qualified family, ordered type
arguments and bounded canonical parameters. A `ValueSpec` adds a separately named
physical representation. Shape and row count belong to values in the initial
supplied modules; extensions may define static shape or unit parameters. Native expressions
are immutable; creating a literal pins the supplied payload and provenance.

## Python

```python
from contextlib import ExitStack
import gr_os as gr
from gr_os import data

with ExitStack() as owned:
    context = owned.enter_context(data.Context())
    runtime = owned.enter_context(gr.Runtime(workers=2))
    column = owned.enter_context(context.column([1, None, 3]))
    source = owned.enter_context(context.input("chunk", column.spec))
    total = owned.enter_context(context.apply(
        "gr.data.sum@1", [source],
        attributes={"nulls": "skip", "min_count": 1}))
    program = owned.enter_context(total.compile(bindings={"chunk": column}))
    result = owned.enter_context(program.run(runtime)).raise_for_status()
    assert result.outputs[0].tolist() == 4
```

`Context` installs the standard module by default. Request
`modules=("gr.data.standard@1", "gr.data.sparse@1", "gr.data.relational@1")`
to add the supplied independent modules. The native library is sealed before a
context is returned. JSON cannot install executable callbacks; trusted C++ hosts
register extensions before sealing. Neither Python inference nor a Python
execution engine duplicates native semantics.

`Context.column` accepts bounded int64, boolean or finite float64 columns, with
explicit nullable types when missing values occur or `nullable=True` is selected.
`scalar` carries the full signed int64 range through binary storage. `table`
combines named heterogeneous columns of equal length. `csr` constructs canonical
int64 CSR matrices with validated row pointers and ordered unique column indices.
`import_value` accepts a `ValueSpec` and the existing typed kernel `Value` carrier;
the selected native representation validates it. Constructors copy mutable inputs.
`tolist`, relational `inspect` and `payload` are explicit export operations.

`Context.input` makes an immutable named expression template. Compile it with new
bindings for each bounded chunk. Each compilation produces a separately pinned
program; this is not a persistent streaming engine. `data.compile([expr1, expr2])`
supports shared subexpressions and multiple outputs. `Context.call` returns all
operator outputs; `apply` requires exactly one. A selection may pin a provider:
`selection={"implementation": "gr.sparse.cpu.matvec-direct@1"}`. The native
compiler resolves this to descriptor-pinned L4 contracts.

`Program.submit(runtime)` returns a `DataJob`, wrapping an ordinary L4 job and an
independent program lease. `await job`, `job.result()` and `job.wait(timeout)` use
the same scheduler. Async cancellation requests native cooperative cancellation;
it does not assert rollback. Materialization accepts an actual native job with
the exact plan identity, including the recipe's provenance context. Serialized
diagnostics cannot be used as substitute results. A failed job has no successful
typed outputs. `raise_for_status()` preserves native error details and attaches
both `exception.result` and `exception.diagnostics`.

Close resources explicitly or use context managers. Closing the original context,
input, expression or program after submission does not invalidate admitted work.
Returned values own immutable payload leases and survive runtime closure. A
`payload()` export owns separate buffer handles and must also be closed. The
existing runtime publication arguments may be passed through `Program.submit/run`;
outcome-unknown synchronous results retain a recovery job. Close that result, or
`exception.result`, after resolving the publication. Runtime close drains and joins
native workers, so trusted callbacks must cooperate with cancellation.

## NumPy boundary

NumPy is optional and is not a package dependency. `Context.from_numpy` imports a
one-dimensional `numpy.ndarray` of int64, float64 or boolean values by copying,
normalizing byte order and C layout, then validating natively. Other dtypes require
an explicit caller conversion. Nonfinite float64 values are rejected. Signed zero
is preserved. Imports are bounded to 1,048,576 elements.

`value.numpy(copy=False)` exports a contiguous, nonnullable supported column through
the existing genuine readonly CPython buffer exporter. The ndarray retains native
ownership even after the `DataValue` closes; it cannot be made writable. It requires
the optional `_gr_buffer` extension. `copy=True` produces a writable caller-owned
copy, including strided and negative-stride columns, and works without that extension.
Nullable exports require an explicit caller validity policy and are rejected by
this convenience adapter. The validity representation is one byte per slot, not
an Arrow bitmap. There is no Arrow C Data Interface, SciPy object or DLPack adapter.

## Qualified relational data

With the relational module installed, `context.history(store, workspace,
semantic_revision=..., physical_revision=...)` eagerly selects and pins a qualified
historical snapshot. Subsequent pure expressions do not reread mutable storage.
`gr.data.incidence@1`, `gr.data.select-incidence-roles@1` and
`gr.data.count-incidences@1` retain source qualifications and contributor references.
Use `inspect()` deliberately to decode a qualified container; it may be much larger
than the compact value description. See [relational contracts](relational-data.md)
for the exact preservation and temporal limits.

## C++ and build targets

Include `gr/data/language.hpp` and link `GR::data` for the generic language.
`GR::data_standard`, `GR::data_sparse` and `GR::data_relational` are separate
installed targets. Construct a `shared_ptr<Library>`, install required modules,
seal it, then use `literal`, `input`, `call/apply`, `compile`, `Program::plan` and
`Program::materialize`. The plan runs through the existing `gr::runtime::Runtime`.
See [native example](../examples/cpp/data_language.cpp) and
[Python example](../examples/python/data_language.py). `GR_BUILD_DATA=OFF` omits
the Layer 3 libraries and bridge while retaining L4–L6. Native targets have no
Python dependency.

Rebuild native C++ consumers against the matching SDK headers/libraries; experimental
C++ object layouts are not a stable binary ABI. The versioned C ABI remains additive.

## C ABI 1 and CLI session

The ABI remains version 1. Call the existing `gr_call_json` and release returned
responses with `gr_free`. `sdk.capabilities.features` contains `data_language`
only when these modules are compiled. All commands use `{abi:1, op:..., ...}`;
unknown fields are rejected. Binary constructor input uses existing immutable
buffer handles with the default opaque-octet descriptor, containing complete packed
little-endian items. Declared typed buffers are rejected by this staging interface;
use `data.import` with their explicit logical/physical spec instead. This avoids
silently reinterpreting a conflicting endian descriptor and avoids JSON int64 loss.

| Command | Purpose / principal fields |
| --- | --- |
| `data.open` | Select `modules`; return context handle and capabilities |
| `data.create` | Context `handle`, `kind`, binary `values` and optional `validity`; table `columns`; CSR `rows`, `columns`, `indptr`, `indices`, `values` |
| `data.import` | Context `handle`, `value:{spec,payload,provenance}` |
| `data.history` | Context `handle`, `store`, `workspace`, optional semantic/physical revision |
| `data.literal` / `data.input` | Context plus value handle, or name and spec |
| `data.call` | Context, `operation`, expression `inputs`, `attributes`, `options` |
| `data.compile` | Expression `outputs`, optional named value `bindings`; return program and ordinary plan handles |
| `runtime.submit` | Existing runtime handle and the returned ordinary plan handle |
| `data.result` | Program `handle`, actual `job`, optional bounded `timeout_ms`; return typed owned values |
| `data.value.payload/items/inspect/describe` | Explicit value exports or compact description |
| `data.value/expr/program.retain` | Independent owned handle |
| `data.close`, `data.value/expr/program.close` | Release one owned handle |

The process-local limits are 32 contexts, 4096 value handles, 8192 expression handles
and 1024 program handles; programs also consume the existing 1024-plan capacity.
Native module data limits and runtime payload admission limits apply separately.
Newly created response handles are rolled back if response serialization fails.
`gr session` accepts these same commands as bounded JSON lines. A standalone command
cannot meaningfully return a process-local resource for later processes; use the
session or an embedding host for multi-step language work.

## Reusable functions and scoped validation reuse (v0.6)

`data.Function(outputs)` freezes native expression roots and exposes immutable
`inputs` specifications. `function.bind({name: value})` compiles an ordinary Program;
`function.instantiate({name: expression})` composes the template with expressions in
the same sealed native Library. The C++ equivalents are `gr::data::Function`,
`bind` and `instantiate`. Rebinding is explicit; there is no hidden streaming engine.
Function and Program handles retain their native owners after Context/Expr closure.
Close all returned expressions and values, or use context managers.

`Context(validation_cache={"max_entries":64,"max_bytes":16777216,
"profile_timing":False})` configures a library-local validation cache. The
`validation_statistics()` and `clear_validation_cache()` methods expose its bounded
state; zero entries disables reuse. Payload owners are weakly held, and exact native
specification, transport tag and library context qualify reuse. These limits are
cache charges, not a process memory bound. `relational_cache` accepts the same option
shape for the installed relational Session. `context.inspect(value)` uses that
session; `value.inspect()` remains the independent uncached inspection path.
`relational_statistics()` and `clear_relational_cache()` are scoped to the Context.

The additive ABI commands are `data.function.create` (`outputs`),
`data.function.bind` (`handle`, named value `bindings`),
`data.function.instantiate` (`handle`, named expression `bindings`), and
`data.function.describe/retain/close`. At most 1024 function handles are retained.
`data.validation.statistics/clear` and `data.relational.statistics/clear/inspect`
mirror the Python methods. The CLI session accepts the same requests. No pointer or
callback registration was added to the C ABI.

## Explicit numeric module (v0.6)

Select both `gr.data.standard@1` and `gr.data.numeric@1` in Context modules, or link
`GR::data_numeric` and call `gr::data::numeric::install` after standard installation.
The module adds full-range uint64 and finite complex128 values, finite float64
scalar construction, and checked/exactly specified arithmetic and reductions.
`context.scalar(value,dtype=...)` and `column(...,dtype=...)` accept `uint64`,
`float64` and `complex128` explicitly. Choosing a floating constructor is an explicit
Python numeric conversion and can round an integer operand. Operator inference
does not silently promote mixed numeric domains. Missingness stays explicit.

NumPy copy-in and immutable export also support uint64 and complex128. Complex
storage is two adjacent little-endian doubles, real then imaginary. Nullable
columns still need an explicit validity policy before NumPy export. Full-range
integer construction crosses the ABI as packed bytes; uint64 item responses are
JSON integers and Python decodes them exactly. Floating capability is queried with
`context.numeric_capabilities()` / `data.numeric.capabilities`; see the module's
numerical contracts for supported host environment, ordering and error behavior.

For separately distributed trusted source extensions and their C++/Python boundary,
see the [independent diagonal package](../packages/gr_diagonal/README.md).

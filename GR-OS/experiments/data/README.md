# Historical int64 data proof of concept

**Class:** Experimental implementation documentation; GR-OS v0.4.

This optional Layer 3 module demonstrates extension through public contracts. It
does not define GR ontology, a dataframe API, scientific validity, or a general
numerical library. Its synthetic observations are attributed examples.

`GR::experimental_data` registers `gr.experimental.data@1` explicitly into an
unsealed Catalog. The default kernel/runtime catalog excludes it. The C/Python
adapter can opt in with `Runtime(modules=["gr.experimental.data@1"])`; builds may
disable the module using `GR_BUILD_EXPERIMENTAL_DATA=OFF`.

`select(coordinator, workspace, record_id, payload_pointer, semantic_revision,
physical_revision)` pins the physical manifest selection, reconstructs the
selected semantic history snapshot, and extracts one explicitly named JSON array.
No column inference, missing-value coercion, or latest-revision substitution occurs.
The returned Selection retains an immutable column and provenance with workspace,
physical revision, semantic revision, exact source record version and JSON pointer.
Imported record metadata still obeys canonical JSON's safe-integer bounds.

The semantic column is an ordered sequence with no nulls, units or categorical
interpretation. Its physical carrier is one `values` buffer of signed two's
complement int64 elements, encoded little endian with alignment 8. At most 1,048,576
elements are accepted. Column and scalar types are separate versioned descriptors.

The transform contract performs checked multiplication then checked addition per
element. Each metadata parameter is an integer within +/-9007199254740991. Buffer
elements and results span int64; they do not pass through JSON numbers. Sum starts
at zero and checks every addition in original input order. Overflow fails the job;
there is no saturation, wrapping, floating tolerance, or reassociation.

`transform_sum` builds two independent transforms over immutable half-column views,
then concatenates the results in source order and performs one ordered reduction.
Runtime serial and parallel scheduling therefore execute the same semantic recipe.
The example does not claim that its small workload benefits from threading.

The native C++ helper returns Selection plus Graph; callers retain selection
provenance alongside the prepared plan and JobResult diagnostics. Python
`gr_os.experimental.analyze` invokes those same C++ helpers/providers and combines
the provenance, recipe/plan description, native diagnostics and owned outputs.
The helper's attribution identifies a technical derivation, not authenticated
authorship or independent observation. It does not publish or overwrite sources.

See [Python example](../../examples/python/historical_data.py),
[C++ integration example](../../tests/integration/experimental_data.cpp), and
[buffer contracts](../../docs/buffers.md). Close result handles explicitly; exported
readonly memoryviews independently retain the native allocation.

# Layer 2 module contracts

Class: experimental implementation documentation. Version: v0.7.
See [assessment](reviews/v0.7-assessment-and-plan.md) and
[ADR-0007](../decisions/ADR-0007-independent-layer2-packages.md).
Build/install commands and cross-language usage are in
[independent package guidance](module-packaging.md).

Layer 2 packages supply specialized reusable methods. A category describes a
purpose, not a mandatory base class. An analytical package can register Layer 3
operators and expose direct APIs; a visualization package can use a typed scene
without being a scheduler; a formal model can introduce its own representations.
Domain vocabularies remain extensions of general record contracts. Workflows and
connectors need explicit effect/authority protocols before deployment.

## Composition choices

| Boundary | Contract | Cost and limitation |
|---|---|---|
| Native registered computation | Install providers in one Library before seal; Expr/Function lower to Layer 4 | Matching native SDK/toolchain; trusted callbacks |
| Native direct analysis to visualization | Immutable source-retaining NetworkView and shared NetworkSnapshot | Retains owners; scene construction allocates geometry |
| Python package composition | Persistent native analysis owner; compact encoded snapshot passed explicitly | Copied transport and structural revalidation; no foreign engine handles |
| Python-only method | Ordinary package functions and optional metadata entry point | Python code is trusted; no automatic native operator registration |
| External service | Deferred separate process/RPC or authorized connector contract | Installation or discovery grants no external access |

Native package namespaces and distribution names are distinct from semantic
type/operator IDs, and both differ from wire-format versions. Snapshot and scene
formats are package contracts, not a universal interchange format. Other tools
may use typed arrays, their own mathematical objects or existing community
formats with explicit conversion contracts.

## Metadata and discovery

Format `gr.module-package/1` requires `id`, `version`, `categories`, `requires`,
`exports`, `effects`, `trust` and `compatibility`. IDs are bounded ASCII namespace
strings; versions are exact nonnegative `major.minor.patch` triples. Dependencies
have exact `id` and `version`, with optional CMake `components`. This deliberately
small declaration profile is not a version resolver. CMake/pip handle actual
installation and dependency resolution. A dependency inventory check establishes
declared version agreement only, not native ABI or component availability.

Categories and export-kind names are open descriptive labels. Known effect
declarations are `pure`, `read-only`, `history-write`, and `external`; they describe
the package and do not enforce behavior or grant permissions. Format-1 readers
reject unknown required extensions and effects. Unknown optional fields are
retained. JSON metadata is limited to 64 KiB and duplicate keys are rejected.
The bundled native factories additionally use the existing canonical-metadata
numeric subset: safe interoperable integers, with fractional values represented
by explicitly typed strings. Python-only optional metadata may use ordinary finite
JSON numbers; that broader acceptance does not establish native factory compatibility.

Python packages advertise a no-argument manifest factory under the
`gr_os.modules` entry-point group, using their manifest ID as the entry name.
[PyPA entry points](https://packaging.python.org/en/latest/specifications/entry-points/)
and Python's [importlib.metadata](https://docs.python.org/3/library/importlib.metadata.html)
provide the installed-distribution mechanism; no GR package manager is added.
The [standalone discovery example](../examples/python/module_discovery.py) lists
installed advertisements by default and can explicitly activate one named factory.

```python
from gr_os.modules import discover, read_manifest

entries = discover()             # metadata enumeration; no entry-point imports
for entry in entries:
    print(entry.id, entry.distribution, entry.version, entry.reference)

# Inspect an explicitly selected installed JSON manifest without importing it:
declaration = read_manifest("path/to/manifest.json")
declaration.check_dependencies({"gr-os": "0.7.0"})

# After selecting trusted package code explicitly:
selected = next(e for e in entries if e.id == "org.example.tool")
declaration = selected.activate(available={"gr-os": "0.7.0"})
```

Activation imports code **before** it can validate the returned declaration. It
checks manifest ID/version against the advertised distribution and can check an
explicit dependency inventory. It does not register providers or start a runtime.
Duplicate advertised IDs fail rather than choosing a winner by path order. The
import system and metadata providers already present in the process are trusted;
enumeration is not an inspection sandbox. Package factories in this increment
return metadata without loading native libraries; constructing the explicitly
selected engine is a separate action.

## Scientific meaning and custody

The analysis package preserves participant occurrences and qualification context.
Counts describe selected records, not the true prevalence of social relations.
Recorded continuity and target resolution are distinct from real-world identity.
Visualization is a configured projection. Categorical temporal bands do not
invent elapsed time, causation, or ordering where the source is indeterminate.

A native source-derived snapshot records its locally validated origin. An
imported portable snapshot validates structure and digest consistency only;
its source wrappers and full computational history are omitted. No wire flag
can claim source validation. NetworkView retains the selected qualified source and
direct archive; artifact bodies and the full history DAG require separate custody.
Source IDs and
snapshot IDs enable comparison, not independent proof of the omitted derivation.
Scene/SVG output carries these distinctions and declared losses.

Numerical modules describe their own exactness/approximation criteria. Diagonal
integer weighting rejects overflow and makes no empirical claim. Mapping a
selected degree vector to weights is a caller-specified modeling assumption.

## Authority and lifecycle

Native libraries execute with process privileges. Resource limits are bounded
logical inputs/outputs and cooperative work checks, not hard process memory or
CPU isolation. Cancellation cannot preempt arbitrary foreign code. Each package
owns its C handles and allocations; no raw C++ pointers or gr_os opaque handles
cross independently linked engines. Owners retain libraries for active calls.
Library unloading during outstanding work is unsupported.

The Python analysis engine uses the built-in record-profile registry. A native
caller can explicitly supply a relational Session with its own pinned profiles.
Portable preservation of an unknown profile is not validation of that profile's
meaning, and metadata discovery does not install new profile validators.

Analysis/visualization methods do not publish results or mutate history.
Authorized acquisition occurs through the existing caller-selected storage/runtime
interfaces. Export and publication remain explicit caller operations. The
transaction, frozen-retry and recovery contracts of Layers 4-6 remain unchanged.

## Next decisions

Qualify additional platforms and API stability before promising binary releases.
Choose representative real workloads before adding streaming, device execution,
incremental network indexes or large-scene renderers. Decide whether Python needs
external provider registration in one native Context, or process-isolated services,
using a concrete composition need. Design connector authorization and durable
external outcomes separately. None requires a second semantic kernel.

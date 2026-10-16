# ADR-0007: Independent Layer 2 packages and qualified projections

Date: 2026-10-16. Status: accepted for the experimental v0.7 increment.
Publication class: software architecture decision, not a Standard.

## Issue and alternatives

Independent reusable tools need registration, packaging, composition and explicit
semantic/authority boundaries. A universal plugin host, a second runtime,
Python-only modules, and mandatory serialized native calls were considered.
Existing Layer 3/4 facilities already cover typed registration and scheduling;
the missing evidence concerns independent packages and practical composition.

## Decision

Use conventional CMake/Python packages with versioned descriptive manifests and
explicit activation. Categories are descriptive. Reuse Library registration and
prepared runtime execution where useful; permit direct typed calls. Discovery
does not execute native code or grant authority. No custom package manager,
universal plugin ABI, or in-process sandbox is introduced.

Implement qualified incidence analysis and deterministic scene/SVG visualization
as independent packages. Add an immutable owner-retaining Layer 3 inspection
view and expose the existing native value descriptor in Python for exact import.
Native analysis retains the selected qualified source and direct archive, and
shares a typed snapshot. Artifact bodies and the full history DAG stay separate.
Python packages compose through explicit compact snapshot copies and persistent
module-owned handles. Imported snapshot validation is structural only and
cannot be upgraded by wire-supplied claims. Rendering returns data; historical
publication and external actions remain separate authorized operations.

Use exact SDK dependencies for the new native builds. Distribution versions,
semantic IDs and wire versions are separate. C++ ABI across toolchains/build
configurations is not promised. Existing gr_diagonal supplies the contrasting
mathematical extension and gets an explicit version update for the new SDK.

## Rationale and consequences

This minimizes new concepts while demonstrating native/Python package independence.
The Python boundary copies compact data and cannot splice arbitrary package
providers into gr_os.Context; this limitation is explicit. Untrusted code needs
future process isolation. Source retention costs memory; dense scenes have finite
budgets. No kernel/history/transaction semantics or institutional authority changes.

Affected contracts: relational inspection, independent package manifests and
SDK version, analysis snapshot/Scene formats, Python facades, installed-consumer
tests and documentation. Existing v0.6 artifacts and evidence are preserved.
The assessment and acceptance plan are in
[v0.7 assessment](../docs/reviews/v0.7-assessment-and-plan.md).

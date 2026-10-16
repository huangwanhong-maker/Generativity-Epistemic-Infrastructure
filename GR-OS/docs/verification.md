# GR-OS v0.7 verification record

**Class:** Implementation test and performance evidence

**Date:** 2026-10-17 JST

**Scope:** Independent Layer 2 packages and Layers 3-6 regression

**Status:** Complete; stopped for architectural review

## Environment and baseline

Windows x64, MSVC 19.44 / Visual Studio 2022, CMake 3.31.6, Python 3.12.10,
NumPy 2.5.0, Intel Core i7-11370H and eight reported logical processors.
Wheels use existing build-local setuptools/wheel tools offline. No global Python
installation is changed. Other operating systems, compilers and devices are not
qualified by this run.

Published v0.6 commit `bcbb477479d4f5e5988a7cd078a47bd76eb3ac57` is the baseline.
Before edits, all 172 source entries matched and the unchanged Release suite passed
**38/38 in 179.96 s**. Its source archive, installed SDK, Python facade, runners and
six benchmark executables were preserved as 94 hashed files. See the
[baseline manifest](validation/v0.6-pre-v0.7/manifest.json),
[baseline test](validation/v0.6-pre-v0.7/ctest-baseline.txt), and
[original v0.6 verification](validation/v0.6-pre-v0.7/verification-v0.6-original.md).
All seven vendored files match pinned hashes and lengths in the
[dependency recheck](validation/dependencies-v0.7.json).

## Release acceptance

The full suite passed **40/40 in 382.68 s**:
[configure](validation/configure-v0.7-release-final.txt),
[build](validation/build-v0.7-release-final.txt),
[summary](validation/ctest-v0.7-release-final.txt),
[details](validation/ctest-v0.7-release-final-detail.txt), and
[commands](validation/commands-v0.7-release-final.json).

| Area | Accepted coverage |
|---|---|
| Existing computation | 83 language, 89 numeric, 42 standard, 108 sparse, 58 access and 69 qualified-history checks; owning views and provenance guards added without removing prior behavior |
| Analysis package | 57 checks: independent graph oracles, repeated/n-ary/isolated participation, identities, changed recordings, malformed/oversized interchange, custody/lifetimes and cancellation |
| Visualization package | 123 checks: source/glyph consistency, layouts, filtering/losses, XML escaping, deterministic identities, limits/cancellation, narrow labels and dense fallback |
| Formal contrast | Independent diagonal suite, including 32 oracle cases; explicit degree-vector mapping and checked weighting/reduction in the composed consumer |
| Discovery | Six Python cases: metadata-only discovery, explicit activation, dependency/version errors, malformed metadata, duplicate IDs and ownership |
| Installed composition | C++ Function/Runtime and direct APIs, shared native topology, portable validation-basis separation; eight Python integration cases and a working example in the full run |
| Existing foundation | Kernel, Memory/SQLite history, execution, planner, scheduling, publication/recovery, process termination, buffers, CLI, installed C/C++ SDK and prior Python suites retained |

Actual NumPy passed in Release. Packages build from copied independent source
projects against relocated installed dependencies; the core build does not add
those projects as source subdirectories. Release module products are under
`build/m/9b198579`. Python checks cover source/summary/portable scene parity,
provenance changes, malformed snapshots, close/race behavior and handle capacity.

Two narrowly scoped changes followed the full run. First, package CMake projects
gained explicit manifest configuration dependencies. All three reconfigured and
manifest touches triggered regeneration without a native object rebuild:
[regeneration evidence](validation/manifest-regeneration-v0.7-release-final.json).
Second, Python analysis import now closes a rejected exported buffer immediately.
A ninth installed case retains the rejection traceback and checks native closure.
The changed wheel passed all nine installed tests in 0.343 s plus the example:
[Release correction](validation/python-cleanup-v0.7-release-final.txt).
These changes are not attributed to the earlier full run.

## Packaging, compatibility and retained failures

Native compatibility means exact SDK version plus matching compiler/runtime,
configuration and sanitizer settings. It is a tested source/API build boundary,
not a cross-version C++ ABI promise. Wheels contain facades; native libraries are
installed and explicitly selected separately. Discovery does not load them.
[Exact SDK/package commands](validation/v0.7-sdk-commands.md) link archived
commands, hashes, conformance output and source-copy checks for both configurations.

Missing SDK and unsupported declared versions fail in isolated CMake probes.
The actual preserved GR 0.6.0 SDK was also supplied to the analysis package and
rejected for its GR 0.7.0 requirement:
[diagnostic](validation/older-sdk-v0.7-rejection.txt) and
[command/result](validation/older-sdk-v0.7-rejection.json).
An incompatible declaration fixture and an actual historical SDK probe are
separate evidence; neither constitutes a historical source rebuild.

Initial failures remain available. A negative probe first hit Windows path-length
compiler detection, corrected with shorter build paths. Another attempt used an
invalid empty Relation as an isolated-owner fixture; it now uses a valid generic
Entity with an empty participants array. A later diagnostic assertion mishandled
CMake line wrapping, corrected through whitespace normalization. Fresh package
acceptance covers the intended checks. See
[initial](validation/modules-v0.7-release-initial.txt),
[second](validation/modules-v0.7-release-second.txt),
[third](validation/modules-v0.7-release-third.txt), and
[continued consumer](validation/modules-v0.7-release-third-consumer.txt) logs.

## AddressSanitizer

The final selected MSVC AddressSanitizer run passed **39/39 in 832.22 s**,
including independent native packages, installed consumers, actual NumPy, and the
nine-case Python module suite with the final rejection-cleanup regression:
[configure](validation/configure-v0.7-asan-final.txt),
[build](validation/build-v0.7-asan-final.txt),
[summary](validation/ctest-v0.7-asan-final.txt),
[details](validation/ctest-v0.7-asan-final-detail.txt), and
[commands](validation/commands-v0.7-asan-final.json).

Only unchanged legacy `python_binding` was excluded: its 10,000-record capacity
fixture has extreme instrumented allocation overhead and passed in full Release.
No optional NumPy case was skipped in this selection. Native C/C++ and external
consumers were instrumented; Python and NumPy themselves were not sanitizer
builds. Outer watchdogs use multiplier eight without changing operation deadlines.
No sanitizer finding was reported. TSan, other platforms, power-loss behavior and
arbitrary devices are not qualified by these tests.

## Visual inspection and measurements

Final SVGs were rendered and inspected with Windows Chrome 155.0.8059.40:
[inspection record](validation/v0.7-visual/inspection.json),
[incidence columns](validation/v0.7-visual/incidence-columns.png),
[temporal bands](validation/v0.7-visual/temporal-bands.png), and
[narrow canvas](validation/v0.7-visual/narrow.png).
An initial small-layout label overlap was corrected with deterministic bounded
spacing. Dense scenes retain an explicit compressed-spacing fallback. Narrow
long text is intentionally clipped; full text and mappings remain in scene/SVG
metadata. This is visual smoke coverage, not cross-browser/font/accessibility
qualification. The browser used isolated local profiles and local SVG files.

The first visualization benchmark failed because its 1,024-owner fixture exceeded
the existing publication update cap. The fixture now uses bounded batches and
reserves the history-manifest update. Production limits and libraries are unchanged.
A new large-fixture regression brought visualization coverage to **127 checks**;
targeted CTest passed **1/1 in 4.58 s Release** and **1/1 in 180.52 s ASan**.
Both examples passed and small-fixture scene/SVG bytes remain unchanged.
The [supplement](validation/visualization-fixture-v0.7-supplement.txt) and
[package command appendix](validation/v0.7-sdk-commands.md) preserve original
sources/artifacts, final hash guards and copied-source equality. Full-suite results
above precede this fixture-only correction; the targeted runs verify the final files.

[Performance analysis](validation/benchmark-v0.7-analysis.md) records serial
matched v0.6/v0.7 workloads, independent module phases and a focused Python transport
profile. The reverse-order large serial table result remains about 20.7 ms, but
scheduled/transport regressions and substantial timing variation are retained.
Portable rendering and large JSON imports remain significant costs. Timings do not
replace conformance or establish a production-scale guarantee.

[Source/artifact manifest](validation/v0.7-source-artifacts.json),
[evidence manifest](validation/v0.7-evidence-manifest.json), and
[baseline recheck](validation/v0.7-baseline-reverification.json) identify the final
implementation, accepted builds/wheels, captures and preserved baseline. Living
review/README documents are outside frozen implementation hashes. An independent
rehash verified all 212 source, 355 artifact and 146 evidence entries; all 94
preserved baseline files still match. The 18 current documents checked here contain
211 resolving local file links. Both accepted
package runs match all 51 package source files and four composition-consumer files.
The core native libraries, analysis/visualization package libraries and small SVG
fixtures are unchanged by the final Python/benchmark-fixture corrections.

See the [architectural review](reviews/v0.7-review.md), [module contracts](modules.md),
[packaging guide](module-packaging.md), and [next decisions](planning/open_questions.md).
Native sharing, Python copies, selected source custody, omitted histories/artifacts,
and factual/institutional authority have different contracts. Installation does
not sandbox trusted native code or authorize external effects. Work remains inside
GR-OS; no Layer 1 application or legacy migration is included.

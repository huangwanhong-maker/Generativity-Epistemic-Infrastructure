# GR-OS review evidence

**Class:** Informative source and verification record  
**Status:** Observed local evidence with explicitly bounded scope  
**Date:** 2026-10-13 JST

This record supports the [architectural assessment](ARCHITECTURE_REVIEW.md). It distinguishes current local source, historical handoff evidence, executed tests, targeted probes, and engineering proposals. No source changes, dependency installation, retained-data migration, or Git configuration repair were performed.

## Source identity

The user-named [CONTRIBUTORS.md](../gr_infrastructure/CONTRIBUTORS.md) is an attribution document. The substantive handoff is [GR_Infrastructure_Architecture_R8_Codex_Handoff_2026-10-13.zip](../gr_infrastructure/GR_Infrastructure_Architecture_R8_Codex_Handoff_2026-10-13.zip), with internal root `GR_R8_Architecture_Codex_Handoff_2026-10-13/`.

Archive SHA-256:

```text
f9ec247a9f3b91c38a57855282576dc2b248b0f0f066931babf32b1e8de76e09
```

The archive contains 249 entries, including its bundle manifest. All 248 entries listed in `BUNDLE_MANIFEST.json` matched their recorded byte lengths and SHA-256 digests. All 185 curated files under `evidence/source_reference/` matched current local files byte for byte after mapping the historical prefix `applicative_infrastructure/` to `gr_infrastructure/`. None of those 185 files was missing or different. This does not establish that the curated set contains every implementation file or that remote branches match the local checkout.

| Repository | Observed local HEAD |
|---|---|
| Programme | `af4d97f4e1a28464766cb1c7f8769a2d81d94b0c` |
| Infrastructure | `80aa7996f641a8ccbb4d1e55f0712356714bd026` |
| Generalized application | `a3f39719578f9a87ea4e6a4dc8180e585bee36ac` |
| Academia application | `4e577f2b3f38a3b3c3dcd009804b06182ea4b70b` |

The application heads match the infrastructure's committed gitlinks. Both application tracked worktrees were clean before and after the review. Infrastructure status, with submodule recursion disabled, showed only the existing untracked handoff ZIP. The parent already had modified README/CHANGELOG, the removed old submodule path, and unrelated untracked material. The review added only `GR-OS/` to that parent status.

The physical directory rename has not been reconciled with parent `.gitmodules` and nested Git worktree metadata. Ordinary Git commands can fail trying to enter `applicative_infrastructure`. Read-only inspection used explicit absolute worktree overrides, for example:

```powershell
$grReviewWorktree = (Resolve-Path gr_infrastructure).Path
git --work-tree=$grReviewWorktree -C gr_infrastructure status --short --ignore-submodules=all
```

No remote fetch or claim about current upstream branch tips is part of this review.

## Reading and inspection scope

The review covered the handoff's README and AGENTS, R8 navigation and documents 25–32, R6/R7 documents 18–24, earlier architecture documents 01–17 and navigation, decision register/open questions, Codex handoff/checklist, source audit/selection, changelog, diagram, and the historical R1 design, migration, language, storage, security and component-disposition reasoning. The archive's included source references were compared with live files; actual implementation and tests were inspected directly in the available repositories.

| Area | Principal sources examined |
|---|---|
| Programme scope and authority | `PROGRAMME_BRIEF.md`, root `AGENTS.md` and README, publication taxonomy, publication status, ADR-0002 |
| Existing architecture decisions | ADR-0007 Git workspace, ADR-0008 graph/transactions/modules, ADR-0009 consolidation, ADR-0010 repository/licensing, ADR-0011 event partial orders |
| Technical contracts | GR-SPEC-120, GR-SPEC-130, GR-SPEC-121, GR-120/130/140/160 working drafts; selected GR-100/110/200/210 context and epistemic clauses; SPEC-200 status |
| Generalized implementation | Common protocol core/schema/temporal code; Git store; Flask composition and protocol binding; graph/time interface source; package and application tests |
| Academia implementation | Native canonicalization, keys/identity, actions/CLI, store, checks, views, export/bundle/disclosure; TypeScript canonicalization; server accounts/runtime/records/projects/authority paths; client and tests |
| Existing design and recovery | Common migration verification/audit, academia plans/notes and relocation records, package guides and conformance boundaries |
| Foundation support | Manifest and targeted actual-PDF passages from Metamodel I/II and GRE Chapters 4/5; selected native academia Paper IV passages |

This is a comprehensive architectural assessment with targeted source verification, not an exhaustive security audit, proof of correctness, line-by-line review of every file, or full collation of the foundation corpus. Browsers, production deployment, all publication build tooling, performance benchmarks, and real retained user data were not exercised.

## Standards and foundation anchors

The current [programme README](../README.md) explicitly states there are no adopted technical Standards. [Publications](../publications/README.md) remain preparatory working drafts, with adoption and work-item approval pending. GR-120/130/140/160 are preparatory WD 0.1; GR-SPEC-120/130 are experimental 0.2; GR-SPEC-121 is experimental 0.1. [GR-SPEC-200](../specifications/GR-SPEC-200-epistemic-recording/README.md) is a preliminary placeholder, not an implemented shared epistemic contract.

| Architectural question | Source anchor |
|---|---|
| Identity categories and reference scope | GR-SPEC-120 S120-001; GR-120 M120-001; GR-200 E200-009 |
| First-class, potentially n-ary relations | GR-120 M120-003–006; ADR-0008 |
| Target change, account revision, descriptive change | GR-200 E200-017/018; GR-120 M120-010 |
| Occurrence, observation, recording and interpretation time | GR-200 E200-008; GR-SPEC-121 T121-001/015 |
| Verification, evidence and authority distinctions | Programme Brief 2.0/3A; GR-200 E200-002/010/011/016 |
| Semantic distinction without separate software classes | GR-200 E200-027 |
| Atomic acceptance, stale base and durable retry | GR-130 I130-003–006/012; GR-SPEC-130 X130-014–019 |
| Current authorization for retained history | GR-160 A160-005/006; GR-130 I130-009 |
| Detachment, erasure, complete export and custody | GR-160 A160-004/009/010 |
| Unknown optional and mandatory semantics | GR-SPEC-120 S120-014/028; GR-SPEC-130 X130-007/043 |
| Competing temporal accounts and cycles | GR-SPEC-121 T121-011/012; parent open question Q-45 |

Foundation passages were checked against actual PDFs, using one-based PDF page numbers:

- [Metamodel II](<../foundation_manuscripts/A Metamodel for Ontologically Heterogeneous Social Dynamics-II.pdf>), p.2: selected overlapping categories; pp.12–15, sections 3.4–3.10: identities/inverses allowed, formal equivalence versus normative protection, histories without collapse; pp.78–80, section 17: declared regime changes and limits of recovering lost distinctions.
- [Metamodel I](<../foundation_manuscripts/A METAMODEL FOR ONTOLOGICALLY HETEROGENEOUS SOCIAL DYNAMICS-I.pdf>), sections 3–4 and 7: fixed action/congruence assumptions, conditional collapse results, limitations of a fixed carrier. Later qualifications are preserved rather than silently harmonized.
- [GRE Chapter 4](../foundation_manuscripts/GRE_Chapter_04.pdf), section 4.9, p.65: general manifestation relation rather than a mandatory deterministic function.
- [GRE Chapter 5](../foundation_manuscripts/GRE_Chapter_05_Complete_First_Draft.pdf), section 5.14, pp.114–115: relational continuity versus generative influence; section 5.20, pp.168–171: reasoned revision and preservation of differences.
- Academia native Paper IV, targeted PDF pp.8, 20–21: substrate capability versus named technology; performer/registrar separation and credential independence; Claim 8.4 on p.21 explicitly limits attestation claims about truth or improvement. This was supporting local domain-source inspection, not a verified external publication-status claim.

## Generalized implementation anchors

Paths below start at `gr_infrastructure/`.

| Finding | Source and line anchors |
|---|---|
| Fixed profile, six roles, built-in module handlers and bounds | `common/packages/gsp_record_protocol/src/gsp_protocol/core.py:17`, `:24`, `:62` |
| Repeated complete-snapshot adaptation and validation | `core.py:153`, `:236`, `:277`, `:443`, `:559` |
| N-ary incidence and independent relation records | `core.py:197`, `:283` |
| Opaque optional preservation and mandatory-module blocking | `core.py:197`, `:446`, `:504` |
| Complete candidate supports new forward references | `core.py:477`, `:559` |
| Receipt summarizes changes and is not full replay input | `core.py:560` |
| All non-withdrawn order assertions enter one model | `common/packages/gsp_record_protocol/src/gsp_protocol/temporal.py:81`, `:91` |
| Cycles or unsupported order versions block a cut | `temporal.py:198`, `:218` |
| Git-shaped revision constraints | Protocol schema `:513`, `:1022`; `gr_generalized_application/web_application/generative_app/protocol_api.py:47` |
| Bounded batch reads and conditional publication | `common/packages/gsp_git_store/src/gsp_git_store/store.py:303`, `:345` |
| Receipt and retained material published together | `store.py:500`, `:619` |
| Receipt lookup uses historical introduction | `store.py:635` |
| History cap and no pagination in this interface | `store.py:676` |
| SQLite project ownership plus separate Git creation | `gr_generalized_application/web_application/generative_app/__init__.py:343` |
| Whole snapshot loading during project listing | `generative_app/__init__.py:334` |
| Actor/digest replay precedes stale-base rejection | `generative_app/protocol_api.py:59` |
| Revision-pinned graph and historical access | `generative_app/static/graph.js:70`; `generative_app/protocol_api.py:244` |

## Academia findings and probe scope

Paths below start at `gr_infrastructure/gr_academia_application/`.

| Finding | Source and line anchors |
|---|---|
| Web authorizes session but omits native actor binding | `web_application/packages/server/src/app.ts:328`, `:402`; `records.ts:50`, `:95` |
| Record-local identity generated and defaulted to self | `domain_packages/grrp/src/grrp/actions.py:56`; `store.py:200` |
| Numeric canonicalization mismatch | Native `canonical.py:66`; TypeScript `web_application/packages/protocol/src/canonical.ts:92` |
| Signature coverage differs from complete record | Native `canonical.py:40`, `:120`, `:129` |
| Different answered-objection projections | Native `views.py:79`; web `records.ts:269` |
| Hosted project visibility does not implement full native disclosure | Web `app.ts:259`; `records.ts:521`; `plans/spec-graph-model.md:355`, `:382` |
| Filesystem private-key custody | Native `keys.py:28`; `identity.py:103`; compare older claims in web `db.ts:31` and client `SignIn.tsx:3` |
| Multiple direct file writes, optional Git commit | Native `store.py:91`, `:404`; `gitutil.py:35` |
| Broad reindex after writes | Web `app.ts:207`, `:233`, `:408`, `:464`; `projects.ts:48` |
| File hashing and repeated graph reads | Web `records.ts:428`, `:734` |
| Profile-specific policy, not universal kernel mechanics | `plans/implementation-plan.md:41`; `plans/spec-graph-model.md:334` |

The actor probe used current server/protocol TypeScript through Vite SSR loading, process-local import aliasing, Fastify injected requests, an in-memory account database, and a fresh temporary native record directory. It created two synthetic accounts with separate party identities, opened a project/question, made the project listed, and submitted a claim as the second account. It did not exercise group-tier or delegated registration routes. No public server listener or retained runtime was used.

Observed output:

```json
{"actStatus":201,"performerMatchesSubmittingAccount":false,"performerMatchesProjectAccount":false,"performerMatchesOpeningQuestion":true}
```

The existing authority suite tests that another account may submit, but does not assert this native attribution. This probe establishes an actor-mapping gap for the tested path; it is not a claim that no native GRRP multi-identity operation exists.

The numeric probe used current Python source and the already-built TypeScript `dist/canonical.js`. The corresponding current TypeScript source was inspected and uses the same finite-number/`JSON.stringify` behavior. No new TypeScript build was performed.

| Input | Python canonical text | TypeScript canonical text |
|---|---|---|
| `1.0` | `{"n":1.0}` | `{"n":1}` |
| `1e-7` | `{"n":1e-07}` | `{"n":1e-7}` |
| `9007199254740993` | `{"n":9007199254740993}` | `{"n":9007199254740992}` |

This demonstrates disagreement on admitted inputs, not an exhaustive canonicalization audit. The finding is scoped to native GRRP's `json-sorted/1`; it is not evidence that the generalized RFC 8785 implementation has the same behavior.

## Executed suites

| Suite | Observed result | Qualification |
|---|---|---|
| Common protocol, Git store, generalized application | 178 passed, 7 skipped, 156.46 seconds | All skips were opt-in browser tests |
| Academia native Python | 208 passed, 1 skipped, 72.24 seconds | Existing interpreter, isolated temporary output |
| Academia TypeScript protocol | 43 passed, 839 ms | Existing dependencies, cache disabled |
| Academia server default invocation | 8 passed; authority suite failed collection | Existing `@gra/protocol` junction targeted the old directory |
| Academia server with process-local alias | 48 passed, 67.30 seconds | Current protocol source aliased in memory; no dependency repair |

The completed suites total 477 passes and 8 skips. The initial eight server passes are included in the final 48 and are not counted twice. Tests establish bounded behavior in the reviewed environment, not GSP/GRRP equivalence, production security, standards adoption, browser correctness, or improved performance.

## Reproduction commands

All commands below use PowerShell and existing dependencies. Temporary output directories are disposable test directories, never retained runtime paths. Source paths are supplied explicitly because relocated editable environments or npm junctions can otherwise point to the former directory. These snippets record checks; they are not new GR-OS implementation files.

From the programme root, the generalized invocation used the existing interpreter at `gr_infrastructure/.runtime/environments/generalized/Scripts/python.exe`, with `PYTHONDONTWRITEBYTECODE=1` and `PYTHONPATH` containing the absolute protocol `src`, store `src`, and generalized `web_application` paths:

```powershell
& ./gr_infrastructure/.runtime/environments/generalized/Scripts/python.exe -B -m pytest -p no:cacheprovider -c gr_infrastructure/gr_generalized_application/web_application/pytest.ini gr_infrastructure/common/packages/gsp_record_protocol/tests gr_infrastructure/common/packages/gsp_git_store/tests gr_infrastructure/gr_generalized_application/web_application/tests --basetemp C:\Users\Micro\AppData\Local\Temp\groa581c33 -q
```

For a later rerun, use a fresh temporary directory rather than assuming this recorded path is disposable. `GSP_BROWSER` was unset.

From the academia application root:

```powershell
$env:PYTHONDONTWRITEBYTECODE = '1'
$env:PYTHONPATH = (Resolve-Path 'domain_packages/grrp/src').Path
$grAuditTemp = Join-Path ([System.IO.Path]::GetTempPath()) ('grrp-audit-' + [guid]::NewGuid().ToString('N'))
& '../.runtime/environments/academia/Scripts/python.exe' -m pytest domain_packages/grrp/tests -p no:cacheprovider --basetemp $grAuditTemp -q
```

The native temporary directory used the shown prefix and a generated UUID; its exact suffix was not printed. No removal of that test output was attempted.

From academia `web_application/packages/protocol`:

```powershell
node ../../node_modules/vitest/vitest.mjs run --no-cache
```

The initial server invocation, from `web_application/packages/server`, failed collection of the authority suite:

```powershell
$env:PYTHONDONTWRITEBYTECODE = '1'
$env:PYTHONPATH = (Resolve-Path '../../../domain_packages/grrp/src').Path
$env:GRA_PYTHON = (Resolve-Path '../../../../.runtime/environments/academia/Scripts/python.exe').Path
node ../../node_modules/vitest/vitest.mjs run --no-cache --configLoader runner
```

It reported `Cannot find package '@gra/protocol'`. The installed junction targeted `generativity_standards_program/applicative_infrastructure/gr_academia_application/web_application/packages/protocol`, which no longer exists under that name.

The passing server rerun used an in-memory alias from the academia root:

```powershell
$env:PYTHONDONTWRITEBYTECODE = '1'
$env:PYTHONPATH = (Resolve-Path 'domain_packages/grrp/src').Path
$env:GRA_PYTHON = (Resolve-Path '../.runtime/environments/academia/Scripts/python.exe').Path
@'
import {startVitest} from './web_application/node_modules/vitest/dist/node.js';
import {resolve} from 'node:path';
const ctx = await startVitest('test', [], {root:resolve('web_application/packages/server'),config:false,watch:false,run:true,cache:false,fileParallelism:false,testTimeout:60000,hookTimeout:60000},{resolve:{alias:{'@gra/protocol':resolve('web_application/packages/protocol/src/index.ts')}}});
await ctx.close();
'@ | node --input-type=module
```

The serial-file and timeout settings reproduce the checked-in server configuration. The authority tests create their own temporary runtimes and clean them after each case.

The numeric probe, from the academia root, was:

```powershell
$env:PYTHONDONTWRITEBYTECODE = '1'
$env:PYTHONPATH = (Resolve-Path 'domain_packages/grrp/src').Path
@'
from grrp.canonical import canonical_bytes
for value in (1.0, 1e-7, 9007199254740993):
    print(canonical_bytes(dict(n=value)))
'@ | & '../.runtime/environments/academia/Scripts/python.exe' -
@'
import {canonicalText} from './web_application/packages/protocol/dist/canonical.js';
for(const n of [1.0, 1e-7, 9007199254740993]) console.log(canonicalText({n}));
'@ | node --input-type=module
```

## Boundaries of the recommendation

Replacing Git with SQL and artifacts trades existing Git history/export mechanisms for explicit staging, receipt, garbage-collection, and restore responsibilities. A pure semantic library cannot enforce hosted authority by itself. A profile mechanism must retain mandatory queryable meaning rather than turning important qualifiers into uninterpreted optional fields. A native language introduces packaging and binding costs and remains a candidate, not a settled conclusion.

Two adversarial histories should be used in subsequent design work:

1. At R1, one source claims A precedes B. At R2, a retrospective source claims B precedes A. At R3, evidence changes an interpretation. Preserve the earlier recorded account, both selected chronologies, the contradictory combined view, and reasons for exclusions; never infer a unique recovered reality.
2. A write stages an artifact, commits its reference and receipt, and loses its response. Another writer advances the history. The first client retries while an analytical job is running and its access is revoked. Recover the original receipt without a duplicate revision; retain the later write; enforce declared revocation/result disclosure; preserve accepted artifact closure through cleanup and restore.

## External primary references

These references were checked for bounded technical claims, not as evidence that a proposed GR design has been implemented or benchmarked:

- SQLite WAL documentation supports the same-host, single-writer and reader-concurrency qualifications used in the storage proposal: [Write-Ahead Logging](https://www.sqlite.org/wal.html).
- Apache Arrow documents the stable C data interchange structures and lifetime responsibilities relevant to a possible analytical boundary: [C data interface](https://arrow.apache.org/docs/format/CDataInterface.html).
- RFC 8785 specifies a particular canonical JSON profile, including numeric serialization; it is distinct from native GRRP's sorted-JSON contract: [JSON Canonicalization Scheme](https://www.rfc-editor.org/rfc/rfc8785.html).

The preferred architecture, migration stance, language discussion, and proposed acceptance sequence are engineering judgments derived from this evidence. No performance improvement, comprehensive interoperability, or production-readiness claim is made.

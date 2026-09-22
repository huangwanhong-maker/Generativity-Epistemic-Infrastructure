# Academia application reference review

**Document class:** Informative implementation comparison  
**Status:** Source inspection; proposals are provisional  
**Reviewed:** 2026-09-22  
**Reference repository:** `research/projects/Generative-Relational-Academia-Application`

This review supports the generalized application's graph, recording protocol and record-module design. It compares the academia project's source with its own design documents. It does not establish conformance to the Generativity Standards Program or run the academia application's test suite. No files in that repository were modified and no software source was copied.

> Historical scope: the review below describes the pre-import inspection. ADR-0009 subsequently authorized consolidation; the application is now retained under `applicative_infrastructure/gr_academia_application/`. Links below resolve to the imported source. Exact original source is recoverable from its verified history bundle; see the [migration guide](../../applicative_infrastructure/README.md). Earlier statements about work not yet copied or tested describe that original review, not the completed relocation.

## 1. Reuse the architectural distinction, preserve the different models

The academia application has a useful separation between protocol, host and presentation. Its TypeScript protocol package implements canonical encoding, content identifiers and signatures; its Fastify host stores accounts and a derived SQLite index; its React client presents records. The host delegates substantive record writes to the Python `grrp` command. Plain record files remain available without the host database.

Its semantic kernel is narrower than the programme's six operational record classes. GRRP records changes in understanding as eight acts: `question`, `claim`, `challenge`, `transformation`, `decision`, `connection`, `verification` and `release`. A question starts a trajectory inside a project. Projects are containers; they are not trajectories. A state is content reached through a transition rather than a separate selectable graph subject.

The [graph-model draft](../../applicative_infrastructure/gr_academia_application/plans/spec-graph-model.md) explicitly refuses person and project nodes and treats occasions as readings of shared cited materials. These are substantive academia-profile choices. They cannot serve as universal restrictions on Entity, State, Event, Process, Relation and Property records in the generalized application.

A shared future kernel can provide stable references, record revisions, explicit relations, attachment digests, protocol versions, transaction integrity and portable storage. Academia can supply its own acts, trajectories, views and policy constraints through a separately declared profile. Mapping GRRP states or transitions directly to generalized State or Event records would require a recorded semantic argument.

## 2. Actual stored envelope and history

The Python [store implementation](../../applicative_infrastructure/gr_academia_application/domain_packages/grrp/src/grrp/store.py) constructs transition records with:

| Group | Fields and treatment |
|---|---|
| Identity and structure | `id`, `protocol`, `kind`, `trajectory`, `parents`, `prior_state`, `posterior_state` |
| Factored description | `act`, `target`, `relation`, `trigger`, `disposition` |
| Attribution and grounds | `performer`, `performed`, `contributions`, `absorption`, `artefacts` |
| Registration | `registrar`, registration `time`, `attested`, optional `signature` inside `registration` |
| Administrative operations | `kind: operation`, `operation`, `subject`, `payload`; the common envelope supports these separately from substantive transitions |

The three dispositions are `accepted`, `contested` and `unresolved`. Relations include sixteen CiTO bindings and three explicitly local terms. Contributor-role bindings use CRediT. The source preserves the distinction between performing an act and a later party registering it. A distinct registrar's signature is not represented as approval of the substantive content.

State identifiers are SHA-256 digests of stored content bytes. Content is normalized once before initial writing; later readers verify the actual bytes. Transition identifiers cover an explicit, versioned field list, including semantic parents. Registration and disclosure are excluded from that identifier. Registrar signatures cover `{id, registrar, time}`. A cross-language test-vector suite checks Python/TypeScript agreement; it previously exposed different whitespace handling.

Transitions live in append-only `trajectories/<id>/transitions/<hash>.yaml`; state content lives separately in `states/<hash>.md`. Disclosure is a separate sidecar, so its lawful change does not alter the transition identifier. Group-tier proposals live outside the registered-transition directory until registration. Corrections are later transitions. The store topologically orders parent records before descendants; timestamps break ties rather than determine the semantic graph.

The [Git adapter](../../applicative_infrastructure/gr_academia_application/domain_packages/grrp/src/grrp/gitutil.py) stages and commits exactly the paths an operation supplies. It returns a success flag; record files can exist without a successful Git commit. Git history is therefore a substrate, not a one-to-one definition of a semantic transition. This is a useful conceptual distinction, but a generalized application promising Git-backed atomic writes needs stronger failure, locking, rollback and concurrency behavior than this optional adapter supplies.

## 3. Graph and module patterns worth adapting

The [graph canvas](../../applicative_infrastructure/gr_academia_application/web_application/packages/client/src/pages/Trajectories.tsx) already supports selection, neighbor highlighting, dragging, panning, zooming, fit and local layout persistence. Connection transitions with multiple parents appear as selectable capsules on connecting lines. The same connection is thus an edge-like presentation and an inspectable record with its own content.

The [subject panel](../../applicative_infrastructure/gr_academia_application/web_application/packages/client/src/pages/NodePanel.tsx) separates Record, Disclosure, Workspace and conditional Calendar tabs. Selecting a graph subject opens relevant components and actions in one place. This is a useful interaction pattern for generalized records: selecting a Relation should expose the same provenance, revision and attachment capabilities as selecting another record class.

The [host projection](../../applicative_infrastructure/gr_academia_application/web_application/packages/server/src/records.ts) builds graph edges from semantic parents and artifact citations. It distinguishes `follows`, cross-trajectory `crosses`, and `cites`. Repeated references to an artifact become one material node. Structural links and substantive connection records have different authority and affordances; a displayed line need not itself be a record.

The graph-model draft classifies modules by storage and authority:

| Module | Intended boundary | Generalized design implication |
|---|---|---|
| Record | Protocol-authored semantic content | Persist substantive changes through the recording transaction path. |
| Disclosure | Protocol operation and sidecar history | Keep permissions and disclosure changes explicit; do not infer them from canvas visibility. |
| Workspace | Mutable project-shared working material | Distinguish a convenient file collection from a cited, immutable evidence snapshot. |
| Calendar/layout | Local presentation or planning state | Moving a node or changing a local view need not assert a change in the represented situation. |

A workspace upload does not itself become evidence in GRRP. A substantive act deliberately cites a digest. The generalized application can adapt this distinction while offering a clearer attachment workflow: immutable bytes and manifest metadata when attached to a record, with a separately named scratch workspace if later needed.

## 4. Observed implementation gaps and migration traps

These findings are based on the inspected source, not executed exploit or integration tests.

1. **Disclosure is a placeholder in the node panel.** Its own text says the sidecar is not wired. An attractive tab is not evidence of working access or disclosure controls.
2. **Calendar storage differs from the design.** The draft proposes `.gra/calendar/`; the client stores entries in browser `localStorage`, keyed by project slug. This is not account-scoped persistence or a shared calendar.
3. **The universal workspace is narrower in the UI.** The draft gives every material subject a workspace; the panel enables it for transitions and held `state:` materials, excluding external works. Upload/delete routes allow the project creator, whereas the draft describes project-party sharing.
4. **Visual connection creation is incomplete.** The canvas supports layout interaction, but connections are created with a form containing a typed target identifier. It does not yet implement drawing a connection between selected endpoints. Ordinary act forms also omit the proposed artifact picker and contributor controls.
5. **Missing parents disappear from the graph.** `projectGraph()` skips parent identifiers absent from the current node map. The draft instead calls for explicit “not held here” stubs. The generalized application should preserve restricted, absent or unresolved endpoint information without inventing endpoint content.
6. **Operations need an explicit projection rule.** The trajectory reader maps every YAML file under `transitions/`; the graph builder does not filter `kind: operation` before producing transition-shaped nodes. This differs from the design instruction to keep administrative operations in panels rather than substantive graph nodes.
7. **Authentication and recorded performer are not visibly bound in the web adapter.** The host checks the session party for project access, but calls `runGrrp()` with inherited process environment and no request-specific acting identity. Initializing a project creates a GRRP key in its record directory. The account party and recorded performer therefore need an explicit end-to-end identity check before shared authorship claims. Existing account-database tests alone do not test this binding.
8. **Mutable files are not immutable evidence snapshots.** `writeProjectFile()` can overwrite a path and `removeProjectFile()` deletes it without looking for citations. They do not themselves commit those file changes. The draft's “in Git” and cited-material guarantees should not be inferred from those helpers. Content-addressed storage and a record attachment manifest avoid meaning changing behind a stable record reference.
9. **Bundle import needs separate hardening before reuse.** `bundle.apply()` skips existing target paths without comparing bytes; the draft expects conflicting bytes for an identifier to be refused. It also joins archive names to the target after only a `trajectories/` prefix check. It does not establish safe, atomic, validated import by itself. No importer should be transplanted without path, size, schema, hash, identity-collision and partial-failure validation.
10. **Documentation and types have drift.** The TypeScript grounds list uses `exploratory` where Python uses `vulnerability`; its contribution type is a mapping while Python creates a list. README milestone/test statements are inconsistent. The source and behavior need to govern migration evidence, not a single README status paragraph.

## 5. Recommended compatibility boundary

Develop the generalized recording kernel and application first, as the user requested. Keep the academia entry point and domain workflow independent. A future adapter can map supported common fields and preserve the original GRRP bytes, identifiers, signatures, profile version and unmapped fields. It should report incomplete mappings explicitly; it should not rewrite signed historical records to make the two models appear identical.

Candidate shared conformance cases include stable record identity across revisions; distinction between Git commit parentage and semantic relations; relation records carrying their own modules; immutable attachment digests; atomic record-plus-relation creation; conflict rejection without losing either submitted revision; explicit missing-reference handling; portable export; and recovery from failed writes. Cross-application compatibility remains unclaimed until such fixtures have been implemented and independently read by both applications.

The reference project's [licensing file](../../applicative_infrastructure/gr_academia_application/LICENSE.md) identifies software as AGPL-3.0-or-later and specifications/documentation as CC BY 4.0 unless separately stated. Its `grrp/README.md` still says MIT. This review transfers design observations only; a later decision to copy software would need to account for the applicable source notices rather than rely on that stale README line.

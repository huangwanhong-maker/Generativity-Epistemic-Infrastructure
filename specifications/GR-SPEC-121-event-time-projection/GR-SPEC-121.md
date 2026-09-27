# GR-SPEC-121 — Event time and spacetime projection

**Publication class:** Specification  
**Maturity:** EXPERIMENTAL implementation draft; not an adopted Standard  
**Version:** 0.1, 2026-09-22  
**Protocol / record schema / profile:** `gsp-record-protocol/0.2` / `gsp-workspace/0.2` / `gsp.general/0.2`  
**Projection:** `gsp.spacetime/1`

## Foreword

This Specification defines a bounded event-order and temporal-applicability extension to [GR-SPEC-120](../GR-SPEC-120-information-model/GR-SPEC-120.md). It implements selected information and presentation needs from [GR-120](../../standards/GR-120-information-model/GR-120.md), [GR-140](../../standards/GR-140-presentation/GR-140.md) and GR-110 Working Draft 0.3, especially R110-008/049. These are draft dependencies, not adopted authorities or claims of whole-practice conformance.

The [design](../../docs/planning/event_spacetime_design.md) gives the source interpretation, including FM-007 section 9.7, PDF p.42, and FM-005 sections 5.14 and 5.20.4. [ADR-0011](../../decisions/ADR-0011-event-partial-orders-and-spacetime-projections.md) records the alternatives and operational selection. Requirements use **shall**, recommendations **should**, permissions **may**, and capability statements **can**. Identifiers `T121-*` apply only to this experimental draft.

## 1. Scope and interpretation

This profile represents explicit event precedence, event-bounded applicability, predecessor-closed selections, and topology/trajectory projections at one recording revision. Space means the represented relational topology. A geographic location can be a State or Property account with a declared reference system; graph layout coordinates have no geographic meaning by default.

The profile does not compute physical duration, infer ordering from dates, execute event effects, prove causal influence, identify a unique true State, establish target identity, or determine which competing account is warranted. Extended-event overlap, explicit onset/completion semantics, account-selection scenarios and formal transformation composition remain outside this edition.

**T121-001.** An implementation shall distinguish recording revision, represented event order, event cut, temporal applicability and display layout. It shall not infer event precedence from Git ancestry, recording time, UUID order, free-text `occurred_at`, or display rank.

**T121-002.** An Event reference in this profile shall denote an operational boundary marker in the declared account. Its use shall not assert that every represented event is instantaneous. Availability of an Event record in a later snapshot shall not be described as continued occurrence of its target event.

## 2. Module representation and compatibility

The extension adds two version `1` modules. It does not change existing record IDs, protocol/schema/profile identifiers, transaction operation names, or Git binding tree paths. Existing records require no stored migration to remain readable.

**T121-003.** Stored supported `gsp.event_order` and `gsp.temporal_extent` envelopes shall contain `version: "1"`, `required: true`, and their specified `data` object. Structural schema validation shall reject explicit false and stored omission of the required flag. Preparation of a supported new/set module may supply true when the request omits it; it shall not change explicit false into true silently.

**T121-004.** An implementation unable to interpret a required temporal module shall apply the unsupported-required-module mutation restriction in S120-014. Successful ordinary graph rendering shall not imply support for temporal interpretation. Implementations shall expose the added module versions and projection capability.

**T121-025.** Any retained `gsp.event_order` module with an unsupported version shall prevent a complete temporal projection, including when its older envelope declares it optional. The result shall contain an `unsupported_event_order` diagnostic, `consistent: false`, null event ranks, and null cut/topology. An unsupported retained `gsp.temporal_extent` version shall produce an `unsupported_temporal_extent` diagnostic and indeterminate presence, unless the account is withdrawn; it shall not alone invalidate an otherwise usable known event order. Opaque data shall remain retained unchanged.

All references below use the canonical UUID form and same-snapshot reference boundary of GR-SPEC-120. Data objects have only the fields defined here. Existing envelope, module-size and snapshot limits continue to apply.

### 2.1 Event order

```json
{"version":"1","required":true,"data":{"before":"<Event UUID>","after":"<Event UUID>"}}
```

The angle-bracket strings illustrate references; an actual instance uses canonical UUIDs.

**T121-005.** Both `before` and `after` shall resolve to records whose applied roles include Event. The owning record shall include the Relation role and supported `gsp.relation/1`. The latter shall contain exactly two incidences: the `before` target with orientation `in`, and the `after` target with orientation `out`, both with `reference_scope: represented_target`. Each incidence shall retain the ordinary independently identified, descriptively named role.

**T121-006.** Order records shall retain ordinary attribution, epistemic basis, modality, status, grounds and revision history. Duplicate endpoint assertions shall remain independently identifiable. Self-precedence and cyclic sets of otherwise well-formed assertions shall remain retained and receive consistency diagnostics rather than being erased as invalid archival evidence.

The module supplies strict ordering semantics. An arbitrary Relation predicate or direction does not. Temporal precedence does not by itself supply a causal or generative contribution claim.

### 2.2 Temporal extent

```json
{
  "version":"1",
  "required":true,
  "data":{
    "start":{"kind":"event","event_id":"<Event UUID>"},
    "end":{"kind":"unknown"},
    "basis":"Grounds, scope and continuity assumptions of this applicability account.",
    "subject_record_id":"<optional subject UUID>"
  }
}
```

**T121-007.** Extent data shall contain `start`, `end` and nonblank `basis` text of at most 2,000 characters. Each boundary shall be exactly `{kind: event, event_id: UUID}`, `{kind: unknown}`, or `{kind: unbounded}`. Event-boundary references shall resolve to Event-role records in the same snapshot. Any supplied `subject_record_id` shall resolve in that snapshot.

**T121-008.** Unknown shall mean an unresolved boundary. Unbounded shall mean that no limiting boundary is asserted on that side within the declared account scope; it shall not be treated as missing data or universal perpetual existence. The absence of the module shall remain distinguishable from each explicit boundary form.

**T121-009.** Start applicability shall be inclusive and end applicability exclusive. When both bounds reference events, the active ordering shall support strict start-before-end reachability. Equal, reversed or incomparable bounds shall make that extent indeterminate and produce a diagnostic; the extent shall not implicitly add an order assertion.

**T121-010.** Subject grouping shall remain a supplied account whose basis explains the relevant scope and continuity judgment. Grouping shall not itself establish represented-target identity, state exclusivity or causal continuity. The projection shall preserve separately identified and potentially competing accounts.

## 3. Ordering and cuts

Let `E` contain the snapshot's Event-role records. Let `A` contain edges from understood order modules whose owning records are not withdrawn. Reachability in `A` supplies strict precedence only when the selected ordering is acyclic.

**T121-011.** The projection shall preserve order-account IDs, endpoints and statuses. Withdrawn order accounts shall remain inspectable with `active: false` and shall not constrain `A`. Contested and non-realized temporal accounts shall remain included with visible qualifications. An active order referring to a withdrawn Event account shall produce a warning rather than silently dropping that explicit reference.

**T121-012.** The projection shall identify cycle diagnostics and their involved events/order accounts. If a cycle exists, `consistent` shall be false, event ranks shall be null, and `cut` and `topology` shall be null. It shall not produce a purported admissible cut by silently dropping assertions. The ordinary record graph remains separately available. Here `consistent` means an interpretable, usable selected event order, subject also to T121-025; it is not a finding of factual truth.

**T121-013.** For an acyclic ordering, an explicit selection shall contain at most 1,000 distinct Event UUIDs present in the selected snapshot. Its cut shall be the predecessor closure of those IDs. A cut shall include every known predecessor of every included event.

**T121-014.** Omitted selection shall denote the all-event cut. An explicit empty selection shall denote the empty cut. The result shall expose the requested IDs, included closure, added predecessor IDs, and maximal included events as the frontier. All-event selection shall use `requested: null` and `derived: []`.

**T121-015.** Event ranks shall be display aids derived from the selected acyclic order; they shall not supply additional precedence, simultaneity, duration, or physical position. Incomparability shall mean absence of supported ordering in this account, not established simultaneity or independence.

## 4. Presence, topology and trajectories

Presence is classification of retained applicability claims at a cut, not an assertion that the record's target exists or does not exist independently of those claims.

**T121-016.** Presence classification shall follow this priority:

1. A withdrawn account is `withdrawn`.
2. A retained unsupported extent is `indeterminate`; complete absence of an extent is `unscoped`.
3. Absence of a usable consistent cut, or a diagnosed invalid pair of event bounds, is `indeterminate`.
4. Either bound being unknown is `indeterminate`.
5. A known event start outside the cut is `not_started`.
6. A known event end inside the cut is `ended`.
7. Otherwise the account is `active` within its declared scope.

**T121-017.** Unknown and unscoped accounts shall remain inspectable and excluded from active topology by default. A presenter shall disclose this omission and shall not treat it as evidence of absence. Partial knowledge of one boundary shall not silently complete the other.

**T121-018.** A topology shall contain the canonical incidence graph's active record nodes and only edges whose two endpoints are active, at the same source revision. An active Relation node shall remain present even when some of its participants are not established active. The projection shall emit `partial_relation_at_cut` for omitted participant incidences or an explicitly incomplete participant account, identifying omitted participant-entry IDs. It shall retain Relation identity and scope/completeness qualifications without presenting the visible subset as complete or inventing missing participants, extents, or incidences.

**T121-019.** Trajectory groups shall identify each supplied subject reference and the retained record IDs grouped under it. They shall not impose an additional event order, infer identity from a shared label, select one competing account as true, or calculate event outcomes. Event-order inventory, applicability extents and grouped trajectories shall remain distinguishable in presentation.

## 5. Projection response

**T121-020.** The pure projection shall identify `head`, `project_id`, `protocol_version`, `projection: gsp.spacetime/1`, `consistent`, `events`, `orders`, `cut`, `extents`, `trajectories`, `presence`, `topology`, `diagnostics`, and `limitations`. The HTTP binding shall additionally distinguish the currently observed project head and any legacy read adaptation.

| Field | Content |
|---|---|
| `events` | ID, title, rank or null, direct predecessor/successor IDs, status, modality and descriptive `occurred_at` |
| `orders` | Owning Relation ID, before/after IDs, status and active flag |
| `cut` | Null, or requested/included/frontier/derived IDs as specified above |
| `extents` | Record ID, subject ID or null, start/end, basis and presence |
| `trajectories` | Subject ID and grouped record IDs |
| `presence` | Record ID and classified presence |
| `topology` | Null, or selected canonical nodes/edges |
| `diagnostics` | Code, message and applicable record/event/relation/participant identifiers |
| `limitations` | Declared interpretive and implementation limits |

**T121-021.** Every result shall derive from one pinned snapshot. Historical titles, roles, orders, bounds and qualifications shall not silently resolve against the current revision. Projection shall not modify records, create receipts or commits, or perform migration.

## 6. HTTP selection binding

The binding uses `GET /api/projects/{id}/spacetime` for the all-event cut and `POST` to the same path for `{ "after": ["<Event UUID>"] }`. Both accept optional `revision=<head>` using the existing revision authorization and reachability rules.

**T121-022.** GET and POST shall require the existing project authorization. POST shall additionally require the application's anti-forgery protection despite being read-only. Its JSON body shall contain exactly `after`, an array of at most 1,000 distinct Event UUIDs, and shall not exceed 64 KiB. Invalid selection shall not be silently truncated or coerced into all-event selection.

**T121-023.** Malformed or duplicate-member JSON, unknown body fields, invalid/non-Event references and unsupported request media types shall use the binding's applicable validation/media errors. A structurally readable chronology conflict shall return an inspectable projection with `consistent: false`, not be represented as a storage failure or silently repaired.

Projection requests have no transaction ID or `expected_head` because they publish no change. Temporal module editing continues to use ordinary transactions with their expected revision, actor, reason and retained receipt under [GR-SPEC-130](../GR-SPEC-130-interchange/GR-SPEC-130.md).

## 7. Assessment and limits

**T121-024.** An assessment shall distinguish module/schema validation, reference validation, chronology consistency, projection behavior, authorized HTTP behavior, presentation and substantive factual warrant. It shall identify the cases actually executed and shall not equate a passing diagram or acyclic ordering with an established historical account.

Required discriminating cases for this profile include earlier events inserted after later events; unknown order; explicit empty/all-event cuts; ancestor closure; self and multi-event cycles; withdrawal and contestation; unknown/unbounded/missing extents; equal/reversed/incomparable bounds; subject grouping; incomplete relation context; historical versions; denied access; body bounds; and a check that projection leaves the accepted Git head unchanged. This is an assessment schedule, not a claim that all checks have already passed.

The [reference implementation](../../applicative_infrastructure/common/packages/gsp_record_protocol/src/gsp_protocol/temporal.py) and packaged schema implement this bounded contract. Independent import compatibility, historical interpretation, accessibility and participant comprehension require separate evidence. Academia's native record protocol is not thereby aligned with this extension.

## Annex A — Informative source comparison

The [source-grounded design](../../docs/planning/event_spacetime_design.md) records exact manuscript pages, including the narrower consequential relational-event concept in FM-004 and the occurrence/attempt distinctions in FM-007. Their different scopes are preserved.

[Lamport 1978](https://lamport.azurewebsites.net/pubs/time-clocks.pdf), pp.559–560, supplies a comparison for partial orders and logical-clock displays. Its distributed-process assumptions are not assumed for social records. The [W3C 2017 Time Ontology Recommendation](https://www.w3.org/TR/2017/REC-owl-time-20171019/), sections 3.1–3.3, supplies a comparison for instants, intervals and temporal reference systems. This profile makes no claim to implement that ontology or its complete interval calculus.

# Event order, trajectories, and relational configurations

**Document class:** Informative design proposal  
**Status:** PROVISIONAL; EXPERIMENTAL version 1 implementation scope  
**Date:** 2026-09-22  
**Purpose:** Explain the source basis, distinctions, and limits of an event-defined spacetime presentation for the generalized application.

This document is an operational design proposal. It is not an adopted Standard, a complete formal semantics of GR, or a claim that the application reconstructs reality without interpretation. The [Programme Brief](../../PROGRAMME_BRIEF.md), [foundation manifest](../../foundation_manuscripts/MANIFEST.md), and [record graph review](graph_semantics_review.md) supply its programme context. The companion implementation Specification defines the exact machine contract.

## 1. The requested view

The requested space is the topology of the represented ontological configuration at an event-defined temporal cut: which Entity, State, Event, Process, Relation, and Property accounts apply, and which represented connections can be shown together. It is not necessarily geographic space. A physical location or coordinate can be recorded through the existing State or Property roles, with its subject, reference system, context, grounds, and temporal applicability made explicit.

The view has three coordinated parts:

- an event-order graph showing explicit precedence assertions;
- a graph of records whose temporal applicability supports inclusion at the selected cut;
- trajectories showing how related descriptions enter and leave applicability across event boundaries.

The ordinary record graph remains available. Its full inventory is useful even when temporal scope has not been supplied. A temporal view does not retrospectively supply that missing information.

## 2. Foundation passages and their operational consequences

Page numbers refer to one-based pages of the supplied PDFs. Findings below distinguish what the manuscripts say from the new engineering choices.

| Source read | Finding | Consequence proposed here |
|---|---|---|
| [FM-007, Metamodel II](<../../foundation_manuscripts/A Metamodel for Ontologically Heterogeneous Social Dynamics-II.pdf>), section 9.2, pp.38–39 | Occurrence indices, transition descriptions, and records of occurrences have different roles. Several records can concern one occurrence; repeated occurrences can share a transition description. | Record UUIDs identify retained accounts. Matching timestamps, labels, or effects do not automatically merge events or establish target identity. |
| FM-007, section 9.5, p.41 | A database change can reflect corrected observation of an earlier target event. Inquiry history and target history have distinct carriers and update meanings. | Separate Git revisions from represented event order and temporal applicability. |
| FM-007, section 9.7, p.42 | The text explicitly proposes “A partial-order event record” that retains known precedence while leaving unrelated events unordered. It leaves the extension as a modeling obligation beyond the sequential base. | Adopt a bounded explicit precedence graph. Do not claim that the manuscript already supplies the proposed interchange module. |
| FM-007, section 9.7, p.42 | Coarse timestamps can give apparent simultaneity; overlapping events can resist a faithful total order. Deriving an endpoint from a partial order requires justified treatment of its linearizations and any commutation assumptions. | Do not equate an order layer with physical simultaneity. Version 1 projects applicability assertions without executing event effects. |
| FM-007, section 12.6, p.59 | Sequence position, elapsed duration, calendar position, and historical periodization answer different questions. | An event frontier is not a measured duration, calendar timestamp, or universal historical period. |
| FM-007, section 15.3, p.72 | Joining transitions with mismatched endpoints can conceal an unrecorded intermediate step. Coherence of an event chain does not establish actual occurrence. | Do not invent intermediate states, replay unsupported transformations, or equate projection consistency with factual warrant. |
| FM-007, sections 15.4–15.5, p.73 | A continuing entity can have different states. Attributes and incidence require context to preserve change. | A subject can group several temporally scoped descriptions; grouping does not make their contents timeless. |
| [FM-005, GRE Chapter 5](../../foundation_manuscripts/GRE_Chapter_05_Complete_First_Draft.pdf), section 5.14.1, pp.114–115 | Relational trajectories require a continuity criterion, observational horizon, temporal resolution, boundary, and scale. Latency and gaps remain possible. | Explain the basis of subject grouping. Preserve unknown intervals and distinguish disappearance from the screen from historical dissolution. |
| FM-005, section 5.14.2, pp.115–116 | Generative trajectories concern historically connected contributions; precedence alone is insufficient, and alternatives can remain unrealized. | The view is a temporal reconstruction aid. It does not label every displayed path a causal or generative explanation. |
| FM-005, section 5.20.4, pp.172–173 | Historical revision changes present knowledge of the past while preserving earlier claims and their evidential conditions. | Select both a recording revision and a temporal cut. Later insertion can change a current reconstruction without changing earlier Git snapshots. |

### 2.1 An event-scope difference that remains explicit

[FM-004, GRE Chapter 4](../../foundation_manuscripts/GRE_Chapter_04.pdf), section 4.15.2, pp.116–117, describes a relational event as a temporally delimited reconfiguration consequential for later relations. It permits extended duration and retrospective recognition. FM-007, sections 9.5–9.6, pp.40–41, also retains renewals and failed attempts that can leave a selected state unchanged.

These formulations address different analytical questions. The application does not silently replace either. Its Event role identifies an operational occurrence account. Version 1 uses selected Event references as boundary markers for a declared reconstruction. That narrower use does not assert that every GR event is instantaneous, consequential at every scale, or fully understood by its recorded transition.

## 3. Three independent coordinates of a view

Every projection identifies the following:

1. **Recording revision:** the Git snapshot containing the accounts and their versions.
2. **Event cut:** a selected downward-closed set under that snapshot's usable precedence assertions.
3. **Presentation selection:** filters, layout, grouping, and omitted or unresolved records.

For example, a user first records a dissolution event and later discovers the formation event that preceded it. The later Git revision can contain both events and an explicit formation-before-dissolution account. The represented chronology follows that assertion. The earlier Git revision still records what had been retained before the discovery.

Git commit timestamps do not determine target time. The descriptive `occurred_at` field remains useful context, but version 1 does not parse it into mandatory event precedence. Drawing one node above another likewise adds no precedence assertion.

The account's modality and epistemic basis remain visible. A hypothetical occurrence can be recorded; it does not become actual because its graph has no cycles. Mixing incompatible scenarios can require a narrower account selection in a future profile.

## 4. Explicit event-order accounts

An order assertion is a first-class Relation record. Its `gsp.event_order` version `1` module carries `before` and `after` Event-record references. Its ordinary `gsp.relation` module carries the matching two target incidences. The required module declares that temporal interpretation is needed to handle the account correctly; it does not turn the assertion into an established fact.

The ordinary record retains attribution, epistemic mode, modality, assessment/dispute status, evidence, uncertainty, revision reasons, and relevant qualifications. The temporal projection therefore remains connected to the statement responsible for an edge.

The selected interpretation is strict precedence between operational boundary markers. Its transitive closure can support additional precedence conclusions. The implementation distinguishes submitted edges from derived reachability and does not create extra source assertions merely because a path exists.

An untyped navigation link, an arbitrary Relation predicate, and a directed incidence are insufficient to assert precedence. Event order also does not establish causal influence or a generative contribution.

### 4.1 Contradictions and disputed assertions

Structural checks establish whether the typed references and module shapes are interpretable. They do not establish that every temporal assertion is compatible with every other one.

Contradictory precedence accounts remain recordable. A directed cycle prevents the selected edge set from defining a strict partial order. The projection reports `consistent: false`, identifies the affected assertions, and provides no admissible-cut projection until the conflict is resolved or the selected account changes. It does not silently drop an edge or impose an arbitrary order.

Withdrawn order accounts are omitted from the active ordering, while their records and histories remain available. Contested order accounts remain included with visible qualification. Inclusion means that the projection is conditional on those assertions; it is not an endorsement. User-selectable alternative chronology sets and richer adjudication are future work.

Unsupported retained event-order versions also prevent a complete cut/topology projection, even if an older producer marked that module optional. Their constraints cannot safely be omitted from a claimed complete ordering. The result identifies `unsupported_event_order` and `consistent: false`; this flag concerns a usable interpreted order, not truth. Unsupported temporal extents instead make the affected account's presence indeterminate, with a diagnostic, while leaving an otherwise usable event order available. Opaque modules remain retained.

## 5. Cuts, frontiers, and the limits of “same time”

For a selected acyclic event relation, write `a < b` when there is a directed precedence path from `a` to `b`. A cut `C` includes all predecessors of each included event:

```text
b is in C and a < b  =>  a is in C
```

This is a downward-closed set, also called an order ideal. The frontier is its set of maximal included events. Including a later event includes its known predecessors; removing an included predecessor requires removing its included successors. These are consistency rules for navigating an account, not rules governing reality.

If neither `a < b` nor `b < a` is supported, the events are incomparable in this account. This does not by itself establish simultaneity, physical independence, overlap, or absence of a causal connection. An unrecorded order can still exist.

Topological layers can arrange the event graph compactly. Their layer numbers are display coordinates. Equal layer numbers do not assert equal occurrence times; different layer numbers do not establish precedence between every pair.

A frontier describes a configuration relative to the available order and applicability claims. It need not correspond to one uniquely identified physical instant. There can be several admissible cuts through unordered activity. Version 1 keeps the event graph and selected cut explicit instead of presenting one arbitrary total replay as the recovered history.

## 6. Temporal applicability and trajectories

The required `gsp.temporal_extent` version `1` module can accompany any record role. It contains `start`, `end`, `basis`, and an optional `subject_record_id`.

Each boundary has one of these forms:

| Boundary form | Interpretation |
|---|---|
| `{kind: event, event_id: ...}` | Applicability is bounded by the referenced operational Event marker. |
| `{kind: unknown}` | The boundary has not been sufficiently established. |
| `{kind: unbounded}` | No limiting boundary is asserted on this side within the declared account's scope. |

An unbounded boundary is a positive scope statement. It is not the default meaning of missing data and does not establish that a target has existed or will exist throughout all time. The `basis` explains the grounds and scope of the applicability account, including relevant continuity assumptions or uncertainty.

Version 1 uses start-inclusive, end-exclusive applicability: an included start marker opens the interval, and an included end marker closes it. If both boundaries name events, their strict start-before-end order needs support in the selected event relation. Without that support, the extent remains indeterminate and receives a diagnostic. The extent itself does not invent the missing precedence edge. Equal or reversed markers do not receive an invented positive duration.

### 6.1 Classification at a selected cut

The projection distinguishes:

- `active`: the declared boundaries support applicability at this cut;
- `not_started`: the supported opening boundary has not been crossed;
- `ended`: the supported closing boundary has been crossed;
- `indeterminate`: supplied temporal information is insufficient or inconsistent for a definite applicability result;
- `unscoped`: no temporal extent was supplied;
- `withdrawn`: the account is withdrawn and remains available for inspection outside active topology.

Unknown and absent boundaries remain visible. They are excluded from the active topology by default, with counts and an inspection path. A display can include them as explicitly qualified context; it cannot silently promote them to active. Missing temporal data is not a claim of nonexistence.

An unknown boundary can prevent a positive active result even when the other boundary is known. Where a known bound supports a narrower statement, the interface can identify it without treating the unknown bound as resolved. Exact classification priority and diagnostics belong to the implementable projection contract.

### 6.2 Subject grouping and continuity

The optional `subject_record_id` associates a temporally scoped description with a subject account for trajectory presentation. For example, several State records can describe different conditions of the same organization under a stated continuity criterion.

The reference is a supplied grouping claim, not an identity proof. Its basis matters, especially across renaming, merger, dissolution, succession, or disputed continuity. The renderer does not merge records automatically because their titles match, their endpoints coincide, or they occur in adjacent layers. Without a grouping claim, a record can retain its own temporal trace.

Competing State accounts can both remain visible. This version does not select a unique true state, impose property exclusivity, calculate a next world state, or decide which account defeats another.

### 6.3 An Event boundary is not its record's lifetime

The Event marker used by the ordering graph identifies a represented boundary in the inquiry. Its recording account can remain available in every later Git snapshot. Availability of that account does not mean the event is still occurring.

Likewise, attaching a temporal extent to an Event-role record scopes the account's applicability under this profile. It does not silently redefine the represented event's physical duration. The separate event-order diagram continues to show the selected event inventory and cut frontier. This avoids disappearing historical occurrences merely because an ontology configuration uses a narrower applicability filter.

Extended events, onset/completion distinctions, interval overlap, and the relation between an event's occurrence interval and its use as a boundary require a richer profile. They remain explicitly deferred rather than being approximated through creation timestamps or arbitrary zero-length intervals.

## 7. Constructing the topology

At a consistent cut, the primary topology is induced by the records classified as active. Every active record remains a node, including a Relation whose participants are not all established active. An edge is retained only when both endpoint records are active. A Relation with omitted participant incidences or an explicitly incomplete participant account receives a `partial_relation_at_cut` diagnostic identifying the omitted incidences. The interface preserves its full account for inspection and does not present the displayed subset as a complete relation.

This induced-subgraph policy preserves access to an active relational account instead of removing it because another account lacks temporal scope. It also avoids inventing active participants to complete the picture. Keeping the Relation node is a presentation of its scoped account, not a claim that every part of the relationship has been reconstructed.

Relations retain their own IDs and histories. An independently recorded relation can appear, change through a separately scoped account, or cease to apply without changing its participants' identities. The existing incidence representation continues to support n-ary relations and relations whose participants are relations.

Record scope, relation participant completeness, and hidden display filters remain separate limitations. An active Relation with an explicitly incomplete participant account still needs that qualification even when every retained participant is active.

The graph is a projection of temporal assertions at one recording revision. It is not produced by replaying Git edits as world events, and it is not a simulation of all event effects. In particular, unordered events with potentially incompatible effects do not receive a fabricated outcome through identifier sorting.

## 8. External reference boundaries

[Lamport's 1978 paper](https://lamport.azurewebsites.net/pubs/time-clocks.pdf), pp.559–560, supplies a precise comparison for partial event order and logical-clock displays. Its concurrency relation does not require equal physical time. Its assumptions concern distributed processes and messages; the application does not infer that those assumptions hold for historical or social records. The proposed cut and applicability rules are this programme's declared operational choices.

The [2017 W3C Time Ontology Recommendation](https://www.w3.org/TR/2017/REC-owl-time-20171019/), sections 3.1–3.3, distinguishes instants, intervals, interval relations, temporal reference systems, and temporal granularity. It is a useful comparison for a later interval profile. Version 1 neither implements the full ontology nor claims RDF/OWL interoperability. The version-specific citation matters: the generic OWL-Time publication URL currently resolves to a 2022 Candidate Recommendation Draft.

## 9. Discriminating review cases

| Case | Expected design result |
|---|---|
| A later event is recorded before an earlier event | Explicit precedence determines represented order; Git retains insertion order independently. |
| Two events have no supported order | They remain incomparable; shared display position does not assert simultaneity. |
| Two accounts assert opposite strict orders | Both survive; the selected ordering is diagnosed as inconsistent and no cut topology is asserted. |
| An order account is contested | Its qualifications remain visible and the projection discloses its conditional inclusion. |
| A State has two boundary events with no supported precedence | The extent is indeterminate; no order is inferred from the extent alone. |
| An old agreement has no temporal module | It appears as unscoped context, rather than being assigned the date of its first commit. |
| A Relation is active while a participant's applicability is unknown | The relation's boundary context is exposed; the active graph does not display a complete unsupported arrangement. |
| Two scoped State accounts make incompatible claims about one subject | Both remain inspectable; temporal projection does not silently adjudicate them. |
| A later correction changes the claimed onset | The current reconstruction changes; the earlier Git revision still shows the earlier claim. |
| An Event record remains in later snapshots | Record retention does not imply continued event occurrence. |

## 10. Remaining questions

- **OPEN:** How should users select competing coherent chronology accounts while preserving the sources, exclusions, and reasons for each selection?
- **DEFERRED:** How should extended events, intervals, overlap, approximate calendar dates, and multiple temporal reference systems interact with event boundaries?
- **OPEN:** Which subject-continuity and relation-continuity criteria are suitable for particular domains, and how should competing criteria be compared?
- **DEFERRED:** When should a profile interpret event effects as transformations, and what evidence or formal conditions justify equal outcomes across different linearizations?
- **OPEN:** Do users understand the distinction between a cut, an order layer, an actual time, an unknown interval, and a recording revision? Browser correctness alone does not establish that comprehension.

The implementation advances a bounded presentation capability under the existing temporal and epistemic requirements. It leaves causal inference, complete historical reconstruction, and the broader theory of generativity available for further work.

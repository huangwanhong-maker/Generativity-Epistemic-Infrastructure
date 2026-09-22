# Open Questions

**Document class:** Informative issue register  
**Status:** OPEN / evolving

## Graph protocol and modular application, 2026-09-22

[ADR-0008](../../decisions/ADR-0008-record-graphs-transactions-and-modules.md), the [design](graph_workspace_design.md), [source review](graph_semantics_review.md) and [academia comparison](../reviews/academia_application_reference.md) record the selected experimental choices. They do not settle the corresponding conceptual or institutional questions.

| ID | Status | Question and present treatment | Needed by |
|---|---|---|---|
| Q-38 | EXPERIMENTAL | How should relation identity, unknown participants and cross-project references evolve without leaking protected targets or equating record identity with target identity? The first profile uses local UUID references, distinct incidences and explicit participant completeness; zero-incidence and cross-project structured relations remain outside the profile. | Wider graph profile and protected collaboration |
| Q-39 | OPEN | How should external material stores, physical quotas, selective erasure, sealing and independent preservation custody work with atomic transactions and existing history/bundles? The first binding retains bounded Git assets; detachment is not erasure and logical asset budgets are not disk quotas. | Sensitive records, hosting or larger material |
| Q-40 | OPEN | Do intended users understand Relation nodes, reference scope, hidden incidences, historical selection and changes of epistemic basis? Canvas alternatives and browser tests do not establish accessibility or measured comprehension. | Participatory graph/interface evaluation |
| Q-41 | DEFERRED | Which academia GRRP records map exactly, partially or not at all to the shared profile while preserving native IDs, signed bytes, registrations, disclosure and material? ADR-0009 completes source/runtime consolidation and independent entry points; an adapter fixture corpus and identity binding remain required. The original checkout remains unchanged and the imported application retains native GRRP. | Academia upgrade |
| Q-42 | PROVISIONAL | Which optional-module, profile and registry governance preserves extension semantics across independent producers? Unknown optional module envelopes survive unrelated edits, unknown required semantics block mutation, and unsupported binding manifests are not rewritten. This is conservative pilot behavior, not a completed extension-governance system. | Second implementation and import design |
| Q-43 | OPEN | What deployment, host/cookie isolation, account recovery, dependency maintenance and backup-restoration procedures are required before operating the consolidated applications beyond local development? Separate ports and cookie names support independent local use but do not create a browser security boundary. The inherited academia dependency advisories and historical identity/custody findings require a dedicated upgrade. | Domain deployment and academia maintenance |
| Q-44 | DEFERRED | Which hosted URLs, access policies, CI checks and release procedures will serve the four source repositories? ADR-0010 establishes committed local nested submodules; the user will create remotes after local commits. Local relative URLs must be replaced before remote publication. MIT repository licensing does not license private application records or adopt governance drafts. | Remote repository creation and releases |

The new transaction categories partially address Q-36 without completing structured mixed epistemic grounds or source assessment. Q-34/35/37 remain open; a graph and receipt ledger do not supply shared governance, authorized erasure or actual contestation.

## Experimental application, 2026-09-22

[ADR-0007](../../decisions/ADR-0007-git-backed-project-workspace.md) records the private-project Git workspace; [its design](../../applicative_infrastructure/gr_generalized_application/web_application/DESIGN.md) distinguishes working features from draft conformance. User authorization to implement a pilot does not adopt a technical Standard.

| ID | Status | Question and present treatment | Needed by |
|---|---|---|---|
| Q-34 | DEFERRED | What membership, invitation, contribution and historical-access rules should shared projects support? The first implementation authorizes only the project creator. | Collaboration release |
| Q-35 | OPEN | How should retention, erasure, account recovery, repository repair and backup restoration be operated and verified? Revision and withdrawal currently retain historical content, including in bundles. | Sensitive use or hosted deployment |
| Q-36 | EXPERIMENTAL | Which richer claim/evidence objects, mixed epistemic bases, observation/acquisition times and semantic revision distinctions are needed beyond the current primary-role form? Optional prose preserves qualifications but does not enforce the complete draft model. | Pilot feedback and GR-SPEC-120/200 development |
| Q-37 | OPEN | Which accountable actors and workflows can support an actual challenge, reasoned review, notification and practical remedy? Owner-assigned record status is not an independent contestation procedure. | GR-210 application pilot |

## Substantive validation cycle, 2026-09-22

The [0.2 validation dossier](../reviews/validation_0.2/README.md) records review coverage, P0/P1 results and corrections implemented in the preparatory 0.3 edition. [ADR-0006](../../decisions/ADR-0006-validation-and-assessment-boundaries.md) explains the operational choices. Fixed drafting defects do not close the broader governance and theoretical questions below.

| ID | Status | Question and present treatment | Needed by |
|---|---|---|---|
| Q-31 | OPEN | May a final ballot open while the reply-to-disposition window is still open, or must the windows be sequential? P0/v2 uses a conservative sequential design; it does not adopt that choice. | Before a live review/ballot schedule |
| Q-32 | OPEN | How are start events, closing instants and calendar days interpreted across time zones and daylight-saving transitions? Notices need exact times; synthetic elapsed-day examples do not establish a universal civil-calendar convention. | Before a live time-bound procedure |
| Q-33 | PROVISIONAL | Does conditional R110-112 adequately record recirculation stages and link-specific uncertainty without treating every generativity-return account as recirculation? Its source basis is FM-005 section 5.17.3, p.145; broader definition, valuation and entitlement remain unresolved. | Source/author review and relevant return-account pilot |

Q-22/26/28 remain open: all requirements have editorial review coverage, but that supplies no actual founding authority or measured participant burden. P1/v2 leaves retention-decision applicability unresolved and supplies no actual notification, review or repair evidence. Its fulfilled documentary components do not establish whole-practice conformance.

## Expanded working-draft review, 2026-09-22

The [0.2 package](../../publications/README.md) develops the existing proposals in greater detail. [ADR-0005](../../decisions/ADR-0005-expanded-drafts-and-source-models.md) records why ISO, IEEE, ITU and UN sources have different roles. The earlier questions remain open unless a separate decision closes them.

| ID | Status | Question and current treatment | Needed by |
|---|---|---|---|
| Q-27 | PROVISIONAL | Is the local formal publication structure suitable for this family? The draft uses ISO-like numbered clauses and labelled annexes, accessible IEEE drafting rules and ITU comparison without importing institutional authority. Full inaccessible external resources need review before more detailed reliance. | Editorial adoption and external release |
| Q-28 | OPEN | Which expanded obligations are proportionate for different practices, and what justified profiles would reduce burden while preserving material uncertainty, protection and accountability? No reduced conformance profile is implied by a short form. | Participatory rehearsal and GR-150 development |
| Q-29 | PROVISIONAL | Is EIP-069's reasoned 'more likely than not' criterion appropriate for internal conduct findings, with fair-response and independent-review safeguards? Alternatives include a higher threshold, contextual criteria or non-adjudicative findings. Neither the criterion nor a finding establishes external legal liability. | Governance and ethics review before adoption |
| Q-30 | OPEN | Which UN-system institutional model or potential counterpart fits an actual pilot purpose and mandate? ITU, UN/CEFACT and UN statistical guidance perform different functions; no sponsor, partnership or policy adoption is presumed. | Any international policy proposal |

Normative schedules in the 0.2 drafts are operational proposals for review. Their presence does not establish that an actual institution can supply the stated evidence or that affected users find the procedure usable. P0/P1 retain their historical 0.1 scope until reassessed.

## First working-draft review, 2026-09-22

The [initial package](../../publications/README.md) supplies proposed operational answers, not resolutions of this register. [ADR-0004](../../decisions/ADR-0004-initial-working-draft-package.md) records alternatives and affected requirements. The following choices need focused review before progression:

| ID | Status | Question and current draft treatment | Needed by |
|---|---|---|---|
| Q-21 | PROVISIONAL | Are D1's 60-day public review, 30-day re-review/ballot/appeal periods, two-thirds returned-ballot quorum, three substantive votes and two-thirds substantive approval practicable and sufficiently inclusive? Numerical support alone does not establish stakeholder balance. | Governance adoption |
| Q-22 | OPEN | Who can sign the founding instrument with actual authority over programme resources and publications, accept roles and provide neutral review? The unsigned template does not create those capacities. | Governance adoption |
| Q-23 | PROVISIONAL | Are preparatory GR-100/110/200/210 scopes the right first technical cycle, and do the dependency boundaries prevent hidden package obligations? All remain unapproved projects pending governance. | NWIP decision and next technical draft |
| Q-24 | OPEN | What limited package/case conformance profiles are useful? This draft permits partial documentary assessment but does not treat it as observed practice or procedure fulfillment. | Pilot assessment design / GR-150 |
| Q-25 | OPEN | Which alternative reading format and accessibility target can be sustained? LuaLaTeX compilation, extracted text and source availability do not establish PDF accessibility or user comprehension. | Before external formal release |
| Q-26 | OPEN | What are the measured recording/review costs and effects on affected participants? P0 arithmetic and P1 authored scenarios supply no observed staffing, elapsed effort, participation, review fairness or remedy evidence. | Participatory rehearsal / live pilot |

Q-05/06/07 now have explicit candidate operational treatments in the TeX drafts, including overlapping roles, distinct modality/epistemic mode and three revision reasons. They remain open to source and pilot review; Property/Attribute, manuscript chronology and formal emergence assumptions are not settled. Q-03 rights choices also remain unresolved: the policy draft supplies a rights-clearance process, not a licence grant.

## Decisions needed for the current development plan

These priorities supplement the earlier thematic questions below. Milestones refer to the [comprehensive plan](comprehensive_development_plan.md). Roles are proposed and unassigned. An operational resolution records alternatives, rationale, source mappings and affected documents; it does not retroactively resolve a philosophical question.

| ID | Status | Decision or investigation | Needed by | Proposed accountable role |
|---|---|---|---|---|
| Q-01 | OPEN | Who holds founding authority, and how are governance adoption, delegation, conflicts and independent appeals made credible for a small programme? | M1 | Founding authority / governance editor |
| Q-02 | OPEN | Which participation, consensus, review, objection and appeal rules are practicable? Which ISO/IEC or IEEE mechanisms are adopted, adapted or deferred, and why? | M1 | Governance editor and stakeholder reviewers |
| Q-03 | OPEN | What licensing and contribution terms apply separately to publications, schemas, examples, software and third-party source material? | M1 | Governance and IP reviewers |
| Q-04 | PROVISIONAL | Is GR/NWIP-001 one coordinated package or several linked work items, and which responsible editors have capacity for its first cycle? | M1–M2 | Sponsor and established decision body |
| Q-05 | OPEN | How do Part II's Attribute and the repository's Property relate? Are the six roles overlapping, and how are admissibility, identity and persistence scoped? | M2 | GR-100/120 editors |
| Q-06 | OPEN | How are claim, observation, trace, evidence, interpretation and judgment represented without conflating target reality, source records and institutional acceptance? | M2 | GR-200/210 editors |
| Q-07 | OPEN | What records distinguish world change, evidence change and category/regime change, including revised identity and retrospective recognition? | M2 | GR-100/120/200 editors |
| Q-08 | OPEN | Which version relationships among the foundation manuscripts are author-confirmed, and which apparent formal or terminological differences remain unresolved? | M2 for definitions relying on them | Source editor and author |
| Q-09 | OPEN | How are Part I's fixed-carrier collapse results and stronger interpretive language qualified by Part II? Which emergence claims require additional assumptions? | Before reliance in a requirement | GR-TR101/102 editors |
| Q-10 | PROVISIONAL | What is the smallest useful generative history: which changes in capacity, maintenance, constraints, alternatives and loss need grounds, and when is unknown the appropriate result? | M2–M3 | GR-110/210 editors and pilot users |
| Q-11 | OPEN | How are corrections, access withdrawal, retention, deletion and permitted audit traces reconciled without an unconditional immutable-history requirement? | M2 before sensitive pilots | GR-160/310 editors and contextual reviewers |
| Q-12 | OPEN | Which conformance subjects and profiles separate capability from recording depth? Does the candidate name Standard profile confuse publication status? | M2 | GR-150 editor |
| Q-13 | PROVISIONAL | Which institutional procedures need machine protocols, and when do portable exchange, notifications or federation justify separate technical work? | M3 | GR-130/200/400 editors |
| Q-14 | OPEN | How will pilot burden, meaningful contestability, presentation comprehension and unequal access be evaluated, and what result warrants redesign? | M2, before measurement | Pilot lead and affected participants |
| Q-15 | DEFERRED | Which jurisdictional profiles, generativity-return assessments, formal semantics or additional publications need development beyond the first cycle? | After relevant pilot evidence | Relevant editors and domain reviewers |
| Q-16 | OPEN | How are ethical disagreement, distributive effects, recognition and responsibility assessed without implying one universal value ordering? | M2 and relevant policy analysis | Ethics reviewer and affected participants |
| Q-17 | OPEN | Which legal concepts and sources of authority are relevant to the selected use case, and which unresolved conflicts require contextual analysis or a policy proposal? | Before contextual pilot claims | Jurisprudential and institutional reviewers |
| Q-18 | OPEN | Which international organization or class of institutions has a suitable mandate, and how do voluntary pilots, shared stewardship, existing infrastructure or deferral compare? | Before any policy recommendation | Policy editor and mandate reviewer |
| Q-19 | OPEN | How are ownership, access, funding, maintenance cost, infrastructural dependency and returns to contributors governed without concentrating recognition power? | M1–M4 as scope develops | Programme governance and political-economy reviewers |
| Q-20 | OPEN | Which cross-domain review competencies can actually be supplied, and how do participation gaps limit scope or progression? | M1–M2 | Sponsor and review coordinator |

The [foundation map](foundation_traceability.md) preserves source tensions. The [technical landscape](external_standards_landscape.md), [development comparison](standards_development_comparison.md), and [cross-domain programme](cross_domain_programme.md) distinguish external sources from proposed local choices. No external compatibility or institutional affiliation is assumed.

## Ontology

- Are Entity, State, Event, Process, Relation, and Property sufficient for the core record ontology?
- Should Observation, Claim, Interpretation, or Agent be first-class ontology classes or profiles built from the six?
- How should identity and persistence be represented without assuming substance ontology?
- How should relation identity be handled through time?

## Generativity

- How should a record represent the emergence of a distinction that was previously unavailable?
- How should changes in a possibility field be represented?
- How should unrealized branches be recorded without implying that they were equally feasible?
- How should generativity return be represented?

## Formal semantics

- Which category-theoretic structures are most appropriate?
- Should the core information model remain formalism-neutral?
- How can state transition, relational change, and persistence coexist in one mathematical framework?

## Standards governance

- What threshold constitutes consensus?
- Which stakeholder classes must be represented in public review?
- What is the minimum viable appeals process for a small NGO?
- What licensing model best supports open implementation and future formal standardization?

## Conformance

- What does it mean for a record to conform?
- Should there be Core, Standard, and Deep conformance profiles?
- Which clauses can be automatically validated?

## Privacy and ethics

- How should preservation be separated from publicity?
- How are sealed, restricted, anonymized, and destroyed records distinguished?
- How should third-party generative contributions be recorded without creating surveillance risks?


## Foundation corpus

- Which GR manuscripts should be designated foundational rather than supplementary?
- Which terms have changed meaning across the GR corpus?
- Which notational conventions should GR-100 preserve?
- Where should a standards-specific operational definition deliberately diverge from a broader GR concept?
- How should manuscript-to-clause traceability be represented?


## Epistemics

- Should Observation, Claim, Evidence, Interpretation, Judgment, and Attribution be first-class record classes, or epistemic wrappers/profiles around other records?
- How should evidentiary provenance and uncertainty be represented?
- How should conflicting interpretations coexist without premature canonicalization?

## Jurisprudence and law

- Which legal concepts require dedicated profiles rather than generic metadata?
- How should correction, annotation, objection, access, restriction, and preservation rights be represented?
- How should the standards distinguish technical conformance from legal validity?
- What kinds of jurisdiction-specific legal profiles are appropriate?

## International organizations and policy

- Which IO functions are plausible first-use environments: research integrity, cultural heritage, AI governance, institutional memory, scientific collaboration, or public administration?
- Which proposals should be global, and which should be mandate-specific?
- What pilot designs can test utility without imposing excessive recording burdens?


## Publication classes

- Earlier question: which projects begin as Specifications versus Standards? The default direction is resolved by accepted ADR-0002: begin from the Standards track. The remaining question is which requirements justify particular Specification work items, including the boundaries of GR-120/130/200 and their corresponding Specifications.
- Should project-family numbers remain stable across promotion from Specification to Standard?
- When should a policy proposal be classified as an International Policy Proposal versus an International Institutional Proposal?
- What minimum evidence is required before a Specification may be proposed as a Standard?

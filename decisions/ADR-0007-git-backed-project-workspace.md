# ADR-0007 — Git-backed project workspace

**Status:** PROVISIONAL; selected for experimental implementation  
**Date:** 2026-09-22  
**Class:** Application architecture and scoped operationalization decision record  
**Related decisions:** [ADR-0002](ADR-0002-standards-first-specifications-on-demand.md), [ADR-0006](ADR-0006-validation-and-assessment-boundaries.md)

## Issue and authorization

The user requested a web application implementing the programme's ideas, with registration, login, project creation, recording within projects, and Git as each project's record backend. The current Working Drafts identify practical needs for distinguishable records, qualified assertions, attributed revision, historical inspection, and bounded access. An application can expose implementation difficulties and recording burden that authored examples alone cannot establish.

The application is an experimental implementation of selected functions. It is not an adopted Standard, a complete GR information model, a formally approved institutional pilot, or a finding of conformity to GR-100, GR-110, GR-200, or GR-210. The user has authorized implementation now. This follows ADR-0002's provision for implementation during draft development; it does not constitute programme adoption or work-item approval. The accompanying [design](../applicative_infrastructure/gr_generalized_application/web_application/DESIGN.md) is informative implementation documentation, without a new GR publication number.

## Alternatives

| Alternative | Benefit | Limitation relevant to this request |
|---|---|---|
| Store projects and record history solely in a relational database | One transactional system and straightforward queries | Does not provide the requested per-project Git record backend |
| Store accounts, sessions, projects, and records together in Git | One storage mechanism | Replicates credential and session material into retained history; weak fit for authentication queries and expiration |
| Use SQLite for application accounts and access lookup, with a separate Git repository for each project's manifest and records | Separates authentication from retained project content; project history is inspectable using Git | Requires explicit authority and failure boundaries between the database and repositories |
| Depend on a hosted Git service for every operation | Reuses remote collaboration and repository administration | Adds a service account, network dependency, and permission system before the local recording workflow is established |

The third alternative is selected. Flask serves the application and JSON endpoints, SQLite holds account/session information and project ownership lookup, and a separate bare Git repository holds each project's authoritative manifest and record snapshots. A browser interface uses ordinary HTML, CSS, and JavaScript. This bounded stack can run locally without an external hosting account.

## Decisions and rationale

Projects initially have one owning account and private application access. The application does not infer that account ownership establishes legal ownership of recorded material. Registration establishes an application identity; it does not verify a person's real-world identity, evidence, competence, or authority.

Each accepted project or record mutation creates a new Git snapshot. A record has a stable application identifier; a commit identifies a retained project version. Revision requires a reason and records the authenticated actor and server time. The client supplies the project revision it edited, and the server advances the main reference only if its previous value still matches. Git's [`update-ref` old-value check](https://git-scm.com/docs/git-update-ref) supplies this conditional update; [`commit-tree`](https://git-scm.com/docs/git-commit-tree) constructs a commit from a tree and parent. A conflicting writer receives a conflict rather than silently overwriting the intervening update. These mechanisms do not make SQLite and Git one transaction.

Project content is represented as versioned JSON, not as arbitrary user-selected filesystem paths or executable files. Password material and sessions remain outside project Git history. Git authorship identifies the recording account through server-assigned attribution; a record's separate `attributed_to` field identifies the source or observer as represented by the recorder. Attribution remains a claim unless separately examined.

The six role labels are Entity, State, Event, Process, Relation, and Property. One primary role is selected for a particular application record. This is an interface limitation, not a claim that targets fit exclusive kinds. Multiple linked records can describe the same target from different roles, without the linkage itself proving target identity. Role-specific admissibility and identity criteria remain incomplete. The application retains the programme's current label Property without deciding the unresolved relationship to the manuscript's Attribute.

The interface keeps epistemic mode, asserted modality, and owner-assigned status separate. This preserves a useful part of V100-016 and R110-012. Its finite mode list remains narrower than the drafts: a retrospective inference, for example, needs a primary choice with further qualifications in prose. Neither a completed form nor a status choice constitutes an evidence assessment. Evidence references and alternatives can be supplied without being described as verified or exhaustive.

## Source basis and operational limits

The design was checked against [PROGRAMME_BRIEF.md](../PROGRAMME_BRIEF.md), the active 0.3 drafts, and these local foundation passages:

- [FM-001](../foundation_manuscripts/GRE_Chapter_01.pdf), section 1.14.4, PDF page 81, distinguishes authenticity, provenance, and epistemic trust. This motivates separating the record's history from the warrant of its content.
- [FM-005](../foundation_manuscripts/GRE_Chapter_05_Complete_First_Draft.pdf), section 5.4.1, PDF page 29, treats evidence relative to a claim and an inquiry. The application's evidence text therefore records supplied grounds or references, without automatically assessing their evidential role.
- FM-005, section 5.20, PDF pages 168 and 170, motivates reasoned, traceable revision with earlier formulations retained. The first application captures revision reasons and earlier snapshots but does not implement the full distinction among evidence updates, target changes, and category changes.
- [FM-007](<../foundation_manuscripts/A Metamodel for Ontologically Heterogeneous Social Dynamics-II.pdf>), PDF page 2, presents overlapping analytical categories and leaves exhaustiveness and metaphysical priority open. Application role selection does not settle those questions.

These are scoped design translations. No foundation manuscript is promoted to a conformance authority. No new metamodel class, equivalence claim, universal generativity score, or legal determination is introduced. Exact feature-to-draft references and gaps appear in the design document.

## Consequences, protection, and maintenance

Git provides retained versions while the repository and its history are preserved. It is not an immutable ledger: a server administrator can alter references, rewrite history, or remove objects. A commit identifier alone does not prove the factual truth, independent time, real-world authorship, or legitimacy of the represented claim. This version does not add independent signatures, external timestamp witnesses, replication guarantees, or content encryption at rest. The record-history view lists the latest 100 revisions; a separate owner-authorized Git bundle export contains the complete history reachable from the current main reference.

The `withdrawn` status changes the current account of a record. It does not delete content from earlier commits. Private application access does not remove access held by server operators or anyone with a repository copy. A current-snapshot JSON export excludes history and credentials. A Git bundle includes prior content, including withdrawn or subsequently edited content, but excludes account credentials, sessions, and application access configuration. Neither export is a complete service backup. Custodians need arrangements covering repositories, the account/access database, secrets, backups, and copies before relying on the service for consequential retention. Selective erasure, legal holds, retention schedules, and controlled disclosure are unresolved application work, not functions implied by using Git.

The initial owner-only model permits personal recording and revision. It does not establish third-party challenge intake, reviewer independence, participant notification, appeal, remedy, or completed practical repair. Broad sharing would require an explicit access model and protection review, including historical content and export permissions.

Future changes to schema meanings, role restrictions, access scope, and retention behavior need recorded migration and design decisions. Feedback from actual use can propose draft revisions; application behavior does not silently redefine the drafts. Automated checks establish only their tested behavior and environment. They do not establish user comprehension, evidential sufficiency, or institutional performance.

## Affected artifacts

The new `webapp/` application, its design and operating instructions, application tests, repository navigation, changelog, and open-question register are affected. The normative text and maturity of the nine Working Drafts remain unchanged by this implementation decision.

# Changelog

## 0.15 — Hosted source repository configuration

- Configured the four user-supplied GitHub remotes, replaced local submodule URLs with hosted URLs, and updated repository READMEs and the recursive-clone/development guide.
- Selected the user's Windows SSH identity in repository-local configuration; private key contents remain outside source control. Existing host verification remains enabled.
- Integrated academia's existing remote contributor-attribution commit into the migrated MIT repository, preserving both development histories and the earlier frozen source snapshots.
- Retained the application-to-infrastructure-to-programme publication order and separated source publication from private runtime records. Repository governance, CI and release policies remain open under Q-44.

## 0.14 — Independent repositories, nested submodules and MIT licensing

- Established the programme root, infrastructure and two applications as four independently committed Git repositories. Infrastructure pins both application repositories as submodules; the programme pins infrastructure. The enclosing repository's index and references remain unchanged.
- Preserved academia's original source history as actual ancestry and retained its original history bundle. New relocation, documentation and licensing changes are subsequent commits; private record repositories remain outside source history.
- Added boundary-local ignore rules, byte-preserving attributes, comprehensive repository READMEs, a submodule development/publication guide and a read-only structure checker. Local relative URLs support the present workspace; hosted remotes remain deferred at the user's direction.
- Applied the requested MIT grant to project-controlled content, added contributor attribution at each repository boundary and in distributable Python packages, aligned package metadata, and retained third-party and historical notices with explicit scope notes.
- Recorded ADR-0010, Q-44 and independent repository/recursive-clone verification. No application record schema, record bytes, account data or accepted record-history heads were changed.

## 0.13 — Application infrastructure consolidation

- Accepted ADR-0009 and established common packages plus independently runnable generalized and academia applications under `applicative_infrastructure/`; programme publications remain canonical at the root.
- Relocated the generalized application and record-protocol package, extracted the unchanged Git store into its own tested package, and introduced explicit runtime paths and separate reproducible Python environments.
- Imported academia source with its verified complete Git history bundle, per-file provenance, original README and licensing notices. Preserved native GRRP semantics and kept the original checkout untouched; semantic adapters remain deferred.
- Preserved the existing four accounts, three project repositories, record files and academia key material. Created private pre-migration backups, verified SQLite data and record inventories, and retained the generalized legacy project format without automatic conversion.
- Added setup and launch tools for separate ports 8000/8001, updated active documentation links and recorded migration verification, independent audit and remaining deployment questions. Old generalized source directories are retired into ignored operator backups after verification.
- Verified 128 generalized/common tests including browser journeys; 208 academia Python tests with one platform-specific skip; 91 academia TypeScript tests; production build; isolated deep-path record creation; and both live entry points. Inherited academia npm dependency advisories are recorded for separate maintenance.

## 0.12 — Experimental record graphs, atomic transactions and retained modules

- Reviewed the current GR manuscripts and the earlier academia application's actual source. Recorded current-manuscript precedence, conceptual mappings, implementation gaps and the separate academia migration boundary; no academia source was copied or modified.
- Added ADR-0008 and a detailed design separating semantic record graphs, Git revision history and local display state. Relations remain records with role-indexed incidences; repeated, unary, cyclic, parallel and higher-order participation is supported within the experimental profile.
- Developed bounded GR-120/130/140/160 Working Drafts for the identified requirements, preserving existing outlines and the coordinated nine-document TeX edition 0.3. Added implementable experimental GR-SPEC-120/130; no Standard adoption or independent interoperability certification is asserted.
- Added an application-independent Python protocol package, packaged JSON Schema, fixtures and CLI validation. Added overlapping roles, versioned modules, whole-candidate validation, RFC 8785 request digests, explicit legacy migration and preservation of opaque optional content.
- Implemented atomic Git transactions for records, modules, receipts and verified file bytes; durable retry recognition, conditional publication, pinned historical reads, bounded batch loading, supported-material ZIP exports and streamed files/history bundles.
- Added interactive graph/list views, visual selection and connection, multi-participant Relation forms, atomic connected-record creation, conditional notes/files/history panels, local layout, visible filters, historical project views and draft-preserving conflict review.
- Extended protocol, real-Git, API and browser checks. Recorded the current limits and unresolved cross-project references, governed erasure, independent custody, accessibility evaluation and academia identity/record preservation work.

## 0.11 — Experimental Git-backed web workspace

- Added a runnable Flask/Waitress web application with registration, login, logout, private projects and a responsive recording interface.
- Added one bare Git repository per project, structured JSON records, attributed revisions with reasons, historical views, current-snapshot exports and full-history bundles. Compare-and-swap publication prevents silent concurrent overwrites.
- Kept accounts, password hashes, expiring sessions and project authorization in SQLite outside project history; added CSRF, browser-security headers, authentication rate limits and input validation.
- Added API, real-Git and browser verification, reproducible runtime dependencies and local operation instructions.
- Recorded the implementation decision in ADR-0007, exact draft traceability and explicit feature/conformance limits in the application design, and Q-34–37 for collaboration, retention/recovery, richer epistemics and actual contestation. No Standard adoption or whole-practice conformance is asserted.

## 0.10 — Substantive validation and traced Working Draft 0.3

- Preserved the 0.2 source, tool and review baseline with hashes and copied its generated PDFs/reports before revision.
- Reviewed all 753 baseline requirement identifiers through 116 explicit topical groups, with actual foundation-manuscript passages, operational narrowing, findings and unresolved questions. Added a reproducible per-ID review matrix without treating editorial coverage as conformance.
- Added new P0/v2 and P1/v2 fixtures while preserving the original examples. Executed 18 ballot arithmetic/input cases; all matched the authored expectations. Added 208 P1 assessment rows that separate 54 selected documentary components from 154 whole-requirement coverage results.
- Prepared coordinated Working Draft 0.3: clarified invalid/unresolved ballot accounting, general versus conditional metamodel assessment, effect modality versus dispute disposition, and GR-210's vocabulary dependency. Added conditional recirculation requirement R110-112 with source rationale, semantic information and a discriminating assessment route.
- Retained the unresolved P1 retention arrangement and unperformed institutional actions explicitly; corrected a premature failure classification when its decision trigger was not established. Added readiness/burden evidence gaps and future measurement design; no participant effort, real procedural success or adoption is invented.
- Added ADR-0006, revision regression cases, Q-31–33, validation tooling, baseline-selectable change comparison, archive-integrity checks, and current publication/navigation updates.

## 0.9 — Expanded formal Working Drafts and ISO/IEEE/UN source models

- Expanded all nine governance and GR-100/110/200/210 documents to coordinated Working Draft 0.2, with detailed procedures, definitions, application conditions, semantic schedules, assessment obligations and worked examples.
- Added formal Foreword/Introduction, Scope, Normative references, numbered Terms and definitions, decimal clause hierarchy, captioned tables, labelled normative/informative annexes and bibliographies using a shared restrained TeX layout.
- Compared verified ISO publication models, accessible IEEE rules, ITU drafting guidance and UN statistical quality guidance. Recorded actual source access, unsuccessful downloads, and the successfully cached UN NQAF manual without importing external institutional authority or claiming compatibility.
- Preserved the complete 0.1 source/build-tool snapshot with hashes, retained all original requirement identifiers and added a reproducible text change register. Updated dated technical dependencies to 0.2; corrected the ethics acknowledgement/deadline ambiguity through explicit revision.
- Expanded independent-review capacity gates, ethics case handling and reconsideration, information/evidence methods, generative trajectory recording, and correction/maintenance procedures. Internal conduct criteria and proportionality remain proposed choices.
- Extended artifact checks for formal structure, local cross-references, archive integrity and historical ID retention. Added a new review record, ADR-0005 and Q-27–30; preserved earlier review and fixture scope as historical evidence.
- Updated publication navigation, development status, source register and project index. No adoption, appointment, licensing grant, real pilot success or international partnership is represented by these changes.

## 0.8 — Initial substantive working drafts and TeX publication package

- Developed five governance Working Drafts: Programme Charter, Directives Parts 1 and 2, GR/TC 1 Charter, and ethics/conflict/contribution-rights policy. They cover founding authority, appointments, review, consensus/ballots, appeals, maintenance, rights, records and separate publication-class routes.
- Developed preparatory GR-100, GR-110, GR-200 and GR-210 texts with stable requirement identifiers, bounded conformance subjects, source rationale, assessment methods and unresolved questions. No governance, committee, work item, Specification or Standard is represented as adopted by these edits.
- Separated claimed occurrence/modality from epistemic mode; required material maintenance/loss recording; made GR-200 package dependencies explicit; distinguished partial documentary assessment from observed institutional performance.
- Added shared TeX presentation, a nine-document manifest, WSL/LuaLaTeX build tooling with hashes and reports, PDF requirement-coverage checks and publication navigation. Generated build artifacts are ignored in source control.
- Added synthetic governance and contested-decision fixtures, an unsigned founding-agreement template, a requirement-assessment template, and expanded NWIP/comment/disposition templates without inventing operational evidence or signatures.
- Added Proposed ADR-0004, cross-document review evidence and Q-21–26. Preserved earlier Markdown outlines as historical planning material linked to the developed drafts.
- Updated programme navigation, Programme Brief development status and the project index. Governance adoption, actual appointments and rights terms, domain review, accessibility targets and live pilot evidence remain open.

## 0.7 — Comprehensive epistemic and practical development planning

- Reframed the programme brief and README around the confirmed motivation: symbolic crisis, warranted recognition, contestation, revision and practical action.
- Added a PROVISIONAL comprehensive plan connecting objectives, the existing publication portfolio, substantive domain work, dependencies, conformance subjects, milestone evidence and an immediate work queue.
- Added a comparative study of ISO/IEC, IEEE and WTO standards-development approaches, with official sources, access/edition limits and explicit local adaptation proposals.
- Added a cross-domain programme for epistemology, archives, jurisprudence, ethics, institutional governance, public policy, political economy and implementation, preserving their different sources of authority.
- Added a draft GR/NWIP-001, governance/pilot planning, protocol/Specification planning, and a verified candidate technical standards landscape. These do not adopt Standards or approve work items.
- Clarified that protocols describe behavior within Specifications or Standards and are not a separate publication authority class.
- Indexed the seven local foundation manuscripts and added source-to-operational-proposal traceability with unresolved terminology, version and formal-scope differences preserved.
- Added Proposed ADR-0003 with alternatives and rationale; retained accepted ADR-0002 as the governing Standards-first decision.
- Marked the earlier preliminary plan as historical and preserved its text, identifying its superseded specification-first wording.
- Added prioritized open questions and updated publication taxonomy, Specification-track guidance and project navigation.
- Added explicit publication-class and non-adoption labels to the existing standards-track and Specification placeholders without changing their identifiers.


## 0.6 — Standards-first, specifications-on-demand

- Added ADR-0002 establishing Standards as the default starting point.
- Clarified that Specifications are created when Standard requirements reveal reusable technical or infrastructure needs.
- Added the co-evolution loop: Standard WD → Specification → Pilot → Standard revision.
- Updated AGENTS, publication taxonomy, programme brief, preliminary plan, and README.


## 0.5 — Publication-class separation

- Added a formal taxonomy distinguishing Specifications, Standards, and International Political / Policy Proposals.
- Added `docs/framework/publication_taxonomy.md`.
- Added a new `specifications/` track with preliminary GR-SPEC placeholders.
- Added GR-IPP / GR-IIP identifiers for international policy and institutional proposals.
- Updated `AGENTS.md`, `PROGRAMME_BRIEF.md`, README, planning, open questions, and policy-proposal guidance.
- Clarified that technical precision, standards adoption, and political/institutional recommendation are different forms of authority.


## 0.4 — Multilayer architecture and policy-proposal track

- Expanded the programme beyond ontology to include epistemics, normativity, jurisprudence, governance, political economy, and implementation.
- Added `docs/framework/multilayer_architecture.md`.
- Added epistemic and jurisprudential background documents.
- Added provisional GR-200/210 epistemic standards, GR-300/310 jurisprudential standards, and GR-400 governance standard.
- Added a separate `policy_proposals/` publication track with an international-organizations folder and policy-brief template.
- Updated `PROGRAMME_BRIEF.md`, `AGENTS.md`, planning, README, and open questions to maintain the distinction between standards and policy proposals.

## 0.3 — Programme-level Why / What / How brief

- Added root-level `PROGRAMME_BRIEF.md` as the first-read orientation for contributors and reviewers.
- Added explicit Why, What, and How sections covering historical motivation, AI-era context, GR foundations, standards scope, document family, development lifecycle, source discipline, traceability, and pilot-driven implementation.
- Updated `README.md` to point new readers to the programme brief.
- Updated `AGENTS.md` so agents must consult the programme brief before substantive development.
- Linked the preliminary plan to the new brief.


## 0.2 — Foundation corpus and contextual background

- Added `foundation_manuscripts/` with a source-corpus README and manifest template.
- Added `docs/background/GR_background.md`.
- Added `docs/background/era_background.md`.
- Expanded `AGENTS.md` with foundation-manuscript consultation, source hierarchy, conflict handling, historical consistency, and terminology rules.
- Updated preliminary planning and open questions for manuscript-to-standard traceability.

## 0.1 — Initial project scaffold

- Created governance, standards, manual, technical-report, template, schema, example, and decision directories.
- Added preliminary background and planning documents.
- Added `AGENTS.md` for AI-assisted vibe development.
- Added provisional publication family and lifecycle.
- Added starter templates for new work items, comments, and architecture decisions.

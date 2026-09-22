# Initial working-draft review

**Document class:** Informative editorial and artifact review record  
**Status:** PREPARATORY; version 0.1; 2026-09-22  
**Package:** [Archived nine Working Drafts](../../publications/archive/0.1/README.md), draft version 0.1  
**Review relationship:** AI-assisted authoring and cross-review within the same development session. This is not independent stakeholder review, a formal public review, or a competent legal/ethical determination.

## 1. Scope and evidence

The review examines conceptual distinctions, cross-document requirements, explicit authority, source traceability, synthetic counterexamples, and generated artifacts. It draws on the Programme Brief, repository instructions, seven indexed foundation manuscripts and the prior standards-development comparison. TeX sources contain passage-level source rationale; the manuscript map preserves unresolved differences. External procedures inform comparison, not a claim of ISO/IEEE affiliation or procedural conformity.

Different drafting agents reviewed the governance documents and the recording/epistemic documents; the primary author integrated findings. The P1 example received a separate cross-review. These are useful editorial checks within one AI-assisted process and do not supply the expertise, constituency independence or participation evidence required for actual programme adoption.

## 2. Findings and dispositions

| ID | Finding | Implemented disposition | Evidence |
|---|---|---|---|
| RV-01 | Draft R110-012 and some vocabulary metadata mixed occurrence/modality with observational/inferential basis. | Separate them. A realized past event can be inferred retrospectively. | GR-100 Event/Process; E200-005; R110-012; P1-C. |
| RV-02 | “Loss shall be recordable” tested capability but allowed known material loss to be omitted. | Require actual recording or an explicit collection/protection limitation. | R110-017; P1-I. |
| RV-03 | Claim-level GR-200 references could hide its package-scope and assessment obligations. | Identify a bounded material-claim package and require the corresponding dependency results. | R110-005; C210-030. |
| RV-04 | Package/case review and practice/procedure assessment had ambiguous applicability. | Treat documentary-only assessment as partial in this edition; unobserved performance remains not assessed. Condition multi-case rehearsals on procedure assessment. | GR-110 conformance section; C210-030–032; Q-24. |
| RV-05 | D2 amendment language would constrain ordinary draft editing as though every edition were adopted. | Apply formal amendment/revision to adopted normative editions, draft-stage rules to drafts and declared maintenance to released Specifications. | D2-045. |
| RV-06 | D1 referenced separate participation/records policies that did not exist. | Cite the Charter, named ethics/rights policy and exact records clauses. | D1-001/008/048 and associated prose. |
| RV-07 | Specification and policy release routes were referenced but underspecified. | Require class-specific review plans, release authority, evidence, objections and appeals; preserve explicit promotion and mandate-specific policy decisions. | D1-056–058; CH-013; TC-022. |
| RV-08 | Charter wording implied every maintenance decision required a substantive ballot. | Refer to applicable ballot/review rules and distinguish substantive changes, corrections and confirmation. | CH-038; D1-029/045/047. |
| RV-09 | Returned-ballot quorum alone could allow one approval plus abstentions. | Require at least three substantive votes in addition to quorum and the two-thirds substantive approval threshold. | D1-031; P0 arithmetic cases. This threshold remains a proposal. |
| RV-10 | Some documents lacked editor/source metadata and governance titles introduced unnecessary codes. | Mark responsibility UNASSIGNED and canonical source; use descriptive governance identifiers and existing GR/TC 1 designation. | Source front matter; manifest build IDs. |
| RV-11 | P1 risked deriving absence from a partial packet, implying an unavailable log had been inspected, overstating adjustment evidence as calibration quality, and treating scheduling as completed reassessment. | Preserve source availability, narrower source content, conditional conclusions and pending actions; include pre-submission antecedents in temporal scope. | P1 inventory, evidence table, procedure and summary. |
| RV-12 | Source prose implied a chronological relationship between two Chapter 3 files. | Describe separately supplied variants and retain unresolved chronology. | GR-110 source rationale; Q-08. |
| RV-13 | The initial GR-110 PDF had a long-path overflow. | Use a breakable source path and recompile the revised package. | Build report and PDF inspection. |

No finding resolves the Property/Attribute relationship, full metamodel interpretation, universal generativity measurement, legal jurisdiction, rights licence selection or institutional legitimacy.

## 3. Artifact checks and reproducibility

The machine-generated [build report](../../build/archive/0.1/build-report.json) is the authoritative record of compiler outcomes, source/preamble/PDF hashes and detected warnings for the historical 0.1 full build. The [check report](../../build/archive/0.1/check-report.json) records source metadata, unique/resolved requirement identifiers, build freshness and extraction of all source requirement IDs from the PDFs. These historical reports were copied before the 0.2 revision. Current build results belong to the expanded review; they do not replace this record.

The [archived build guide](../../publications/archive/0.1/README.md) explains reproduction of this edition. Compilation uses LuaLaTeX through `latexmk` under WSL with shell escape disabled. No external source material or package dependencies are fetched by these scripts.

The checks discriminate lost identifiers, stale PDFs and unresolved references. They do not prove that every rendered sentence is correct or assess the substantive warrant of a claim. Visual inspection samples title pages, body text and tables; it is not a full accessibility audit.

**Completed artifact results (historical 0.1):** all nine documents compiled; all 318 requirement identifiers were found in the historical 84-page PDF package; source, preamble and PDF hashes matched; no duplicate/unresolved requirement identifiers, overfull boxes, missing glyphs or detected unresolved LaTeX references remained. Local Markdown links resolved and the project index matched all 87 source/corpus files. These are artifact checks, not a D2-wide conformance determination.

| Draft build identifier | Requirements | PDF pages |
|---|---:|---:|
| GSP-Charter | 41 | 9 |
| GSP-Directives-1 | 58 | 13 |
| GSP-Directives-2 | 45 | 8 |
| GR-TC1-Charter | 30 | 7 |
| GSP-Ethics-IP | 40 | 8 |
| GR-100 | 12 | 13 |
| GR-110 | 34 | 8 |
| GR-200 | 26 | 11 |
| GR-210 | 32 | 7 |

Toolchain recorded by the final build: Python 3.12.3, latexmk 4.83 and LuaHBTeX 1.17.0 (TeX Live 2023/Debian), running in Ubuntu-24.04 under WSL. The machine report supplies exact versions and hashes.

Visual review covered all nine title pages, PDF page 4 of each document, and assessment/source tables on PDF pages 8 (GR-110), 9 (GR-200) and 7 (GR-210). Additional inspection covered GR-100 role definitions on PDF page 8 and the D1 defaults table on PDF page 11. The samples showed readable layout without observed clipping or overlap. Title-word hyphenation and description-list warnings were corrected in the shared preamble; narrow table columns were changed to ragged-right alignment. The affected samples were inspected again after recompilation. One nonblocking underfull-line spacing advisory remains in the Charter; the build report records it separately from errors and LaTeX/package warnings. Final render samples and contact sheets are in `build/review/`; page numbers here count the title page as PDF page 1. No screen-reader, tagged-PDF or affected-user comprehension assessment was performed.


## 4. Synthetic evidence and remaining work

[P0](../../examples/p0-governance-rehearsal.md) checks explicit ballot arithmetic and illustrates comment/disposition logic. It does not perform a real public review, election, recusal, appeal or founding agreement. [P1](../../examples/p1-contested-decision.md) supplies attributed claims and discriminating defects for review; it does not establish that procedural actions occurred, controls worked, users understood the presentation or harm was repaired.

Use the [assessment template](../../templates/requirement_assessment.md) to record per-requirement evidence in actual rehearsals. No unqualified conformance claim is made for P0, P1, the programme or an institution. Documentary inspection and arithmetic are only parts of a future assessment.

Before governance adoption and technical initiation, remaining work includes actual founding authority, accepted appointments, neutral-review capacity, participation/representation review, selected rights terms and a real governance rehearsal. Before live institutional claims, work includes contextual legal and ethical review, affected-user participation, disclosure/retention authority, resource and burden measurement, observed procedure tests, accessibility/comprehension evaluation, and a decision on bounded conformance profiles.

M1 and later milestones remain open. Compiled drafts are the first reviewable development output, not evidence that the programme's institutional infrastructure is already operating.

# Validation-revised working-draft package

**Document class:** Informative publication and build guide  
**Status:** PREPARATORY; version 0.3; 2026-09-22. All nine documents are working drafts, not adopted governance or Standards. Formal work-item approval remains pending.

The package develops the [comprehensive plan](../docs/planning/comprehensive_development_plan.md) into detailed requirements, responsibilities, procedures, semantic and assessment schedules, and worked examples. Read the [Programme Brief](../PROGRAMME_BRIEF.md) for purpose and [ADR-0006](../decisions/ADR-0006-validation-and-assessment-boundaries.md) for this validation-driven revision. The [external model comparison](../docs/references/drafting_models.md) explains the distinct roles of ISO, IEEE, ITU and UN guidance.

Every document has formal front matter, numbered scope/references/definitions, technical or institutional clauses, explicitly normative/informative annexes, and a bibliography. The [substantive validation dossier](../docs/reviews/validation_0.2/README.md) records review of all 753 baseline IDs, bounded P0/P1 assessments, supported corrections and remaining evidence gaps. The [0.1](archive/0.1/README.md) and [0.2](archive/0.2/README.md) archives preserve earlier editions; the [0.2 to 0.3 change register](../docs/reviews/requirement_changes_0.2_to_0.3.csv) identifies retained, revised and new provisions.

| Draft | Editable source | Compiled review copy | Pages |
|---|---|---|---:|
| Standards Programme Charter | [TeX](../governance/standards_programme_charter.tex) | [PDF](../build/publications/GSP-Charter.pdf) | 19 |
| Directives Part 1 — Development and Maintenance | [TeX](../governance/directives_part_1.tex) | [PDF](../build/publications/GSP-Directives-1.pdf) | 28 |
| Directives Part 2 — Structure and Drafting Rules | [TeX](../governance/directives_part_2.tex) | [PDF](../build/publications/GSP-Directives-2.pdf) | 26 |
| GR/TC 1 Charter | [TeX](../governance/technical_committee_charter.tex) | [PDF](../build/publications/GR-TC1-Charter.pdf) | 17 |
| Ethics, Conflict-of-Interest, and Contribution Rights | [TeX](../governance/ethics_ip_policy.tex) | [PDF](../build/publications/GSP-Ethics-IP.pdf) | 22 |
| GR-100 — Foundations and Vocabulary | [TeX](../standards/GR-100-foundations-vocabulary/GR-100.tex) | [PDF](../build/publications/GR-100.pdf) | 30 |
| GR-110 — Generativity Recording Requirements | [TeX](../standards/GR-110-recording-requirements/GR-110.tex) | [PDF](../build/publications/GR-110.pdf) | 28 |
| GR-200 — Epistemic Recording Framework | [TeX](../standards/GR-200-epistemic-recording/GR-200.tex) | [PDF](../build/publications/GR-200.pdf) | 32 |
| GR-210 — Evidence, Interpretation, and Contestation | [TeX](../standards/GR-210-evidence-interpretation/GR-210.tex) | [PDF](../build/publications/GR-210.pdf) | 24 |

The TeX files are the substantive authoring sources for these editions. Earlier Markdown outlines remain preserved with links to their developed drafts. The PDF filenames are build identifiers, not new adopted publication codes. Generated files live in ignored `build/`; a fresh checkout needs compilation before the PDF links work.

The graph pilot additionally develops bounded preparatory Markdown Working Drafts [GR-120](../standards/GR-120-information-model/GR-120.md), [GR-130](../standards/GR-130-interchange/GR-130.md), [GR-140](../standards/GR-140-presentation/GR-140.md) and [GR-160](../standards/GR-160-preservation-access/GR-160.md), plus experimental [GR-SPEC-120](../specifications/GR-SPEC-120-information-model/GR-SPEC-120.md) and [GR-SPEC-130](../specifications/GR-SPEC-130-interchange/GR-SPEC-130.md). They are separate from this nine-document build and have not been promoted to adopted Standards or included in its 0.3 PDF assessment.

Useful accompanying material:

- [P1/v2 contested-decision dossier](../examples/validation_0.2/p1-contested-decision.md): explicit claims, grounds, issue/action tracks, view limits and partial component results.
- [Substantive review and assessment matrix](../docs/reviews/validation_0.2/README.md); historical [0.2 review](../docs/reviews/expanded_working_draft_review.md) and [0.1 review](../docs/reviews/initial_working_draft_review.md).
- [P0/v2 governance rehearsal](../examples/validation_0.2/p0-governance-rehearsal.md): 18 executed arithmetic cases and detailed synthetic procedural traces.
- [Revision regression cases](../examples/validation_0.2/revision_regressions.md) and [readiness/burden evidence](../docs/reviews/validation_0.2/readiness_and_burden.md).
- [NWIP template](../templates/new_work_item_proposal.md), [comment/disposition template](../templates/public_comment.md), and [founding agreement template](../governance/founding_agreement_template.md).
- [Requirement assessment template](../templates/requirement_assessment.md): individual applicability, evidence, results and counterexamples.
- [Open questions](../docs/planning/open_questions.md), including actual authority, participation, rights terms, profiles and pilot capacity.

Original P0/P1 and the initial templates retain their historical 0.1 basis. New P0/v2 and P1/v2 assess the fixed 0.2 baseline; the revision cases examine the corrected 0.3 distinctions. None establishes full institutional conformance. Current normative schedules identify the information needed for an assessment of this edition.

## Build

Requirements: Python 3.10+, `latexmk`, LuaLaTeX, and the TeX packages/fonts named in [the preamble](preamble.tex). These are available in the current `Ubuntu-24.04` WSL environment. No packages or external sources are downloaded by the build script.

From this directory's repository root, in PowerShell:

```powershell
python tools/build_publications.py --wsl Ubuntu-24.04
```

Or inside WSL, from the repository root:

```sh
python3 tools/build_publications.py
```

To rebuild one document, append `--document GR-110` (repeatable). Full builds write `build/publications/build-report.json`; selected builds write `build-report-selected.json`. The reports identify source/preamble/PDF hashes, compiler outcomes, LaTeX/package warnings and nonblocking underfull-line spacing advisories. An old successful PDF can remain after a later failed build: check the current report and hashes before using it.

For source/PDF requirement coverage and report freshness, from Windows with `pypdf` installed:

```powershell
python tools/check_publications.py
```

The checker also examines formal opening clauses, declared front matter, annex roles, local references, captions, archive hashes and retention of historical requirement IDs. To regenerate the text change register:

```powershell
python tools/compare_publication_requirements.py --baseline 0.2
```

To check the fixed 0.2 review matrix, fixture identifiers/locators and finite ballot expectations, run `python tools/check_validation_package.py`. This does not automatically evaluate manuscript interpretation, factual warrant or procedural performance.

The build and static checks do not assess factual truth, institutional performance, legal status, user comprehension or PDF accessibility. Accessible publication targets and a tested alternative reading representation remain work before formal release. TeX sources and Markdown examples are available for review, but their availability alone establishes no accessibility conformance.

## Suggested reading sequence

Read the Charter and Directives first, then GR-100 and GR-200, then GR-110/210 alongside P1. GR-110 and GR-210 depend on the declared GR-100/200 draft editions; GR-200 does not depend on GR-110. GR-120/130 and the Specification placeholders remain future work driven by the requirements and pilot findings. Jurisprudential and policy publications retain their separate scope and authority.

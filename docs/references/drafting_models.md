# External drafting and institutional models

**Class:** Informative comparative editorial study  
**Status:** PROVISIONAL; supports Working Draft package 0.2  
**Sources checked:** 2026-09-22

The UN system does produce technical standards, but it does not supply one universal standard-writing format. ITU is a UN specialized agency whose ITU-T Recommendations address technical standardization. UN/CEFACT publishes technical specifications and other trade-facilitation instruments. A UN statistical quality manual serves a different purpose. The programme selects practices by function and retains its own publication classes, authority and terminology. [ITU institutional description](https://www.itu.int/en/ITU-D/Conferences/TDAG/Pages/ITU-D-Delegate-Guide-WhatisITU.aspx), [ITU-T Recommendations](https://www.itu.int/en/ITU-T/publications/Pages/recs.aspx), [UN/CEFACT catalogue](https://unece.org/trade/uncefact/mainstandards).

This study supplements the earlier [development-process comparison](../planning/standards_development_comparison.md). It does not replace the GR foundation corpus with an external ontology or establish compatibility with any external standard. The proposed local decisions are recorded in [ADR-0005](../../decisions/ADR-0005-expanded-drafts-and-source-models.md).

## 1. Verified sources and access limits

| Source and edition | What was inspected | Use in this revision |
|---|---|---|
| [ISO Model International Standard](https://www.iso.org/publication/PUB100407.html), third edition, 2023 | Official metadata and the 27-page annotated model PDF through the browser; direct download was refused | Document architecture and differentiated normative/informative material |
| [ISO House Style](https://www.iso.org/ISO-house-style.html), web page checked on the date above | Official HTML guidance | Consistent terminology, formal hierarchy, concise impersonal provisions and readable production |
| [IEEE SA Operations Manual, Clause 6](https://standards.ieee.org/about/policies/opman/sect6/), current official HTML checked on the date above | Published provisions on draft status, standard structure, references and verbal forms | Explicit status, normative/informative separation and reference discipline |
| [ITU Author's Guide for drafting ITU-T Recommendations](https://www.itu.int/oth/T0A0F000004/en), June 2023 | Official catalogue and 30-page guide PDF through the browser; direct download returned HTML | Technical-document structure, definitions, references, annex roles and editorial review |
| [ITU-T A.1](https://www.itu.int/itu-t/recommendations/rec.aspx?rec=a.1), September 2019 | Official catalogue identifies this edition as in force; full operative text was not inspected in this revision | A verified working-methods reference for further institutional study; no detailed rule imported |
| [UN/CEFACT Core Components Technical Specification](https://unece.org/trade/documents/2009/09/uncefact-core-components-technical-specification-version-30), version 3.0, 29 September 2009 | Official metadata; linked full PDF was inaccessible | Evidence of a UN-system technical Specification class; detailed comparison remains deferred |
| [United Nations National Quality Assurance Frameworks Manual for Official Statistics](https://desapublications.un.org/publications/united-nations-quality-assurance-frameworks-manual), 2019 | Official metadata and full PDF; a local copy was downloaded from the UN Statistics Division | Institutional, process and output quality as distinct review concerns |
| [UNESCO Recommendation on the Ethics of Artificial Intelligence](https://www.unesco.org/en/legal-affairs/recommendation-ethics-artificial-intelligence), adopted 23 November 2021 | Official text and adoption metadata | Ethical impact, participation and oversight questions for AI-mediated programme work; not a universal GR technical standard |

The [machine-readable source register](external_source_register.json) distinguishes browser inspection, catalogue-only verification and successful local retrieval. Only the UN NQAF PDF was successfully cached locally in this revision. The cache is in ignored `build/reference_sources/`; it is not a redistributed programme publication.

The official [IEEE drafting page](https://standards.ieee.org/develop/drafting-standard/) links its Style Manual, editorial checklist and 2026 Word template. Access to the manual/checklist was refused with HTTP 418; the template download was refused with HTTP 403. These files were not read or adopted. The accessible Operations Manual is the IEEE source actually used. The [ISO directives page](https://www.iso.org/directives-and-policies.html) identifies Part 2 resources, including an amendment; the complete current amended text was not obtained. The model and house-style guidance do not substitute for a claim of full Directives compliance.

## 2. Differences that matter

### Technical structure

The ISO model illustrates a title page, contents, Foreword, Introduction, Scope, Normative references, Terms and definitions, technical clauses, labelled annexes and a bibliography. Its rice example is instructional; its commodity requirements have no relevance to GR conformance. [Annotated model PDF](https://www.iso.org/iso/model_document-rice_model.pdf).

The ITU guide also separates scope, references, definitions and technical material, but distinguishes integral annexes from supplementary appendices. It is itself author guidance, not an ITU-T Recommendation. The programme uses one alphabetic annex sequence with an explicit normative or informative label. This is a deliberate local choice, rather than an assertion that ISO and ITU structures are identical. [ITU guide PDF](https://www.itu.int/dms_pub/itu-t/oth/0a/0f/T0A0F0000040005PDFE.pdf).

IEEE's accessible rules distinguish normative from informative content, identify the role of references and distinguish mandatory requirements, recommendations, permissions and capabilities. Its own copyright, distribution, committee and approval rules belong to IEEE. The GR programme does not copy those institutional authorities or assert compliance with them. [Operations Manual, 6.4](https://standards.ieee.org/about/policies/opman/sect6/).

### Institutional quality

The UN NQAF manual addresses official statistics through recommendations, a framework and implementation guidance. Its treatment of institutional environment, statistical processes and outputs makes it useful for asking whether competent responsibility, reliable methods and usable results are all present. It does not define truth for arbitrary factual claims, adjudicate legal disputes or supply a general-purpose provenance schema. [UN Statistics Division PDF](https://unstats.un.org/unsd/methodology/dataquality/references/1902216-UNNQAFManual-WEB.pdf).

The local adaptation is to assess records and the practices that produce, review and maintain them separately. A well-formed package does not prove that a review was independent, that a notification reached its audience or that a remedy worked. Those are programme design conclusions, grounded also in the GR sources and programme objectives; they are not presented as clauses copied from the statistical manual.

### Institutional mandate and policy

UNESCO's AI ethics Recommendation is also relevant to the governance discussion. It treats ethical assessment across an AI system's life cycle and connects values, impacts and institutional action. Its domain and recommendatory form differ from a technical interchange specification. The programme uses it as an informative comparator where AI mediation is involved; the broader GR project includes many processes outside that domain. [Official Recommendation](https://www.unesco.org/en/legal-affairs/recommendation-ethics-artificial-intelligence).

The existence of UN-system instruments does not identify the appropriate partner for this programme. An ITU technical activity, a UN/CEFACT interoperability activity and a statistical capacity-building initiative have different audiences and mandates. No organization is proposed as sponsor or adopter merely because its publications provided useful examples.

Any later international policy proposal needs a separate mandate analysis, options for voluntary participation and stewardship, participation and resource estimates, risks, safeguards and an evaluation design. It could cite an adopted GR Standard, propose a bounded pilot, or recommend deferral. It would not gain technical conformance authority by adopting the appearance of a UN publication.

## 3. Local publication model implemented in version 0.2

These are independent programme proposals, implemented in the shared TeX presentation and Directives Part 2:

1. **Front matter:** publication identity, maturity, version and date; contents; Foreword for responsibility and status; Introduction for motivation and relationships.
2. **Numbered clauses:** 1 Scope, 2 Normative references, 3 Terms and definitions, followed by the document's substantive clauses. Parent clauses with subclauses avoid unnumbered hanging text.
3. **Requirements:** stable identifiers retained across rearrangement; the responsible subject, triggering condition, action and evidence expectation are explicit. Editorial numbering and requirement identity remain separate.
4. **Normative schedules:** application conditions and assessment records belong in explicitly normative annexes. Every requirement has an assessment route; an annex's status does not silently turn its examples into obligations.
5. **Informative material:** worked examples, conceptual source maps, external comparisons and unresolved issues remain distinguishable from conformance criteria.
6. **References:** precise draft dependencies are separated from an informative bibliography. A new dependency edition requires reassessment of affected obligations.
7. **Production:** A4, readable serif body text, decimal clauses, restrained monochrome headings, captions, Roman front matter and Arabic main pagination. Annex boundaries have page breaks; ordinary clauses flow continuously. Fonts, code layout and margins are local production choices.
8. **Revision evidence:** the 0.1 source package is preserved; a requirement change register and build hashes identify the revision. Existing IDs are not renumbered to simulate a new origin.

The revision expands procedure and semantic content, rather than imposing a target page count. A useful standard can be short. Here the earlier compression omitted much of the application detail, assessment evidence and failure handling needed to evaluate the proposals. Length is reported as a production result, not as evidence of maturity.

## 4. Remaining review

The full current ISO directives, IEEE style resources and UN/CEFACT technical text need separate access and edition review before more detailed reliance. Language, accessibility and institutional burden need testing with intended users. Governance adoption, rights terms, actual appointments and independent review remain unresolved. No external source model resolves those local decisions.

# External Standards and Specifications: Candidate Reuse Landscape

**Document class:** Informative research and planning document  
**Status:** PROVISIONAL candidate assessment; no dependency selection  
**Version:** 0.1  
**Checked on:** 2026-09-22, Asia/Tokyo (2026-09-21 UTC)

## 1. Purpose and evidence boundary

This bounded review identifies external resources worth evaluating for the [protocol and specification plan](protocol_specification_plan.md). The source descriptions below report published scope; proposed GR uses and limitations are programme planning judgments. They do not establish a mapping, conformance, interoperability, or endorsement claim.

This is the technical reuse component of the research. The [standards-development comparison](standards_development_comparison.md) covers ISO/IEC, IEEE and WTO procedural approaches; the [cross-domain programme](cross_domain_programme.md) covers normative, legal, institutional and policy sources.

Official publisher pages and freely available technical documents were checked. This is an initial scope/status review, not a clause-by-clause compliance or intellectual-property assessment. No inaccessible or paywalled normative requirements are inferred. Dates identify the editions inspected, not a guarantee that each is the newest available edition.

A reuse decision still needs a motivating GR requirement, a precise version/section, candidate mappings and counterexamples, licensing and maintenance review, implementation evidence, and an explicit decision record. External terms retain their own definitions: matching names do not establish conceptual equivalence.

## 2. Verified source register

**Structural role:** record the exact primary sources inspected and candidate programme destinations. The checked-on date above applies to every entry.

| Source and inspected edition | Official source and observed scope/status | Candidate GR destination |
|---|---|---|
| PROV-O, 30 April 2013 | [W3C Recommendation: PROV-O](https://www.w3.org/TR/2013/REC-prov-o-20130430/). OWL representation of provenance with entities, activities, agents, derivation, attribution, and qualified relations. | GR-110/120/200/210; GR-SPEC-120/200 provenance mappings |
| Web Annotation Data Model, 23 February 2017 | [W3C Recommendation: Web Annotation Data Model](https://www.w3.org/TR/2017/REC-annotation-model-20170223/). Annotation bodies and targets, including targeting parts or states of resources. | GR-140/200/210; GR-SPEC-200 annotation and evidence targeting |
| Time Ontology in OWL, 19 October 2017 | [Published 2017 OWL-Time Recommendation](https://www.w3.org/TR/2017/REC-owl-time-20171019/). Temporal instants, intervals, ordering, durations, and reference systems. | GR-100/110/120/140; GR-SPEC-120 temporal representation |
| JSON-LD 1.1, 16 July 2020 | [W3C Recommendation: JSON-LD 1.1](https://www.w3.org/TR/2020/REC-json-ld11-20200716/). JSON serialization of Linked Data, with contexts and identifier/term interpretation. | GR-130; GR-SPEC-130 candidate serialization |
| SHACL, 20 July 2017 | [W3C Recommendation: Shapes Constraint Language](https://www.w3.org/TR/2017/REC-shacl-20170720/). Validation of RDF data graphs against shapes and production of validation reports. | GR-150; validation artifacts for GR-SPEC-120/130/200 if an RDF representation is selected |
| HTTP Semantics, RFC 9110, June 2022 | [RFC Editor: RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html). IETF Standards Track, STD 97; methods, representations, request conditions, and responses. | GR-130; eventual GR-SPEC-130 HTTP binding |
| C2PA Technical Specification, version 2.4 | [C2PA: Content Credentials technical specification 2.4](https://spec.c2pa.org/specifications/specifications/2.4/specs/C2PA_Specification.html). Technical architecture for signed content provenance manifests, validation, and trust-model handling. | Optional GR-110/130/140 evidence/provenance import; GR-SPEC-130 adapter if a media pilot requires it |
| PREMIS Data Dictionary, version 3.0 | [Library of Congress: PREMIS version 3.0](https://www.loc.gov/standards/premis/v3/index.html). Official indexed overview identifies preservation Objects, Events, Rights, and Agents; full dictionary updated November 2015. | GR-160 and GR-SPEC-120/130 preservation/migration metadata mapping |

**Version observations:** the [unversioned OWL-Time publication URL](https://www.w3.org/TR/owl-time/) displayed a later Candidate Recommendation Draft dated 2022, so it is not interchangeable with the 2017 Recommendation cited above. The C2PA site exposed 2.4 alongside earlier versions; this review inspected the 2.4 technical specification without claiming programme adoption. PREMIS overview content was available through the official-domain search index; direct page opening failed, and its full dictionary/errata have not been reviewed here.

## 3. Candidate uses, boundaries, and mapping experiments

### 3.1 PROV-O

**Proposed use:** test whether shared provenance vocabulary can express capture, transformation, contribution, and revision without inventing equivalent mechanisms. Its qualified-relation patterns are relevant where a relationship itself needs descriptive detail. [Source](https://www.w3.org/TR/2013/REC-prov-o-20130430/)

**Boundary:** `prov:Entity`, `prov:Activity`, and `prov:Agent` are source terms, not declarations that GR Entity/Event/Process have identical admissibility or identity criteria. Provenance attribution does not establish the truth of attributed content or its ethical/legal legitimacy.

**Experiment:** map one observation record, its recorder, an inference, a revision, and two competing interpretations. Record unsupported GR distinctions and reverse-mapping losses. Keep any extension local and explicit until review.

### 3.2 Web Annotation Data Model

**Proposed use:** target a claim version, document passage, or evidence fragment with an attributed interpretation, objection, or reply. Inspect selectors and states for durable references to the relevant material. [Source](https://www.w3.org/TR/2017/REC-annotation-model-20170223/)

**Boundary:** an annotation model does not by itself provide an institution's review authority, required response, adjudication process, or a settled account of evidentiary weight. A reference to a changing resource needs an explicit version/selection policy. Annotation content and the occurrence it discusses remain distinguishable.

**Experiment:** transport an objection about one passage, then revise or restrict the source. Test correct targeting, accessible historical context, and informative handling of unavailable material. Evaluate network annotation protocols separately only if the pilot needs them.

### 3.3 OWL-Time

**Proposed use:** represent ordered or overlapping intervals, instants, durations, and temporal reference systems. [Source](https://www.w3.org/TR/2017/REC-owl-time-20171019/)

**Boundary:** temporal vocabulary does not select which times a GR record needs. Occurrence time, observation time, recording time, assessment time, and version/publication time remain different roles. A precise timestamp format does not warrant precision in the underlying knowledge.

**Experiment:** map an approximately dated historical account, an observation interval, a retrospective assessment, and a later correction. Test unknown bounds, inconsistent chronology, and partial order without manufacturing certainty. Record which cases require a GR profile beyond the source vocabulary.

### 3.4 JSON-LD 1.1

**Proposed use:** evaluate a graph-oriented interchange option with explicit terms and identifiers that can be serialized as JSON. Contexts define how compact terms are interpreted. [Source](https://www.w3.org/TR/2020/REC-json-ld11-20200716/)

**Boundary:** serialization does not supply GR semantics, access control, evidence assessment, validation rules, or preservation policy. Familiar JSON appearance does not eliminate the need to test linked-data processing and context management.

**Experiment:** exchange the minimum pilot package between two implementations; preserve terms, identifiers, language information, and defined extensions. Assess context versioning, offline interpretation, dependency availability, and loss on conversion. Compare total implementation burden with a simpler documented interchange before selection.

### 3.5 SHACL

**Proposed use:** express selected structural constraints and produce inspectable validation reports if an RDF representation is adopted. The source distinguishes validation failure from reported constraint results. [Source](https://www.w3.org/TR/2017/REC-shacl-20170720/)

**Boundary:** passing a shapes check does not establish that an event occurred, evidence is adequate, a procedure was fair, access was authorized, or a presentation is intelligible. Cross-system behavior and institutional practice need additional evaluation.

**Experiment:** test required attribution, allowed reference patterns, and version links against positive and negative fixtures. Keep input defects, processor failure, unsupported validation features, and substantive disagreements distinct in the GR conformance report. Document any constraints checked outside SHACL.

### 3.6 HTTP Semantics

**Proposed use:** if a network binding becomes necessary, reuse method semantics, status codes, validators/preconditions, and the distinction between idempotent and non-idempotent operations. [Source, especially sections 9 and 13](https://www.rfc-editor.org/rfc/rfc9110.html)

**Boundary:** HTTP does not define the GR review procedure, application authorization policy, record meaning, synchronization strategy, or safe retries for an arbitrary application operation. Successful delivery is distinct from successful application processing and substantive acceptance.

**Experiment:** after batch exchange works, test interrupted submission, repeated delivery, stale revision, partial import, and denied access using a concrete proposed binding. Document application-level receipts and conflict handling. No API endpoint design is selected by this landscape.

### 3.7 C2PA

**Proposed use:** evaluate importing or referencing media provenance manifests when an evidence pilot already encounters Content Credentials. Its scope includes cryptographically verifiable provenance and validation under a specified trust model. [Source, sections 1.2 and 14–15](https://spec.c2pa.org/specifications/specifications/2.4/specs/C2PA_Specification.html)

**Boundary:** successful signature/manifest validation does not settle whether depicted events happened as described, a source was candid, a claim is justified, or a signer has institutional authority to decide it. Absence of credentials is not proof of falsity.

**Experiment:** preserve the credential, validation time/result, applicable trust context, and limitations separately from an assessor's evidentiary judgment. Test missing, invalid, and unavailable credentials and a valid credential attached to contested content. Defer mandatory signing infrastructure pending a demonstrated threat-model need.

### 3.8 PREMIS

**Proposed use:** evaluate preservation metadata for retained objects, preservation actions, agents, and relevant rights statements. This review uses the official version 3.0 overview; detailed unit-level selection awaits the dictionary and errata review. [Source](https://www.loc.gov/standards/premis/v3/index.html)

**Boundary:** PREMIS preservation Events and Objects are not automatically the GR operational classes of the same names. Preservation metadata and recorded rights statements do not themselves establish lawful retention, unrestricted disclosure, or legal authority.

**Experiment:** after source review, map an ingest, format migration, access restriction, and disposal action. Examine the difference between preserving content, preserving permitted evidence of an action, and limiting access. Record information loss and institutional assumptions.

## 4. Evaluation sequence and decision record

**Structural role:** limit dependency work to what the next pilot requires.

1. From GR-110/200/210 and early GR-140/150/160 requirements, identify the smallest provenance, epistemic, temporal, exchange, validation, and restriction needs.
2. Evaluate PROV-O, annotation, and temporal mappings against those needs. Preserve mismatches as issues rather than expanding the GR ontology to mirror an external vocabulary.
3. Compare candidate interchange approaches, including JSON-LD; assess SHACL only in conjunction with an RDF-based option. Keep an offline, bounded package within the comparison.
4. Inspect PREMIS's full dictionary and errata before detailed preservation mappings. Evaluate C2PA only for a relevant media case and HTTP only for a demonstrated network requirement.
5. Record costs, unsupported distinctions, loss, implementation evidence, accessibility, security/privacy, licensing, edition pinning, maintenance, and migration implications before selecting a dependency.

For each candidate, the evaluation record includes: GR requirement reference; external edition and exact sections; mapping direction and limits; alternatives; fixture set; independent results; known defects; selected direction or reason for deferral; and affected documents. A selection becomes an architecture decision where it constrains programme design.

**OPEN:** sufficient semantic preservation for a mapping claim; treatment of external version changes; availability and licensing of supporting artifacts; implementation burden for small institutions; and boundaries between shared GR semantics and domain-specific profiles.

This landscape is deliberately bounded. Later pilots can identify other metadata, identifier, security, accessibility, archival, or domain standards for separate verified review. Inclusion here is an evaluation priority, not a normative reference list for a future Standard.

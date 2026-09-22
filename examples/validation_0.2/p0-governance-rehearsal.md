# P0 — Governance rehearsal against Working Draft 0.2

**Class:** Informative synthetic rehearsal and finite arithmetic fixture.  
**Status:** EXPERIMENTAL; authored 2026-09-22.  
**Baseline:** The five Governance Working Drafts 0.2, preserved in [the source archive](../../publications/archive/0.2/README.md).  
**Relationship:** Extends [historical P0 0.1](../p0-governance-rehearsal.md), which remains unchanged.  
**Responsible exercise roles:** UNASSIGNED. Scenario role names below are fictional functions, not actual appointments.

## 1. Purpose and evidence boundary

This rehearsal asks whether the proposed governance rules lead to distinguishable outcomes when a process succeeds, fails, receives an objection, or lacks capacity. It covers founding, proposal intake, review and reply, ballot arithmetic and submission handling, recusal, procedural appeal, conduct review, temporary protection, rights, publication and maintenance. It exercises rule interpretation on synthetic inputs; it does not constitute an operating governance service.

There are three distinct evidence kinds:

1. **Authored expected results:** The finite count inputs and their expected numerical results are in [p0_ballot_cases.json](p0_ballot_cases.json). They are inspectable assertions awaiting or linked to a recorded execution; the JSON itself does not prove they were executed.
2. **Actual arithmetic execution:** A validator run can compare those expected values with recalculation, record its tool and fixture versions, and report disagreements. Its conclusion is limited to the selected finite count states and malformed-input handling. The execution record produced by the repository checker is the evidence of that run.
3. **Synthetic procedural traces:** Fictional notices, dates, persons, reports and actions below are authored scenarios. None is an observation of actual receipt, notice delivery, review, independence, elapsed institutional time or remedy. No procedural fulfillment is inferred from the prose.

The [governance review](../../docs/reviews/validation_0.2/governance_review.md) records GOV-F01 and GOV-O01–05. The fixture uses the conservative timing choices described there. It does not create adopted timing rules, a reduced conformance profile or authority over anyone's rights.

**Executed arithmetic result:** The repository validator was run for this cycle; all 18 authored cases matched their expected results (13 valid-count states and five malformed inputs). The [generated execution record](../../build/validation_0.2/check-report.json) records the run. This executes the numerical exercise only; the institutional sequences below remain synthetic.

## 2. Count domain and discriminating arithmetic cases

The four-count fixture accepts nonnegative integers `N`, `Y`, `D`, `A`, excluding Boolean values and requiring `Y+D+A <= N`. `N` counts eligible, non-recused persons; `Y`, `D` and `A` count their valid approval, disapproval and abstention ballots. `R=Y+D+A` is the valid-return count. The fixture assumes attempted submissions are valid; cases concerning invalid attempts have a separate record partition in section 5.

The three simultaneous conditions from D1-031 are:

```text
returned-ballot quorum:   R >= ceil(2N/3)
substantive minimum:      Y+D >= 3
substantive approval:     Y >= ceil(2(Y+D)/3)
numeric_pass:             all three conditions hold
```

Integer recalculation can use `(2*N+2)//3` and `(2*(Y+D)+2)//3` on this nonnegative domain. A malformed input has no ballot result; it is not a procedurally valid defeated ballot. A zero-person input is mathematically valid for this finite fixture but fails the substantive minimum and says nothing about actual constitution of an electorate.

| Case | N | Y | D | A | R | Required R | Y+D | Required Y | Authored expected result |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| P0-02-B01 | 5 | 2 | 1 | 1 | 4 | 4 | 3 | 2 | Numerical pass; dissent remains to be addressed |
| P0-02-B02 | 6 | 3 | 0 | 0 | 3 | 4 | 3 | 2 | Fail quorum |
| P0-02-B03 | 3 | 1 | 0 | 2 | 3 | 2 | 1 | 1 | Fail substantive minimum |
| P0-02-B04-before | 5 | 2 | 2 | 1 | 5 | 4 | 4 | 3 | Fail approval threshold |
| P0-02-B04-after | 4 | 2 | 1 | 1 | 4 | 3 | 3 | 2 | Arithmetic passes; changed roster requires a new ballot |
| P0-02-B05 | 3 | 2 | 1 | 0 | 3 | 2 | 3 | 2 | Numerical pass; breadth and independence unassessed |
| P0-02-B06 | 2 | 2 | 0 | 0 | 2 | 2 | 2 | 2 | Fail substantive minimum |
| P0-02-B07 | 7 | 3 | 1 | 1 | 5 | 5 | 4 | 3 | Numerical pass at both ceiling boundaries |
| P0-02-B08 | 7 | 2 | 2 | 1 | 5 | 5 | 4 | 3 | Fail approval threshold |
| P0-02-B09 | 8 | 4 | 2 | 0 | 6 | 6 | 6 | 4 | Numerical pass |
| P0-02-B10 | 8 | 3 | 2 | 1 | 6 | 6 | 5 | 4 | Fail approval threshold |
| P0-02-B11 | 4 | 0 | 0 | 4 | 4 | 3 | 0 | 0 | Fail substantive minimum; `Y>=0` alone proves nothing |
| P0-02-B12 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | Fail substantive minimum |

Cases P0-02-I01–I05 respectively supply more valid returns than eligible persons, a negative count, a fractional count, a numeric string and a Boolean. All expect `valid_input=false` and `numeric_pass=null`. These validation choices are a bounded fixture convention; they do not specify an interchange format or a general voting-service contract.

The fixture does not authenticate persons, evaluate interests, receive ballots, observe a deadline, determine consensus, ensure accessibility, make a recusal or supply the independent tally verification required by D1-076.

## 3. Trace P0-02-G0 — Founding and proposal gate

**Controlling provisions:** CH-001/002/014/021/028/029/036–040/058–060; D1-003–005/009–014/059–061; TC-001/032/033; EIP-001/042/044.

| Record | Synthetic input or operation | Expected state and discriminating consequence |
|---|---|---|
| P0-02-G0-01 | An author has compiled all five governance documents and entered proposed role names in a form. | Prepared documents exist; appointments, acceptance, authority and adoption do not follow. Formal roles remain UNASSIGNED. |
| P0-02-G0-02 | The form proposes a founding signatory who can control a repository but has no mandate to represent a university. | Only the actual resource commitment can be considered; the university representation assertion lacks a basis. |
| P0-02-G0-03 | A fictional founding-review package identifies exact editions, prospective signatories, their capacities, proposed appointments, contribution terms and public/protected routes. | Ready for assessment of the proposed founding review; publishing this scenario does not open that review or start a clock. |
| P0-02-G0-04 | No non-conflicted independent reviewer has accepted a bounded role. | Founding-process independence remains unavailable; formal initiation is pending. Preliminary drafting may continue. |
| P0-02-G0-05 | A hypothetical proposal for a correction protocol supplies purpose, scope, affected interests, interfaces, assessment plan and resource estimate. | Register and acknowledge a proposal within seven days in an actual process; registration remains distinct from acceptance. |
| P0-02-G0-06 | The proposal assumes a maintenance owner and protected-record custodian will be found later. | Essential accepted roles are missing. Revise scope or defer; do not describe assumed future capacity as present. |

The real repository can supply source versions and this scenario. It cannot currently supply the synthetic signatory, notice, accepted roles or observations. The missing launch evidence is an institutional action list, not an invitation for an assistant to assign people.

## 4. Trace P0-02-G1 — Accessible comment, disposition, clocks and re-review

**Controlling provisions:** CH-015/017/018/049–052; D1-018–028/068–072/091–093; EIP-058.

A fictional protocol draft `FICT-P0/0.2-A` accepts a challenge only if the sender provides a replacement technical clause. An affected person submits an ordinary-language explanation that the entry identifies the wrong contributor and describes a practical consequence. A second part of the submission alleges that an administrator dismissed previous comments because of the person's affiliation.

The intake trace has separate but linked records: `P0-02-G1-C01` for the technical objection, `P0-02-G1-E01` for protected conduct handling, and `P0-02-G1-P01` for the access/protection decision. The technical record retains the person's actual concern; converting it into structured fields records any interpretive assistance. A safe summary omits identifying conduct details. An unsupported conduct allegation does not dispose of the technical objection.

The proposed technical disposition accepts ordinary-language challenge submissions and identifies assistance requirements. The record preserves the replacement-clause-only alternative, its exclusion risk and the resulting proposed change. The change affects accepted inputs and operator obligations, so it is substantive. Within the same purpose, constituency and conformance subject, it needs at least 30 days of re-review. If it changes those boundaries substantially, renewed scope approval and 60-day review apply. A label such as “minor wording” cannot select the shorter route.

The following is a separate **unchanged-text timing branch**, used only to make successive periods inspectable. It assumes UTC, start as day zero, effective notice at the stated timestamp, no delivery/access failures, no substantive replies and sequential reply/ballot windows. Those assumptions have not been observed.

| Fictional event | Scenario timestamp | Meaning |
|---|---|---|
| Initial review opens | 2026-10-01 09:00 UTC | Identified fixed draft and notice |
| Review closes | 2026-11-30 09:00 UTC | 60-day opportunity; intake closure is not disposition completion |
| Initial dispositions delivered | 2026-12-30 09:00 UTC | 30-day target after close; delivery assumed only in this scenario |
| Reply window closes | 2027-01-29 09:00 UTC | 30 days after disposition notice |
| Ballot opens after assessment | 2027-01-30 09:00 UTC | Frozen package and completed review dossier assumed |
| Ballot closes | 2027-03-01 09:00 UTC | 30-day ballot |
| Decision notice accessible | 2027-03-02 09:00 UTC | Starts ordinary appeal opportunity; not immediate release permission |
| Ordinary appeal period ends | 2027-04-01 09:00 UTC | Release can be considered after the period only if remaining conditions are met |

The substantive-change branch does **not** inherit this unchanged-text schedule. It inserts the applicable re-review, dispositions, reply and reassessment before a new frozen approval package. A later material reply cannot be ignored to preserve a planned publication date.

Failure injections make the obligations observable in a future live rehearsal:

| Injection | Expected response | Evidence still needed |
|---|---|---|
| Comment arrives 2026-10-02 09:00 UTC; acknowledgement occurs 2026-10-10 | Record a missed seven-day duty; explain effect and correction. A determination-target extension cannot make acknowledgement timely. | Actual received/sent logs, delivery limits and handling reason |
| Essential supporting material is inaccessible for affected reviewers | Assess material access failure and extend review sufficiently to restore opportunity. | Access incident, affected scope, extension reasons and revised notice |
| A disposition email is known to have failed | Offer a workable route or extend the affected reply window; do not count failed delivery as silence. | Delivery record, permitted alternative, effective opportunity and progression decision |
| A late reply identifies a material rights defect | Assess before adoption irrespective of receipt date. | Grounds, competent review, disposition and any blocking consequence |
| The 30-day disposition target cannot be met | Notify reasons, interim state and a new target before expiry. A missed target remains historical if already missed. | Actual notice and revised target; no deemed resolution |

## 5. Trace P0-02-G2 — Ballot validity, replacement and recusal

**Controlling provisions:** D1-029–033/073–076; CH-039/040/053; EIP-053/054.  
**Related finding:** GOV-F01. The explicit `I/U` summary partition below is a proposed repair to 0.2, not a claim that 0.2 already specifies that partition completely.

Five fictional eligible persons are designated A through E. Their proposed receipt ledger is:

| Person | Submission history at closing | Final count state |
|---|---|---|
| A | Attributable approve | Y |
| B | Attributable approve, followed by an unverifiable purported replacement | Earlier valid approve remains the latest valid ballot unless an applicable withdrawal or validity decision changes that; unresolved attempt stays in audit |
| C | Attributable disapprove with a material procedural objection | D; grounds require disposition independently of the arithmetic |
| D | Attributable abstain | A |
| E | Attributable but ambiguous “possibly”; clarification not completed | Attempted but not counted valid; record clarification, exclusion/review grounds and challenge route |

The candidate person partition is `N=5, R=4, I=1, U=0`, with `Y=2, D=1, A=1`. Here `I` counts otherwise eligible persons with an attributable excluded or unresolved attempt and no counted valid ballot; `U` counts eligible persons without an attributable submission. A naive `N−R` nonresponse formula would falsely count E as silent. Unattributable and ineligible submissions need a separate inventory; no identity is invented to force them into the roster partition. The numeric quorum passes; the actual effect of E's handling and C's objection still needs reasoned assessment. If exclusion reflects a material failure of participation or administration, D1-028/076 prevents progression even though the numerical rule passes.

If E supplies a valid approval before close, the candidate partition becomes `R=5, I=0, U=0`, with `Y=3, D=1, A=1`. E is not counted twice. The ambiguous earlier attempt remains in the protected audit. A duplicate transmission from A does not add a person or vote. No unknown message is silently converted to an abstention.

For recusal, use B04-before and B04-after from the count fixture. Removing one disapproving person's eligibility after a material conflict discovery changes both the electorate and apparent result. Preserve the first frozen roster and failed tally, record the conflict decision, and start a new ballot if the matter proceeds. Recalculating the old tally is diagnostic; it cannot transform the old failed ballot into adoption.

The real exercise still needs identity and acceptance evidence, announced validity rules, conflict handling, protected audit controls, actual notice/receipt, an independent tally check and a reasoned consensus assessment. A script supplies none of these.

## 6. Trace P0-02-G3 — Procedural appeal and unavailable capacity

**Controlling provisions:** CH-023/029/061; D1-034–039/078–081/093; EIP-010/020.

| Step | Synthetic case state | Expected action and failure boundary |
|---|---|---|
| P0-02-G3-01 | A commenter alleges exclusion from a changed review package and supplies the earlier notice. | Log receipt and relevant decision; acknowledge within seven days; distinguish alleged failure from a finding. |
| P0-02-G3-02 | The original chair offers to determine whether their own notice was adequate. | Do not treat that chair as independent; obtain a non-conflicted appointed reviewer or keep consequential disputed progression pending. |
| P0-02-G3-03 | Review cannot proceed because no independent reviewer is available. | Record the capacity gap, action owner and review point; preserve appropriate protection and affected publication stay. Silence is not rejection. |
| P0-02-G3-04 | In an alternative staffed branch, a reviewer accepts a bounded mandate, discloses interests, explains admissibility, access and timetable. | Provide meaningful reply to the appellant and decision maker. Record how protected grounds can be assessed fairly. |
| P0-02-G3-05 | The reviewer establishes that a material delivery failure prevented reply and remits the decision for renewed opportunity. | Identify the affected stage, remedy, owner and evidence needed to lift the stay. The finding addresses procedure, not factual truth. |
| P0-02-G3-06 | The appeal file is marked closed, but no corrected notice has been delivered. | Remedy implementation remains open; file closure does not establish restored participation. |
| P0-02-G3-07 | A new notice is actually delivered and the required opportunity is restored in a future exercise. | Verify the actual evidence before renewed progression; cite the appeal and repeated stage in the new decision. |

An ordinary appeal is filed within 30 days of accessible notice. Ineffective notice or newly discovered concealed failure can support a reasoned extension by the independent reviewer. That flexibility is not an excuse to omit notice or make the original decision maker determine their own appeal.

## 7. Trace P0-02-G4 — Conduct, temporary protection and reconsideration

**Controlling provisions:** EIP-013–020/056–075/095/096/099–104; D1-035/036/050.

The synthetic report concerns alleged retaliatory exclusion from a working discussion. It uses no actual person's information. The handler logs the report and requested protection, checks conflict and immediate risk, and separates factual allegations from their assessment. Acknowledgement has a seven-day deadline; a complex inquiry may need a reasoned extension of its 30-day initial-determination target, notified before expiry. The extension affects that target only.

An authorized temporary access restriction, if justified, records evidence, scope, safer alternatives, impact, challenge route and notice limits. It needs independent review within seven days and expires within 30 days unless the authority records a justified extension, next review date and remedy plan. Relabelling it “temporary” every month cannot substitute for reasons and review. An immediate protective measure is not a concluded conduct finding.

Three outcome branches are deliberately distinct:

- **Unresolved evidential balance:** Two plausible accounts remain equally supported after fair examination. The allegation is not substantiated under EIP-069. This does not establish that the report was false or permit retaliation.
- **Unsafe grounds:** The information cannot be disclosed or tested sufficiently for a meaningful response. EIP-016/020/064 prevents an unsupported adverse finding even if the author feels confident. The record identifies limitations and feasible non-punitive improvements within authority.
- **Material assessment error identified later:** A reporter or reported person submits a scoped substantive reconsideration request. A competent, non-conflicted reviewer who did not determine the finding handles admissibility and any admitted grounds under EIP-099–102. Mere repetition without grounds can receive a reasoned refusal; an identified error cannot be treated as misconduct for being raised.

Ordinary reconsideration filing is within 30 days of accessible disposition notice, with the stated extension grounds. Acknowledgement is within seven days and an initial admissibility or substantive determination has a 30-day target. The existing remedy gets an explicit stay/modification/continuation decision under EIP-103; neither automatic continuation nor automatic suspension is inferred from the request alone. Temporary restrictions remain subject to EIP-067. A revised finding preserves the earlier decision and records changed practical effects.

These branches exercise the specified criterion and procedural distinctions. They establish neither actual fair treatment nor contextual acceptance of the programme's proposed internal evidence criterion.

## 8. Trace P0-02-G5 — Rights, publication integrity and maintenance

**Controlling provisions:** CH-047/048/071; D1-040–053/082–088; D2-042–045/075/076/082–084; EIP-026–038/082–090/098.

| Synthetic condition | Expected distinction and disposition |
|---|---|
| A foundation manuscript can be read within the project, but public redistribution is unverified. | Preserve source terms; do not infer permission from the manifest or citation. Obtain a reviewed basis, remove the attachment, or restrict the affected distribution. New independent drafting within actual permission can continue. |
| A contributor assents to code terms, while a submitted example contains a third-party dataset. | Code assent does not clear the dataset. Separate artifacts, versions, intended uses and authority evidence. |
| Source is approved in a fictional record, but the rendered PDF omits a requirement. | Block release; resolve the source/render discrepancy and obtain review by someone other than the final editor. Compilation success alone is insufficient. |
| A proposed one-character edit changes `30` to `3` days. | Substantive change: it changes permitted outcomes and needs the relevant review route. Character count does not define editorial status. |
| A real error can be corrected without altering intended meaning or valid outcomes. | Use a documented corrigendum only after the required non-editor review; retain the original publication and effect interval. |
| An exchange dependency becomes unavailable or changes incompatibly. | Assess affected versions, implementation rights, users and migration losses; narrow scope, revise or defer rather than claim interoperability by citation. |
| A serious protection incident requires immediate warning or distribution suspension. | Use bounded emergency authority, review and expiry; permanent amendment or withdrawal retains the ordinary procedure. |
| The maintenance contact leaves and no alternate accepts custody. | Record the vacancy and loss of capacity. Reassign within mandate or defer dependent decisions; an unchanged email address is not an accepted duty. |
| A recovery test restores files but loses protected-access restrictions. | Record a failed material part of continuity; successful copying does not establish preserved custody or safe service restoration. |
| A withdrawal is proposed. | Give at least 60 days' public notice, dispositions, reply and formal approval; preserve historical edition identity and avoid retroactive claims about earlier users. |

## 9. Actual work remaining and measurement plan

The substantive review and authored fixtures are preparatory evidence. The following remains **NOT ASSESSED** as institutional performance:

| Activity | Observable evidence needed | Responsible function |
|---|---|---|
| Founding process and commitments | Actual authority, reviewed texts, signatory assent, dispositions and independent assessment | Prospective founders; UNASSIGNED |
| Public/protected intake and assistance | Real test deliveries, failure logs, assistance requested and supplied, access controls | Secretariat and protected custodian; UNASSIGNED |
| Eligibility, interests and tally verification | Accepted roles, declared capacities, conflict decisions, per-person audit, separate verifier | Chair and non-conflicted verifier; UNASSIGNED |
| Reply, appeal and reconsideration | Observed notices, meaningful response, independent competence, reasons and actual remedy verification | Independent reviewers; UNASSIGNED |
| Rights and release | Competent use-specific review, identified permissions, exact source/render examination, authorization | Rights reviewer and release owner; UNASSIGNED |
| Recovery, maintenance and remedy | Exercised recovery including access restrictions, accepted handover and demonstrated correction | Custodian and maintenance owner; UNASSIGNED |
| Practical burden and accessibility | Operator/reviewer time, waiting time, repeated entry, failed tasks, assistance, missing perspectives and user findings | Pilot evaluator; UNASSIGNED |

For a future staffed rehearsal, record time separately for preparation, interpretation, record entry, assistance, review, correction and waiting. Track one shared dossier instead of copying the same facts into every form, while retaining each required distinction and locator. Report actual measurements and population limits; do not fabricate person-hours or assume a compact scenario scales to a live institution.

The stop conditions are unavailable authority for the attempted action, unsafe disclosure, unresolved material conflict, failure of independent capacity, material exclusion, and an unhandled substantive change. A stop leads to a recorded limitation and response; it does not prohibit unrelated authorized drafting.

**Governance launch remains pending.** A successful arithmetic run establishes only the finite mathematical results it reports. This document does not approve a work item, create a committee, constitute public review, or adopt a Standard.

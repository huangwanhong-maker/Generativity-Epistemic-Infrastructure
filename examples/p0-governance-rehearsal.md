# P0 — Governance arithmetic and procedure design rehearsal

**Document class:** Informative synthetic example and evaluated arithmetic fixture  
**Status:** EXPERIMENTAL; version 0.1; 2026-09-22  
**Related drafts:** [Directives Part 1](../governance/directives_part_1.tex), [Programme Charter](../governance/standards_programme_charter.tex), and [Ethics, Conflict-of-Interest, and Contribution Rights Policy](../governance/ethics_ip_policy.tex), Governance Working Drafts 0.1  
**Assessment boundary:** Five finite numerical states were evaluated in PowerShell. The comment, disposition, recusal, and later procedure examples are synthetic design exercises. No actual ballot, review notice, appointment, complaint, appeal, adoption, or elapsed review period is represented.

## 1. What was exercised

The arithmetic evaluation checks the three simultaneous conditions in proposed D1-031:

- Returned-ballot quorum: `R >= ceiling(2 × N / 3)`.
- Minimum substantive participation: `Y + D >= 3`.
- Approval: `Y >= ceiling(2 × (Y + D) / 3)`.

`N` is the number of eligible non-recused participants; `Y` approvals; `D` disapprovals; `A` abstentions; `R = Y + D + A`. Nonresponses are `N − R`. These are synthetic counts, not participant identities or representation claims. A numerical pass checks only D1-031 arithmetic. Consensus assessment, legitimate appointments, notices, timing, conflict management, comment disposition, and independent appeal capacity require other evidence.

The formula above records the precise rule used in this exercise even if a later revision of draft 0.1 changes D1-031. The values are local proposed defaults, not ISO or IEEE thresholds.

## 2. Evaluated results

The following results were produced by an executed PowerShell calculation on 2026-09-22. The execution also checked that the five numerical outcomes, in order, were `true, false, false, false, true`; the check completed without an error.

| Case | N | Y | D | A | R | Required R | Y + D | Required Y | Numerical result |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| P0-B01 | 5 | 2 | 1 | 1 | 4 | 4 | 3 | 2 | Pass all three conditions |
| P0-B02 | 6 | 3 | 0 | 0 | 3 | 4 | 3 | 2 | Fail returned-ballot quorum |
| P0-B03 | 3 | 1 | 0 | 2 | 3 | 2 | 1 | 1 | Fail three-substantive-vote minimum |
| P0-B04-before | 5 | 2 | 2 | 1 | 5 | 4 | 4 | 3 | Fail approval threshold |
| P0-B04-after-hypothetical | 4 | 2 | 1 | 1 | 4 | 3 | 3 | 2 | Arithmetic passes; an actual decision still requires a new ballot |

P0-B01 exercises the proposed two-approval, one-disapproval, one-abstention case with one nonresponse. Its two approvals meet the ceiling of two-thirds of three substantive votes. It does not establish that the objection has been considered or that three voters adequately represent affected interests.

P0-B02 shows that unanimous approval among three respondents does not overcome the missing fourth return required for a six-person roster. P0-B03 shows that two abstentions cannot supply the minimum substantive participation needed alongside one approval.

P0-B04 models a later discovery requiring recusal of one disapproving participant. Removing that person's eligibility and ballot changes the arithmetic from fail to pass. Under **D1-030**, the material electorate and result change requires a new ballot; the hypothetical recalculation cannot turn the original failed ballot into adoption. The original roster and result remain in the record, with the recusal assessment and a new ballot identifier. Actual recusal, notice, and reballoting have **not** been performed here.

### Reproducing the numerical evaluation

The following PowerShell code reproduces the evaluated formula and inputs. It creates no appointment, ballot, or decision record beyond the displayed calculation.

```powershell
$p0Cases = @(
    @{ Case='P0-B01'; N=5; Y=2; D=1; A=1 },
    @{ Case='P0-B02'; N=6; Y=3; D=0; A=0 },
    @{ Case='P0-B03'; N=3; Y=1; D=0; A=2 },
    @{ Case='P0-B04-before'; N=5; Y=2; D=2; A=1 },
    @{ Case='P0-B04-after-hypothetical'; N=4; Y=2; D=1; A=1 }
)
$p0Results = foreach ($p0Case in $p0Cases) {
    $p0Returns = $p0Case.Y + $p0Case.D + $p0Case.A
    $p0Substantive = $p0Case.Y + $p0Case.D
    $p0Quorum = [math]::Ceiling(2 * $p0Case.N / 3)
    $p0Approval = [math]::Ceiling(2 * $p0Substantive / 3)
    [pscustomobject]@{
        Case = $p0Case.Case
        N = $p0Case.N; Y = $p0Case.Y; D = $p0Case.D; A = $p0Case.A
        R = $p0Returns
        QuorumRequired = $p0Quorum
        QuorumMet = ($p0Returns -ge $p0Quorum)
        Substantive = $p0Substantive
        MinimumMet = ($p0Substantive -ge 3)
        YesRequired = $p0Approval
        ApprovalMet = ($p0Case.Y -ge $p0Approval)
        NumericPass = (($p0Returns -ge $p0Quorum) -and
            ($p0Substantive -ge 3) -and ($p0Case.Y -ge $p0Approval))
    }
}
$p0Results | ConvertTo-Json
if ($p0Results[0].NumericPass -ne $true -or
    $p0Results[1].NumericPass -ne $false -or
    $p0Results[2].NumericPass -ne $false -or
    $p0Results[3].NumericPass -ne $false -or
    $p0Results[4].NumericPass -ne $true) {
    throw 'P0 ballot arithmetic differs from the stated expected outcomes.'
}
```

This bounded calculation does not test malformed inputs, participant identity, duplicate ballots, eligibility evidence, record protection, notification delivery, or arbitrary roster sizes. It is not an implementation of a voting service.

## 3. Synthetic comment, disposition, and re-review

The invented example below concerns a draft challenge-submission Specification. It is not an actual GR publication, received comment, or decision. Scenario versions `FICT-P0/0.1` and `FICT-P0/0.2` are local fixture labels without programme publication authority.

| Record | Synthetic content |
|---|---|
| Draft provision FICT-P0-007 in 0.1 | A submission is accepted only if the sender supplies a proposed replacement clause. |
| Comment P0-C01 | An affected person can identify a misleading classification without knowing how to draft a technical clause. Accept an explanation in ordinary language and provide formatting assistance. |
| Commenter and recipient | Fictional Commenter A and fictional Review Chair B; neither represents an actual appointment or submission. |
| Initial disposition P0-CD01 | Accepted. Making drafting expertise a condition of intake defeats the intended affected-person challenge route. Preserve the substance of the concern and offer assistance with technical formatting. |
| Revised provision FICT-P0-007 in 0.2 | The challenge route accepts a free-text explanation of the concern without requiring a replacement clause. The operator offers assistance when structure is needed for processing. |
| Change classification P0-CC01 | Substantive: the accepted inputs and operator obligations change. The intended conformance subject and affected constituency remain the same in this scenario. |
| Proposed review consequence | Under D1-025/026, show the changed text, rationale, and effects in at least 30 calendar days of public re-review. Notify prior commenters and relevant affected interests; consider consequences for unchanged validation rules. |
| Alternative scope consequence | If the change instead expands the original purpose, affected constituency, or conformance subject substantially, D1-025 calls for renewed scope approval and a full 60-day review. A “minor edit” label cannot determine that question. |
| Reply and later disposition | No actual reply or later disposition exists. D1-023/024 require disposition notice, the reply opportunity, and assessment of continuing disagreement before final approval. |

The design exercise identifies the expected transition: comment → reasoned disposition → changed provision → substantive-change classification → re-review → further disposition and reply → possible progression. It does not advance a real document through that sequence. No fictional dates stand in for elapsed mandatory periods.

A chair's answer to the comment would not itself establish consensus. The approval package would still need the participation and objection assessment in D1-027/028. A protected procedural complaint about exclusion would use the separate appeal or conduct route appropriate to its grounds, with an actual independent reviewer.

## 4. Procedure exercises still to perform

The following are uncompleted tasks. Actual function holders, contacts, and independent reviewers remain **UNASSIGNED**. The required evidence concerns observable actions and accepted responsibilities, not merely completed templates.

- [ ] **Founding basis and commitments:** Identify actual prospective signatories, their capacities, reviewed texts, scope of commitment, dissent, and succession route under CH-036–038. No adoption instrument has been executed by this fixture.
- [ ] **Notice and accessible intake:** Staff and test public and protected contact routes; deliver a real test notice and acknowledgement; check asynchronous and assisted submission. Record actual timestamps and access failures under D1-006/018–021.
- [ ] **Participant roster and conflicts:** Apply published selection criteria, obtain accepted roles, assess concentrated interests, and exercise a recusal with a legitimate substitute. Verify D1-008/030 and CH-039–041 using consenting participants.
- [ ] **Comment and reply:** Have someone other than the draft's author try the submission route, receive a reasoned disposition, reply, and locate the resulting change. Record barriers without treating a self-review as independent review.
- [ ] **Material re-review:** Publish an appropriately labelled rehearsal change and test notification and version access. A compressed demonstration can test mechanics; it cannot establish compliance with elapsed 30- or 60-day minimums.
- [ ] **Independent appeal:** Identify an available, non-conflicted reviewer; test a process objection, access to grounds, opportunity to reply, stay, remedy, and restoration. Repeat the unavailable-reviewer case and show that consequential progression remains pending under D1-034–038.
- [ ] **Protected conduct report:** Exercise the distinct ethics route using synthetic material; verify contact and alternate, confidentiality limits, response, fair consideration, and remedy monitoring under EIP-013–020.
- [ ] **Publication integrity:** Independently compare an exact approved rehearsal source with its render, detect an intentionally omitted requirement, and record release deferral. Check D1-040–042 without representing the draft as adopted.
- [ ] **Maintenance and exceptional procedure:** Distinguish editorial correction from a changed conformance outcome; exercise revision, withdrawal, emergency expiry, role vacancy, and protected-record disposition under D1-043–053.
- [ ] **Rehearsal assessment:** Record time, effort, failures, unfilled roles, remedies, remaining objections, and proposed clause changes in a D1 requirement/evidence register. Obtain independent assessment of the actual founding process before declaring governance ready.

## 5. Limits and programme status

The arithmetic evidence supports the stated results for five finite input states. The comment fixture makes one design consequence explicit. Neither supplies actual participation, represented consent, independent review, rights clearance, notice delivery, elapsed review time, or procedural adoption.

**M1 — Governance launch remains pending.** This file contributes a limited rehearsal artifact to the [governance and pilot evidence plan](../docs/planning/governance_and_pilots.md); it does not complete that gate, approve GR/NWIP-001, activate a committee, or publish an adopted Standard.

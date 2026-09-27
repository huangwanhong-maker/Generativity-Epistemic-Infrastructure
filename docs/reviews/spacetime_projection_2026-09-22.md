# Event-defined spacetime implementation review

**Class:** Informative implementation verification record  
**Status:** EXPERIMENTAL; implementation checks on 2026-09-22; preview verification completed 2026-09-27

## Scope

This review concerns the additive [GR-SPEC-121](../../specifications/GR-SPEC-121-event-time-projection/GR-SPEC-121.md) implementation selected in [ADR-0011](../../decisions/ADR-0011-event-partial-orders-and-spacetime-projections.md): the common Python protocol package, generalized HTTP binding and interface, and the native file chooser styling. It does not adopt a Standard, certify factual warrant, or establish cross-application interoperability. Academia source and native GRRP semantics are unchanged.

The package distribution advances to `gsp-record-protocol` 0.3.0. Its base protocol, schema and profile identifiers remain at 0.2; supported temporal modules and the projection have their own version identifiers. Earlier clients preserve these required modules and refuse unsupported mutations.

## Observed automated checks

| Check | Observed result and boundary |
|---|---|
| Combined common protocol, real-Git storage and application API suite | 178 passed; five browser tests present at that point were skipped because the browser flag was unset. Isolated runtime; 182.42 seconds. |
| Shared protocol suite | 100 passed, including 47 temporal cases. Covers reverse recording order, incomparable events, predecessor closure, self/multiple cycles, withdrawal, qualifications, unknown/unbounded/unscoped extents, invalid bounds, subject grouping, partial incidence and unsupported module versions. |
| Bounded scale and cycle review | A 1,000-record, 500-event chain verifies iterative traversal and complete predecessor closure. An independent reachability oracle agreed with cycle component detection for 500 deterministic generated graphs. This is a bounded check, not a throughput benchmark. |
| New authenticated API tests | Six passed against real isolated Git repositories. Covers cut purity, historical labels/order, retained contradictions and withdrawal, uncertainty, ownership, CSRF, unreachable revisions, ambiguous/excessive JSON and legacy reads without migration. Included in the combined total. |
| Browser checks | Seven browser tests passed across focused runs: three temporal workflows, three existing application/graph workflows and one topology-layout regression. The temporal workflow run passed in 95.08 seconds. An existing graph journey exceeded its five-second assertion while other browser tests ran; a bounded 20-second assertion allowance and isolated rerun passed in 28.82 seconds. On 2026-09-27 the layout regression and affected temporal workflow passed together in 52.71 seconds. No application assertion was removed. |
| Script/dependency checks | `node --check` passed for app.js, graph.js and spacetime.js; `pip check` found no broken requirements after the editable package upgrade. |
| Existing publication dossier | `tools/check_validation_package.py` passed its fixed-baseline identifier/status checks and all 18 arithmetic cases. Historical TeX editions were not changed. |

The existing application/API tests retain their scope; passing these cases is not a general security audit. Browser checks use authored accounts and isolated temporary Git repositories. They do not exercise the user's research records.

Together, the completed checks account for **185 passing tests**: 178 non-browser tests and seven browser tests. These were separate runs, not one combined command. The selected-file draft and topology-layout regressions were added after the non-browser run.

## Findings addressed during review

- The ordinary unstyled native file button now uses the application palette, a visible focus state and a bounded responsive control. Selection remains a native file input.
- Temporal order is carried by explicit Relation modules. Descriptive dates, Git timestamps, UUID sorting and display columns do not add precedence. An event saved later can occupy an earlier asserted position.
- Contradictory orders remain retained and inspectable. Unsupported ordering versions also prevent a purported complete cut, while unsupported extents keep affected presence indeterminate.
- Active Relation accounts retain their identity when some participants lack active scope. The topology omits those incidences and exposes a partial-relation diagnostic.
- Browser integration exposed a save/refresh race: a closed module dialog could allow a second edit against the previous head. Dialog completion now waits for the accepted snapshot refresh.
- Review required an explicit modality choice for ordering claims, preservation of unseen relation qualifications on revision/rebase, retention of a failed cut selection on retry, and file-aware unsaved-form comparison.
- Final visual review confirmed node overlap in disconnected and mixed-component topology layouts. Edgeless views now use a deterministic grid; connected views retain the force layout when its node boxes are separate and fall back to the grid when needed. All edges are retained. Browser assertions check desktop/mobile separation and a connected pair alongside an isolated node.

## Evidence and remaining limits

Ignored local screenshots are under `build/webapp-review/`, including the file picker and desktop/mobile spacetime views. They document the authored test scenario, not a deployed institutional workflow. Tests and reproduction commands are in [application TESTING.md](../../applicative_infrastructure/gr_generalized_application/web_application/TESTING.md) and [common verification](../../applicative_infrastructure/common/conformance/README.md).

On 2026-09-27 the local generalized preview was started on port 8000. Its public capability endpoint returned `gsp.spacetime/1` and both temporal module versions; its page included the temporal interface script. The two existing generalized project heads matched the private pre-refresh capture. No demonstration records or automatic migration were added to those projects. This verification preceded the separately authorized source publication; its findings concern implementation behavior, independently of the later source commits.

The view evaluates retained applicability assertions. It does not execute events to infer later target states, decide between competing chronologies, model extended-event overlap, or prove that two accounts describe the same continuing entity. Unscoped records stay separate from active topology. Event ranks and consistent cuts do not establish physical simultaneity or elapsed duration. These limits and further assessment needs remain in [Q-45–48](../planning/open_questions.md).

Assistive-technology evaluation, measured participant comprehension, scenario-specific interpretation selection and independent implementation interoperability remain OPEN. The successful bounded tests supply implementation evidence only.

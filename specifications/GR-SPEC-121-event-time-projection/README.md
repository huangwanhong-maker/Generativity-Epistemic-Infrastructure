# GR-SPEC-121 — Event time and spacetime projection

**Publication class:** Specification  
**Maturity:** EXPERIMENTAL implementation draft 0.1; not an adopted Standard

[GR-SPEC-121](GR-SPEC-121.md) specifies additive event-order and temporal-extent modules, consistent cuts, trajectory grouping, and the `gsp.spacetime/1` topology projection. Recording history remains distinct from represented occurrence order.

The specification develops temporal needs in [GR-120](../../standards/GR-120-information-model/GR-120.md) and [GR-140](../../standards/GR-140-presentation/GR-140.md). Its foundation interpretation and limits appear in the [design](../../docs/planning/event_spacetime_design.md) and [ADR-0011](../../decisions/ADR-0011-event-partial-orders-and-spacetime-projections.md). The shared [protocol package](../../applicative_infrastructure/common/packages/gsp_record_protocol/README.md) is the reference implementation; the [GR-SPEC-130 binding](../GR-SPEC-130-interchange/GR-SPEC-130.md) supplies authorized, revision-pinned HTTP reading.

# GR-SPEC-130 — Generativity Interchange Specification

**Document class:** Specification index  
**Status:** EXPERIMENTAL edition 0.2; not an adopted Standard

The [transaction, Git binding and interchange Specification](GR-SPEC-130.md) defines coordinated project mutation, receipt-based retry recovery, bounded retained files, historical snapshots and distinct export classes for `gsp-record-protocol/0.2`.

Its concrete implementation need and alternatives are recorded in [the graph-workspace design](../../docs/planning/graph_workspace_design.md) and [ADR-0008](../../decisions/ADR-0008-record-graphs-transactions-and-modules.md). The independent [protocol package](../../applicative_infrastructure/common/packages/gsp_record_protocol/) supplies model and transaction validation; the application supplies authorization and the Git/HTTP adapters.

The broader GR-130 interchange programme remains preliminary. This experimental binding does not establish independent academia interoperability, an adopted Standard or a whole-family conformance claim. See the [publication taxonomy](../../docs/framework/publication_taxonomy.md) and [protocol development plan](../../docs/planning/protocol_specification_plan.md) for authority and proposed dependencies.

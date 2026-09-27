# Schemas

The original plan deferred schemas until GR-120 stabilized. Under the accepted standards-first co-evolution procedure, the graph pilot now has an experimental [packaged JSON Schema](../applicative_infrastructure/common/packages/gsp_record_protocol/src/gsp_protocol/schemas/protocol.schema.json), a [validator and fixtures](../applicative_infrastructure/common/packages/gsp_record_protocol/README.md), and [GR-SPEC-120](../specifications/GR-SPEC-120-information-model/GR-SPEC-120.md). This is a bounded profile, not the final GR information model. Structural schema checks alone do not establish semantic reference validity, authorization, verified file bytes, factual truth or whole-practice conformance.

The additive [GR-SPEC-121](../specifications/GR-SPEC-121-event-time-projection/GR-SPEC-121.md) defines required versioned event-order and temporal-extent modules in the same packaged schema. Cross-record event-role checks, chronology consistency and temporal-cut evaluation remain semantic validator responsibilities.

Further possible artifacts:

- JSON Schema
- JSON-LD context
- RDF/OWL vocabulary
- SHACL shapes
- validation examples

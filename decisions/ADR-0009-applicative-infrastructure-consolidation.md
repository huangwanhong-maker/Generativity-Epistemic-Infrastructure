# ADR-0009 — Applicative infrastructure consolidation

**Status:** Accepted for implementation by the user, 2026-09-22  
**Class:** Repository and application architecture decision  
**Extends:** ADR-0007 and ADR-0008; does not adopt a technical Standard

**Subsequent decision:** [ADR-0010](ADR-0010-nested-git-repositories-and-mit-licensing.md) adds independent nested source repositories and current MIT licensing. This decision remains the historical source/runtime relocation record.

## Issue and authorization

The generalized application occupied `webapp/` while its shared record package occupied `packages/`. Academia remained in a separate source repository. The user requested a common implementation area and independently runnable domain applications, reviewed the proposed arrangement, and explicitly authorized migration. Current GR manuscripts continue to guide conceptual changes; relocation does not reinterpret historical academia records or adopt its older restrictions as general GR rules.

## Alternatives

1. Retain separate repositories and distribute shared code as separately published dependencies. This preserves independence but leaves the requested local integration and joint inspection incomplete.
2. Combine application code, accounts, domain protocols and storage into one service. This would couple distinct workflows, identities and technologies before their semantic mapping is validated.
3. Consolidate source beneath `applicative_infrastructure/`, extract bounded common packages, retain independent domain applications and runtime, and preserve imported source provenance. This is the selected direction.

## Selected arrangement

`applicative_infrastructure/common/packages/` contains the independent `gsp_record_protocol` package and extracted `gsp_git_store` package. Application-independent validation and storage remain distinct responsibilities. A future domain can depend on these packages or implement their versioned contracts without depending on another application's interface.

`gr_generalized_application/web_application/` contains the relocated Flask/Waitress application. `gr_academia_application/web_application/` contains the existing npm client/server/protocol workspace; `gr_academia_application/domain_packages/grrp/` contains its Python domain package. Academia's native protocol remains domain-specific pending an explicitly assessed adapter. Files are relocated and launch paths repaired before any new semantic alignment is attempted.

The programme's `standards/`, `specifications/`, `governance/`, `foundation_manuscripts/`, `decisions/` and publication history remain canonical at their existing locations. Shared implementation documentation links to these sources. Existing conceptual papers imported with academia remain historical domain source material, not newly designated programme foundations.

## Source provenance and repository boundary

The programme is currently a directory within the larger `relational_being_infrastructure` Git repository. The academia source has its own independent repository. This migration does not rewrite or merge the outer repository's history. Academia's complete source history is preserved in a verified Git bundle, with source origin, commit, path mappings and file digests recorded in its provenance manifest. Its original checkout remains intact. Dependency directories, runtime, credentials and generated builds are excluded from the source import.

Original notices remain with imported components. The import does not extend academia's component-specific terms to independently authored common packages or change the terms of historical papers. Missing/stale source notices are documented rather than silently rewritten as new grants.

## Runtime and failure boundaries

Ignored development runtime is located under `.runtime/generalized/` and `.runtime/academia/`, with separately recreated environments under `.runtime/environments/`. Applications use explicit resolved data locations. They retain separate account databases, session-cookie names, record stores and processes, with default ports 8000 and 8001. No shared account or cross-application access authority is introduced.

Runtime migration includes account data, records, project Git histories, files, and native identity material. Source provenance manifests exclude private runtime contents. Existing record/project identifiers and accepted Git heads remain unchanged. The generalized legacy schema is not automatically migrated. Database integrity, account/project counts and file digests are compared before the new location becomes active.

The old generalized writer is stopped before copying its data. Original data and source backups remain available for rollback. The original academia checkout and runtime remain untouched. Launch verification and regression tests use isolated test data; they do not register test accounts in the migrated user databases.

## Verification and consequences

The migration is assessed through shared-package tests, generalized API/browser tests, academia Python/TypeScript tests and builds, independent launch checks, consistent runtime comparisons, verified source-history export, and corrected active documentation links. These establish relocation behavior, not cross-application semantic interoperability, security certification, institutional practice or Standard adoption.

Remaining work includes explicit GRRP/shared-protocol adapters and fixtures, source-notice reconciliation, independently assessed authorization and preservation procedures, and any later deliberate source-history integration. Separate application operation does not itself establish those capabilities.

## Affected artifacts

Application source paths, common package metadata/imports/tests, independent launch configuration, application and infrastructure guides, source/runtime migration evidence, specification implementation links, project index, changelog and the open-questions register.

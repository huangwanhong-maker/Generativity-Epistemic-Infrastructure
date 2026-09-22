# ADR-0010 — Nested Git repositories and MIT licensing

**Status:** Accepted by explicit user instruction, 2026-09-22  
**Class:** Repository architecture and current project licensing decision  
**Extends:** ADR-0009; does not adopt a technical Standard

## Issue and authorization

The user requested independent repositories for both applications, an infrastructure repository containing them as submodules, and an independent programme repository containing infrastructure as a submodule. The same instruction requests comprehensive READMEs, MIT licensing and attribution to all contributors through contributor files. The user subsequently confirmed that hosted repositories will be created after local commits are complete.

## Alternatives

1. Keep one repository with ordinary application directories. This does not supply the requested independent version histories and commit pointers.
2. Create nested repositories without submodule entries. This leaves parent clones without a reproducible way to identify and obtain their child revisions.
3. Create four repositories and explicit nested submodules. This is the selected arrangement. Parent commits identify exact child commits; application record repositories remain private runtime repositories outside these source histories.

## Repository boundaries

The programme root is a new independent Git repository. Its former enclosing repository neither tracks the programme files nor receives a commit or index change from this operation. The programme contains one submodule at `applicative_infrastructure/`.

Infrastructure tracks `common/`, its operation tools and repository documentation. It contains submodules at `gr_generalized_application/` and `gr_academia_application/`. Each application tracks its own web application and domain-specific source. All four default branches are named `main`.

Academia imports its preserved original Git history and creates a relocation/licensing commit whose ancestry includes source commit `b98730b46eca5fdf441937aef0180f5b3d4f94dd`. Existing working files are not replaced by checking out the old layout. The original external checkout and the retained history bundle remain unchanged. The other three repositories begin with local initial commits; no earlier source history is fabricated.

Commits proceed from applications to infrastructure to programme. The parent gitlink is updated only after the corresponding child commit exists. Submodule Git directories are absorbed into their parent's metadata using Git's supported operation.

## Local URLs and later publication

The initial `.gitmodules` URLs use `./applicative_infrastructure`, `./gr_generalized_application` and `./gr_academia_application`, relative to their respective parent repository locations. They resolve to this local source hierarchy and support a recursive clone from this workspace. No hosted URL or remote publication is invented.

Before publishing, replace these local-layout URLs with actual hosted URLs and synchronize the local submodule configuration. Push applications first, then the infrastructure commit referring to them, then the programme commit. A parent push does not publish every child commit automatically. Detailed procedures appear in the [repository guide](../docs/development/git_repository_layers.md).

Local file transport is enabled only on individual trusted local clone/initialization commands. Global Git transport policy and global user configuration are not changed.

## Preservation and licensing

Each repository owns its own ignore rules; a parent's exclusions are insufficient inside an independent child repository. Private runtime, account databases, signing keys, local backups, dependencies and generated builds remain untracked. Source Git history and application-managed project record histories are separate layers.

Byte-preserving attributes avoid implicit line-ending conversion of retained source and archival files. Shell scripts retain an explicit LF rule. Historical provenance manifests describe their original capture and are not rewritten to claim that later README/licensing edits were part of the earlier relocation.

Each repository receives an MIT `LICENSE`, scope notes and `CONTRIBUTORS.md`. Copyright attribution refers to all contributors listed in that repository's contributor file; the grant does not transfer their ownership to a collective entity. The user's MIT instruction applies to project-controlled material. Third-party dependencies, vendored notices and independently contributed runtime material retain their own applicable terms. Existing historical licensing records and embedded notices are preserved with an explicit current licensing explanation.

Current repository licensing is distinct from formal adoption of the standards-development governance drafts. Those historical drafts remain inspectable and are not retrospectively rewritten as adopted decisions.

## Verification

Check the four actual repository roots, clean committed trees, exact `160000` gitlink entries, matching checked-out child revisions, nested `.gitmodules`, academia ancestry, ignored private paths, source history integrity and a fresh recursive clone. Confirm the enclosing repository's index and references are unchanged. These are repository-preservation checks, not an application protocol or institutional conformance claim.

## Affected artifacts

Git metadata and gitlinks, boundary-local ignore/attribute files, four repository READMEs, licensing/contributor files and package metadata, repository workflow and verification tooling, changelog, project index and Q-44. The application record protocol and retained user data are unchanged.

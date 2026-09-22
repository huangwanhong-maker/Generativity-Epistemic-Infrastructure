# Git repository layering verification

**Class:** Implementation and preservation review  
**Date:** 2026-09-22  
**Status:** Local repositories committed; recursive clone and independent audit passed

## Scope

The user's requested structure is implemented by four source repositories: programme, infrastructure, generalized application and academia application. Infrastructure is the programme's submodule; both applications are infrastructure submodules. Common packages remain ordinary infrastructure files. [ADR-0010](../../decisions/ADR-0010-nested-git-repositories-and-mit-licensing.md) records authority, alternatives and the selected boundary.

The repositories are local. The user will create hosted repositories after these commits. No remote push, outer-repository commit, application-record mutation or account migration is part of this operation.

## Preflight evidence

- The outer repository tracked no programme paths. Its index digest and references were captured privately for an unchanged-state comparison.
- Academia's original source repository was clean at `b98730b46eca5fdf441937aef0180f5b3d4f94dd`, and its preserved bundle matched the recorded SHA-256.
- An independent bounded scan examined 329 reachable academia blob versions for private-key, common token and obvious runtime-path patterns. It found no matches. This is not a comprehensive secret or security certification.
- Source inventories contained no symlinks or junctions requiring an out-of-workspace traversal. The intentionally retained original source-history bundle was identified separately from private runtime history.

## Construction and attribution

Boundary-local ignores exclude private runtime, account databases, key files, backups, environments, dependencies and generated builds. Git attributes preserve source bytes, with explicit LF handling for shell scripts and existing academia web sources. Local `core.longpaths` supports the nested Git metadata paths on Windows; no global configuration was changed.

Academia's original main history is imported from the local bundle. Its prior tree is loaded into the new index without checking it out over the relocated working files. The new commit therefore records relocation and current documentation/licensing changes as a continuation of the original history.

All four repository roots have comprehensive READMEs, an MIT licence, scope notes, contributor attribution and contribution guidance. Python packages carry their own copies of the MIT/contributor notices for distribution. Earlier academia notices are retained as dated history. The original bundle, foundation manuscripts and research PDF bytes are preserved; current additional permission for project-controlled material is documented separately from their historical embedded notices.

Historical migration manifests remain evidence of the earlier captured state. They are not refreshed to claim that these later licensing, README or Git-boundary changes occurred during the earlier relocation.

## Verification record

| Repository | Initial committed revision | Entries in its own index |
|---|---|---|
| Programme | `992284aad1798109362edfb784475875103a29d8` | 166, including one infrastructure gitlink |
| Infrastructure | `34878d4149b02a3e4a2672147478e60b2452ffc2` | 37, including two application gitlinks |
| Generalized application | `4df26e7297f4f917bcb879a5ebd9648051cbcf2a` | 29 |
| Academia application | `1edbabab28486eeb9994e7298773bb6e7d67bccc` | 142 |

The programme receives a subsequent documentation commit recording this verification; child revisions remain as shown. The initial recursive clone was made from the listed programme commit.

| Check | Observed result |
|---|---|
| Four repository identities and clean trees | Passed in source and fresh recursive clone |
| Parent `160000` entries and checked-out child revisions | Exact matches for all three submodules |
| Fresh recursive local clone | Both levels initialized and checked out successfully |
| Source-to-clone byte comparison | All 371 tracked files identical |
| Historical publication snapshots | All 30 recorded file digests match in the clone |
| Private runtime in clone | Absent; ignore probes and bounded tracked-path checks pass |
| Git object integrity | All four source repositories pass `git fsck`; one harmless unreachable former `.gitmodules` blob in infrastructure |
| Academia ancestry and original checkout | Original source commit is an ancestor; external checkout remains clean and unchanged |
| Enclosing repository | HEAD, complete references, index SHA-256 and programme tracked-path inventory unchanged |
| Python distribution licensing | All three wheels built offline and contain MIT metadata, licence text and contributor files |
| npm licensing metadata | Four local packages and matching lockfile entries identify MIT; third-party entries unchanged |
| Retained notices and documents | Earlier licence snapshots, four academia PDFs and source bundle digests preserved |
| Active Markdown links | All checked current guide targets resolve; historical source snapshots excluded |
| Hosted remotes | None configured; no push performed |

An independent reviewer repeated repository identity, gitlink, clean-tree, object-integrity, ancestry, ownership, privacy and enclosing-repository checks. The recursive clone and byte comparisons were performed separately by the implementing agent. Detailed operator outputs remain in ignored `build/git-layers/`.

Windows initially assigned the newly created repository directories to the sandbox identity. Only the owner field on the new repository/Git directory roots was assigned to the user's Windows account; access-control rules and global trust settings were not changed. Absorption ran under that account. Independent inspection confirmed that repository roots, absorbed Git directory roots and `.git` pointer files belong to the user, and ordinary user Git commands succeed.

Reproduce the structural check with `python tools/check_repository_layers.py` after all four repositories are committed. The check is read-only and does not initialize missing modules or read user records. The previous application regression results remain the dated migration baseline; this task's new verification concerns repository structure, preservation and packaging metadata.

## Remaining boundary

Hosted URLs and remote publication remain deferred by the user. Local submodule URLs describe this local nested source layout; they must be updated to actual hosted repositories before publication. The [workflow guide](../development/git_repository_layers.md) gives the commands and dependency order. Application protocol alignment, existing dependency advisories and operational deployment work remain as previously recorded.

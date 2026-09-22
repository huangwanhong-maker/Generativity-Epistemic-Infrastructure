# Git repository layering verification

**Class:** Implementation and preservation review  
**Date:** 2026-09-22  
**Status:** Local repository construction in progress; final clone verification pending

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

Final creation, recursive-clone and independent audit results will be recorded here before this task is complete. Reproduce the structural check with `python tools/check_repository_layers.py` after all four repositories are committed. The check is read-only and does not initialize missing modules or read user records.

## Remaining boundary

Hosted URLs and remote publication remain deferred by the user. Local submodule URLs describe this local nested source layout; they must be updated to actual hosted repositories before publication. The [workflow guide](../development/git_repository_layers.md) gives the commands and dependency order. Application protocol alignment, existing dependency advisories and operational deployment work remain as previously recorded.

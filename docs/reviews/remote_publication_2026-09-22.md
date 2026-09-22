# Hosted source publication

**Class:** Repository operation and verification record  
**Date:** 2026-09-22  
**Status:** Applications and infrastructure published; programme publication and hosted clone verification in progress

## Authorization and destinations

The user supplied four GitHub SSH destinations and explicitly requested setup and publication of the local commits. Each repository uses its own `origin` and `main` branch. Both `.gitmodules` files now contain hosted SSH URLs. The [repository guide](../development/git_repository_layers.md) gives clone, identity-selection and maintenance commands.

| Repository | Destination |
|---|---|
| Programme | [Generativity-Epistemic-Infrastructure](https://github.com/huangwanhong-maker/Generativity-Epistemic-Infrastructure) |
| Infrastructure | [Generativity-Relational-Epistemic-Applicative-Infrastructure](https://github.com/huangwanhong-maker/Generativity-Relational-Epistemic-Applicative-Infrastructure) |
| Generalized application | [Generalized-Generativity-Relational-Social-Science-Application](https://github.com/huangwanhong-maker/Generalized-Generativity-Relational-Social-Science-Application) |
| Academia application | [Generative-Relational-Academia-Application](https://github.com/huangwanhong-maker/Generative-Relational-Academia-Application) |

## Authentication and publication boundary

Windows OpenSSH authenticated as `huangwanhong-maker` using the user-selected `serendip_id_ed25519` identity. The executable and identity selection are configured locally in each repository. Strict host-key verification remains enabled; noninteractive checks use a temporary batch-mode setting. No global Git or SSH settings were changed, and the private key contents were neither displayed nor added to source.

This operation publishes source repositories. Private account databases, native signing keys, project-record repositories, uploaded material, local backups and environments remain outside source control. An independent bounded preflight checked the repository structure, ignored paths and 638 text blob versions for high-confidence credential patterns without finding a match. This is a limited source audit, not a comprehensive secret or security certification.

## Existing remote history

The programme, infrastructure and generalized remotes were empty when inspected. Academia already contained `08590a2e73fd92578e5e75639c09b5815002d7b9`, a README-only commit changing its copyright attribution to all contributors. It and the migrated local branch had diverged from the retained earlier source commit.

The remote commit was fetched and merged with both parents preserved. The README conflict was resolved by retaining the current comprehensive MIT guide and explicitly crediting all contributors listed in `CONTRIBUTORS.md`. The historical README snapshot, import manifest and original source-history bundle retained their earlier bytes. No force push or history replacement was used.

Published application revisions are generalized `19341ae30c6a4f69cb97c424a6a34617a6e8e478` and academia `4e577f2b3f38a3b3c3dcd009804b06182ea4b70b`. Infrastructure `a363e39f51f18734badaabb52eb3740f3063714b` pins those exact commits. Each child was published before its parent pointer.

## Verification

Both application remote `main` references and the infrastructure remote `main` reference match the corresponding local commits. Final programme publication, complete hosted recursive cloning and final repository-cleanliness results will be recorded before the operation is closed. Detailed command results remain in ignored `build/git-publication/`.

This publication does not adopt a Standard, change application record semantics, resolve the existing dependency advisories or complete cross-application protocol alignment.

# Repository layers and contributor workflow

**Class:** Development and operation guide  
**Status:** Repository arrangement authorized 2026-09-22; hosted SSH remotes configured for publication

[ADR-0010](../../decisions/ADR-0010-nested-git-repositories-and-mit-licensing.md) records the decision. Source histories have four boundaries:

```text
generativity_standards_program/                 Git repository, main
└── applicative_infrastructure/                 submodule, own repository
    ├── common/                                ordinary infrastructure source
    ├── gr_generalized_application/             submodule, own repository
    └── gr_academia_application/                submodule, own repository
```

The programme contains canonical manuscripts, governance, standards-track drafts, specifications, policy proposals and programme decisions. Infrastructure owns shared Python packages and orchestration. Domain repositories own their applications and domain packages. The four repositories retain distinct contributor and MIT licensing files.

Private project repositories used by the applications are a separate recording layer. They remain under ignored `.runtime/` alongside accounts and native identity material. Creating source submodules neither publishes those records nor changes their accepted commits, access controls or licences.

## Hosted repository addresses

Each source repository has its own `origin`. Both `.gitmodules` files use the matching SSH addresses, so recursive cloning can retrieve all referenced source commits.

| Repository | SSH address |
|---|---|
| Programme | `git@github.com:huangwanhong-maker/Generativity-Epistemic-Infrastructure.git` |
| Infrastructure | `git@github.com:huangwanhong-maker/Generativity-Relational-Epistemic-Applicative-Infrastructure.git` |
| Generalized application | `git@github.com:huangwanhong-maker/Generalized-Generativity-Relational-Social-Science-Application.git` |
| Academia application | `git@github.com:huangwanhong-maker/Generative-Relational-Academia-Application.git` |

## Obtain the complete workspace

With GitHub SSH access configured for all four repositories:

```powershell
git clone --recurse-submodules git@github.com:huangwanhong-maker/Generativity-Epistemic-Infrastructure.git generativity_standards_program
cd generativity_standards_program
```

For an existing checkout, including one originally created from the local repository layout:

```powershell
git submodule sync --recursive
git submodule update --init --recursive
git submodule status --recursive
```

Synchronizing updates local submodule URL settings from the committed `.gitmodules` files within this checkout. Unrelated clones retain their own settings; inspect their origins separately when repurposing an older clone. The hosted URLs do not need a file-transport allowance.

Submodules normally check out the revisions pinned by the parent. This often gives a detached HEAD; before developing inside one, select or create a branch deliberately. Do not use `update --remote` as a substitute for restoring the committed parent snapshot.

## Windows SSH identity

The maintainer's Windows identity is `~/.ssh/serendip_id_ed25519`. Its contents remain outside the repositories. Other contributors use their own GitHub-authorized identities. The following PowerShell example selects Windows OpenSSH and the named key without changing global Git or SSH configuration:

```powershell
$sshExecutable = 'C:/Windows/System32/OpenSSH/ssh.exe'
$identityPath = ($env:USERPROFILE -replace '\\', '/') + '/.ssh/serendip_id_ed25519'
$sshCommand = '"' + $sshExecutable + '" -i "' + $identityPath + '" -o IdentitiesOnly=yes -o StrictHostKeyChecking=yes'

git config --local core.sshCommand $sshCommand
git -C applicative_infrastructure config --local core.sshCommand $sshCommand
git -C applicative_infrastructure/gr_generalized_application config --local core.sshCommand $sshCommand
git -C applicative_infrastructure/gr_academia_application config --local core.sshCommand $sshCommand
```

These commands run from the programme root after its submodules are initialized. The settings are local machine configuration and do not accompany source clones. They select the identity, not the key's contents. Strict host checking expects a previously verified GitHub host key in the local known-hosts file; verify a new or changed host key before accepting it. A passphrase-protected identity can still prompt because `BatchMode` is not persisted.

For the first recursive clone with this identity, define the same variables and pass the command for that operation:

```powershell
git -c core.sshCommand="$sshCommand" clone --recurse-submodules git@github.com:huangwanhong-maker/Generativity-Epistemic-Infrastructure.git generativity_standards_program
```

After entering the clone, use the repository-local settings above if this identity should remain selected for future fetches and pushes. On another operating system or with another SSH installation, substitute that system's executable and identity path.

## Commit from children to parents

For an application change, work inside that application's repository, review and commit its changed files, then review and commit the new gitlink in infrastructure. Finally review and commit infrastructure's new gitlink in the programme. From the programme root:

```powershell
git -C applicative_infrastructure/gr_generalized_application switch main
git -C applicative_infrastructure/gr_generalized_application status --short
# Stage the intended application files and commit there.
git -C applicative_infrastructure diff --submodule=log
git -C applicative_infrastructure add gr_generalized_application
git -C applicative_infrastructure commit -m "Update generalized application revision"
git diff --submodule=log
git add applicative_infrastructure
git commit -m "Update infrastructure revision"
```

Academia follows the same order. Common-package changes begin in infrastructure. Programme-only changes begin in the programme root. Review a changed child pointer as a substantive source dependency change, including its actual commits and tests.

These commands do not commit application files automatically: a parent records the child commit identifier, not uncommitted changes inside that child. Use `git status` in each affected repository and `git submodule status --recursive` from the root.

## Maintain remotes and publish

The configured origins and submodule URLs can be inspected from the programme root:

```powershell
git remote -v
git -C applicative_infrastructure remote -v
git -C applicative_infrastructure/gr_generalized_application remote -v
git -C applicative_infrastructure/gr_academia_application remote -v
git config --file .gitmodules --get-regexp '^submodule\..*\.url$'
git -C applicative_infrastructure config --file .gitmodules --get-regexp '^submodule\..*\.url$'
```

If an existing clone still has local origins, update them to the hosted addresses. Use `remote add origin` instead of `remote set-url origin` only when that repository has no origin:

```powershell
git -C applicative_infrastructure/gr_generalized_application remote set-url origin git@github.com:huangwanhong-maker/Generalized-Generativity-Relational-Social-Science-Application.git
git -C applicative_infrastructure/gr_academia_application remote set-url origin git@github.com:huangwanhong-maker/Generative-Relational-Academia-Application.git
git -C applicative_infrastructure remote set-url origin git@github.com:huangwanhong-maker/Generativity-Relational-Epistemic-Applicative-Infrastructure.git
git remote set-url origin git@github.com:huangwanhong-maker/Generativity-Epistemic-Infrastructure.git

git -C applicative_infrastructure submodule set-url gr_generalized_application git@github.com:huangwanhong-maker/Generalized-Generativity-Relational-Social-Science-Application.git
git -C applicative_infrastructure submodule set-url gr_academia_application git@github.com:huangwanhong-maker/Generative-Relational-Academia-Application.git
git submodule set-url applicative_infrastructure git@github.com:huangwanhong-maker/Generativity-Relational-Epistemic-Applicative-Infrastructure.git
git submodule sync --recursive
```

Commit any infrastructure `.gitmodules` change before committing the root's `.gitmodules` and infrastructure pointer. Before publishing, fetch existing remote branches and inspect their relationship to local work. Preserve existing ancestry; if a normal push would reject divergent history, reconcile that history deliberately. The publication commands below use ordinary pushes.

Publish in dependency order so every referenced commit is available:

```powershell
git -C applicative_infrastructure/gr_generalized_application push -u origin main
git -C applicative_infrastructure/gr_academia_application push -u origin main
git -C applicative_infrastructure push -u origin main
git push -u origin main
```

Use each command only after that repository's intended changes are committed and reviewed. Verify that the remote `main` commit matches the intended local `HEAD` in every repository after publication. A parent push cannot publish an unpublished application commit on its own.

A Git archive of the root alone does not contain submodule source; distribute the four repositories or an explicitly assembled recursive source snapshot when complete offline source is needed. Trusted offline clones can still use command-scoped file transport, but hosted `.gitmodules` URLs need explicit local overrides if network access is unavailable.

## Preservation and verification

Academia's original source history is real ancestry of its new branch, with its original complete bundle retained as independent import evidence. New licensing and layout changes are later commits. Historical migration manifests retain their recorded baseline; compare later work through Git history rather than overwriting earlier evidence.

Each repository ignores its own generated and private files. Keep the full runtime separately backed up; source Git commits do not back up accounts, native record histories or signing keys. Do not use recursive `git clean` as a housekeeping command over a working application installation.

From the programme root after committing:

```powershell
python tools/check_repository_layers.py
git submodule status --recursive
```

The checker examines repository identities, committed child pointers, clean trees, attribution artifacts and bounded private-path exclusions. It does not read user records or scan every source blob for secrets. The [initial verification record](../reviews/git_layers_2026-09-22.md) records repository creation and local clone checks; the [hosted publication record](../reviews/remote_publication_2026-09-22.md) records remote-history integration and publication.

These procedures follow Git's official [submodule command reference](https://git-scm.com/docs/git-submodule) and [.gitmodules format](https://git-scm.com/docs/gitmodules). In particular, relative child URLs resolve against the parent's default remote repository; they are not arbitrary paths relative to wherever a terminal happens to be opened.

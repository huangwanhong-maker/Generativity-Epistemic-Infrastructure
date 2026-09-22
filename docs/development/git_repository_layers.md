# Repository layers and contributor workflow

**Class:** Development and operation guide  
**Status:** Local repository arrangement authorized 2026-09-22

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

## Obtain the complete workspace

The currently committed URLs describe the local nested source locations; no hosted repositories have been configured. From a trusted local source workspace, clone into a new directory:

```powershell
git -c protocol.file.allow=always -c core.longpaths=true clone --recurse-submodules <absolute-source-programme-directory> <new-directory>
```

The file-transport allowance applies to that command only. Do not enable it globally merely to clone this hierarchy. A copy or clone made from another storage layout needs URLs that identify the actual child repositories in that layout.

Once real hosted URLs are configured and committed, ordinary cloning becomes:

```powershell
git clone --recurse-submodules <programme-remote-url> <new-directory>
# For a clone created without recursion:
git submodule update --init --recursive
```

Submodules normally check out the revisions pinned by the parent. This often gives a detached HEAD; before developing inside one, select or create a branch deliberately. Do not use `update --remote` as a substitute for restoring the committed parent snapshot.

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

## Configure hosted repositories later

After creating four actual empty remote repositories, add their URLs as `origin` in the matching local repositories. Replace the angle-bracket placeholders below with real URLs; none is a configured endpoint:

```powershell
git -C applicative_infrastructure/gr_generalized_application remote add origin <generalized-url>
git -C applicative_infrastructure/gr_academia_application remote add origin <academia-url>
git -C applicative_infrastructure remote add origin <infrastructure-url>
git remote add origin <programme-url>

git -C applicative_infrastructure submodule set-url gr_generalized_application <generalized-url>
git -C applicative_infrastructure submodule set-url gr_academia_application <academia-url>
git submodule set-url applicative_infrastructure <infrastructure-url>
git submodule sync --recursive
```

Commit infrastructure's changed `.gitmodules` before committing the root's changed `.gitmodules` and infrastructure pointer. Publish in dependency order so every referenced commit is available:

```powershell
git -C applicative_infrastructure/gr_generalized_application push -u origin main
git -C applicative_infrastructure/gr_academia_application push -u origin main
git -C applicative_infrastructure push -u origin main
git push -u origin main
```

No push is performed during local setup. A Git archive of the root alone does not contain submodule source; distribute the four repositories or an explicitly assembled recursive source snapshot when complete offline source is needed.

## Preservation and verification

Academia's original source history is real ancestry of its new branch, with its original complete bundle retained as independent import evidence. New licensing and layout changes are later commits. Historical migration manifests retain their recorded baseline; compare later work through Git history rather than overwriting earlier evidence.

Each repository ignores its own generated and private files. Keep the full runtime separately backed up; source Git commits do not back up accounts, native record histories or signing keys. Do not use recursive `git clean` as a housekeeping command over a working application installation.

From the programme root after committing:

```powershell
python tools/check_repository_layers.py
git submodule status --recursive
```

The checker examines repository identities, committed child pointers, clean trees, attribution artifacts and bounded private-path exclusions. It does not read user records or scan every source blob for secrets. The [verification record](../reviews/git_layers_2026-09-22.md) records the initial creation and clone checks.

These procedures follow Git's official [submodule command reference](https://git-scm.com/docs/git-submodule) and [.gitmodules format](https://git-scm.com/docs/gitmodules). In particular, relative child URLs resolve against the parent's default remote repository; they are not arbitrary paths relative to wherever a terminal happens to be opened.

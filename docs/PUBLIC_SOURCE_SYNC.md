# Public source and private development sync

Gitea retains the full development record. The public line starts at clean root
`ed1726a4e8d051ac8ebe8944509b0d343bd159d6`, exported from Gitea main
`c54e01c7fccba711271f3b68c025c7ba4f00a804` (#284/#286). Setup is tracked in
Gitea #287. The two histories intentionally have different roots.

## Local arrangement

- Private checkout: `C:/Users/shaun/Development/city-sim-builder` on `main`.
  Its existing local changes/commits are not a publication baseline.
- Public checkout: `C:/Users/shaun/Development/city-sim-builder-github` on
  `github-main`, using Git worktree in the same local repository.
- `origin` is Gitea. `github` is
  `https://github.com/shusain/city-simulation-vr.git`. Its source branch is
  `github-main`, now the default for forks and source PRs; existing GitHub `main`, release tags and
  release assets remain the playtest record. Never overwrite or merge that
  unrelated release history into the source branch.
- The `github` remote's default push refspec maps only local `github-main` to
  remote `github-main`; tag auto-fetching is disabled. Still use explicit pushes
  from the guarded public worktree. No private branches or tags are published.
- Worktrees share Git/LFS objects and remote-tracking refs, but have separate
  checkouts and indexes. Private objects existing locally is expected. Only
  ancestry reachable from the explicitly pushed public ref may reach GitHub.
- Public worktree `push.default=nothing` disables Git's implicit branch-selection
  fallback. The explicit `remote.github.push` mapping above takes precedence and
  selects only `github-main`; still name the refspec when publishing. Its local
  pre-push hook verifies ancestry and then runs Git LFS pre-push. Other worktrees
  retain their existing hooks and push settings. Hooks/settings are local setup;
  they are not automatically installed by cloning the source.

Never switch/reset the occupied private checkout to synchronize. Use fresh
integration branches/worktrees from `origin/main` for incoming public changes.
Keep ordinary Gitea review, CI and acceptance requirements.

## Gitea changes to the public branch

1. Fetch `origin` and identify focused reviewed commits after the recorded private
   baseline. Inspect messages, full diffs and prerequisites. Exclude secrets,
   machine configuration, player files and unrelated development.
2. In the public checkout, create a topic branch from `github-main`. Use
   `git cherry-pick -x <reviewed-ordinary-commit>` in dependency order. This copies
   changes without private ancestry. For merge commits, identify and review the
   component changes first rather than guessing a mainline or importing a whole
   private feature branch.
3. Resolve conflicts semantically. Preserve public snapshot/security/sync docs,
   license terms, notices and historical evidence. Use `git cherry-pick --abort`
   if the transfer is unsuitable.
4. Review `git diff github-main...HEAD`, check the entire publication tip for
   secrets, and run affected tests plus the history preflight below. After review,
   fast-forward the public topic into `github-main`. Do not move private refs.
5. Record private/public commit mapping and the updated private baseline on the
   work issue. Cherry-picks have different SHAs; `-x` records the original SHA.
   Mark transferred changes so the next reverse sync does not duplicate them.

For a squash transfer, export a reviewed range and named paths to a patch file
with `git diff --binary --output=<patch-file> <before> <after> -- <paths>`, then
`git apply --index <patch-file>` and commit on the clean public topic. Store the
patch outside tracked source. Avoid PowerShell text pipelines for binary patches.
Review the full result and record the range/mapping. Never copy `.git` or add
private commits as parents.

## Public contributions back to Gitea

1. Fetch `github` and review changes on `github/github-main`, including
   the accepted public commits, rights and dependencies. GitHub PRs between
   branches descended from the clean root may merge normally publicly.
2. Create a fresh `codex/` integration branch/worktree from current `origin/main`.
   Cherry-pick selected ordinary public commits with `-x`. Do not pick the initial
   snapshot root. Review component changes for public merge commits as above.
3. Resolve conflicts, choose affected tests and use the normal draft Gitea PR,
   CI/review/acceptance process. Record public/private commit mapping there.
4. Mark these changes as synchronized. Never merge the resulting Gitea merge
   commit back into the public branch; transfer only genuinely new changes.

## Before every publication

Run from the public checkout, using the exact candidate ref if it is a topic:

```powershell
git status --short
git diff --check
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools/Validation/Test-CityPublicHistory.ps1 -Ref github-main
git lfs fsck
```

Require a clean checkout and review the exact tip. Run a current redacted secret
scanner on tracked working files and history reachable from the candidate,
including relevant archives. For example, Gitleaks history selection is:

```powershell
gitleaks git . --log-opts="github-main --full-history" --redact
```

Do not select all refs for this shared repository's publication scan: Gitea's
private history and historical token remain locally by design. The root check
does not detect newly introduced secrets; both ancestry and content checks are
necessary. Local hooks complement server protections/review and can be bypassed;
never disable them to publish a rejected candidate.

The existing playtest repository is the approved destination (Gitea #289).
Inspect `git remote get-url github`, fetch, and compare `github/github-main`
before updating the source branch. After publication authorization:

```powershell
git push -u github github-main:github-main
```

The installed hook checks every actual pushed tip against the clean root,
restricts this worktree's pushes to remote `github` on github.com, and invokes
Git LFS upload. Verify a fresh published clone with `git lfs pull`, `git lfs fsck`
and the root check. LFS pointer files alone are not a complete source transfer.
New maintainers must install equivalent guards in their own environments.
Inspect this worktree's local setup with `git config --show-origin --get
core.hooksPath` and `git config --show-origin --get push.default`. The custom
hook lives in this worktree's Git metadata; it does not replace Gitea's hooks.
Direct Git LFS CLI uploads do not run Git's pre-push guard. Prefer the guarded
Git push above; never run an all-refs LFS upload from this shared repository.
If a separate LFS upload is needed, explicitly select only the verified public
ref, for example `git lfs push github github-main`, after the same preflight.

The existing GitHub `main` has unrelated release-documentation history. The
source was deliberately published as an additional `github-main` branch. Do not
replace `main`, move release tags or replace release assets as part of source
sync. The user separately authorized changing the default to `github-main` after
publication. A future unexpected destination/ref conflict is a
reason to stop and review, never to force-push.

Contributors land on default `github-main` for source work and forks. Keep source
PRs targeted there; the separate `main` branch holds playtest docs. An explicit
source-only clone is also available:

```powershell
git clone --branch github-main --single-branch https://github.com/shusain/city-simulation-vr.git
```

Run `git lfs pull` and `git lfs fsck` in that clone. Target source pull requests
at `github-main`, not the playtest documentation branch.

Never push private `main`, merge private `main`/`origin/main` into a public branch,
allow unrelated-history merges, or push all branches/private tags/a mirror.
Always name the public refspec explicitly. If public ancestry is contaminated,
stop publication and reconstruct from the last verified public tip; do not
automatically rewrite shared history. Gitea retains its original history.

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
- `origin` is Gitea. No GitHub source destination has been selected/configured.
  Existing GitHub playtest releases remain untouched.
- Worktrees share Git/LFS objects and remote-tracking refs, but have separate
  checkouts and indexes. Private objects existing locally is expected. Only
  ancestry reachable from the explicitly pushed public ref may reach GitHub.
- Public worktree `push.default=nothing` requires an explicit refspec. Its local
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

1. Once a GitHub source remote is authorized/configured, fetch `github` and review
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

When an approved GitHub source destination is supplied, add a remote named
`github` with that exact URL. Inspect `git remote get-url github` and fetch before
updating any existing branch. Only after publication authorization:

```powershell
git push github github-main:main
```

The installed hook checks every actual pushed tip against the clean root,
restricts this worktree's pushes to remote `github` on github.com, and invokes
Git LFS upload. Verify a fresh published clone with `git lfs pull`, `git lfs fsck`
and the root check. LFS pointer files alone are not a complete source transfer.
New maintainers must install equivalent guards in their own environments.

If GitHub already has release history or unrelated commits, stop and decide how
to preserve them; a separate empty source repository may be appropriate. Do not
force-push over an existing destination. No GitHub destination or publication is
implied by local setup.

Never push private `main`, merge private `main`/`origin/main` into a public branch,
allow unrelated-history merges, or push all branches/private tags/a mirror.
Always name the public refspec explicitly. If public ancestry is contaminated,
stop publication and reconstruct from the last verified public tip; do not
automatically rewrite shared history. Gitea retains its original history.

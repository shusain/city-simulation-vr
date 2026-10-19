# Contributing to City Sim VR

Read [the license](LICENSE) first. This is a source-available project for private,
noncommercial builds, modifications and personal play. Noncommercial source
forks and fork-based patches/PRs are permitted, including the covered project
assets needed to build locally. Keep notices intact and identify modified forks
as unofficial. Game builds, standalone asset packs and covered mods may not be
distributed; commercial derivatives require separate permission. Contributions
are subject to the license's contribution grant, which allows the project owner
to distribute and license accepted changes.
Keep separately licensed third-party material and its notices intact.

## Development checkout

The supported development host is Windows with Unreal Engine 5.7.4, Visual Studio
C++ tools and Git LFS. Blender 5.0 is needed only for asset authoring. Meta XR
203.0 is restored separately from its official licensed distribution into
`CitySimVR/Plugins/MetaXR`; never commit the vendor plugin. Build scripts retain
the tracked compiler fix. Android tooling is needed only for Quest work, not
ordinary Windows changes. See [the verified environment](docs/ENVIRONMENT.md)
for exact versions and existing commands; do not reinstall tools to resolve an
unrelated build failure.

1. Install Git LFS (`git lfs install`) before cloning the maintainer's source
   repository. Run `git lfs pull` and `git lfs fsck` after cloning.
   On GitHub select/clone `github-main`; default `main` contains playtest docs.
   Include `github-main` in your fork and target source PRs at that branch.
2. Restore the separately licensed dependencies above and open
   `CitySimVR/CitySimVR.uproject`. Keep the original VR-template map intact.
3. Read [AGENTS.md](AGENTS.md), [the issue queue](docs/DEVELOPMENT_TASKS.md),
   the selected Gitea issue and its comments, [workflow](docs/DEVELOPMENT_WORKFLOW.md)
   and [shared contracts](docs/DEVELOPMENT_CONTRACTS.md). Gitea owns current scope
   and acceptance. Historical evidence is a record, not a new work queue.

## Offline tools

Use Python 3.11 or newer with the dependencies in `Tools/requirements.txt`.
For an independent local environment, from the repository root:

```powershell
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r Tools/requirements.txt
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools/Validation/Test-CityTooling.ps1 -Python .\.venv\Scripts\python.exe
```

These checks do not launch Unreal or Blender. Each run preserves fresh reports
under `Artifacts/ToolingValidation`. Supply `-Python` or `CITYSIM_TOOL_PYTHON`
to choose an existing environment. No Codex installation is required. Python
inside Unreal and Blender remains supplied by those applications; do not replace
their interpreters with this virtual environment.

## Changes and validation

- Use an isolated branch/worktree from current remote `main`. Check the entire
  base-to-head diff and stage named files; preserve other contributors' work,
  saves and running games. Use descriptive feature names for folders, commands,
  test profiles and output files. Issue IDs belong in issue links and provenance.
- Simulation and commands live in `CitySimVR/Source/CitySimVR`; Blueprints compose
  presentation/UI. Use `CitySimVR/Content/CitySim` for new game content. See
  [architecture](docs/ARCHITECTURE.md) and [the tools index](Tools/README.md).
- Fork the published source repository and submit a patch or PR from a scoped
  branch in your fork, send a patch directly, or request access for a scoped
  branch and draft Gitea PR. Keep fork build artifacts private. Identify
  third-party sources and rights in your submission.
- For implementation, push the scoped branch and open the draft PR before
  playtest. Maintainer Drone runs the Editor build with warnings as errors and
  the affected exact cases selected in `Tools/CI/Tests.txt`. The shared Windows
  worker has capacity one; follow [queue ownership](docs/DRONE_CI.md). Contributors
  without CI access should submit their patch and local evidence for the
  maintainer to validate; never request or copy worker credentials.
- Windows desktop is the ordinary delivery target, followed by PCVR. Package
  Windows at a useful playtest point. Quest packaging requires a named mobile/VR
  risk or explicit milestone; shared source alone is not a reason. Local Unreal
  builds are for a recorded CI gap or focused debugging, not duplicate CI work.
- Report the exact revision, checks actually run and remaining observations.
  Human playtest acceptance and normal review/checks precede merging. Publication,
  maintained-package replacement and device deployment have separate gates.

## Secrets, history and third-party files

Maintainers: follow [public/private source sync](docs/PUBLIC_SOURCE_SYNC.md) for
the isolated public branch and reviewed transfers in both directions. Never
merge the original Gitea history into a public branch.

See [security guidance](SECURITY.md). Do not commit credentials, local config,
signing keys, player files or generated build output. Use `git diff --cached`
and a redacted secret scan before pushing. Never paste a suspected secret into
an issue or PR. Retain historical receipts and approval manifests unchanged;
the [semantic naming guide](docs/SEMANTIC_NAMING.md) explains the remaining
intentional historical references.

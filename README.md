# City Sim VR

A city-building simulation for Windows desktop, PCVR and Quest, built with
Unreal Engine. The tabletop sandbox includes deterministic simulation, roads,
utilities, zoning, households, commerce, civic services and versioned city saves.
The original VR-template scene remains a regression fixture.

## Source access and license

Public source lives on the
[`github-main` branch](https://github.com/shusain/city-simulation-vr/tree/github-main).
This is the default branch for source forks and PRs. The separate `main` branch
retains the existing playtest documentation and release history.

This project is **source available for personal, noncommercial use** under the
[City Sim VR Personal Use Source License](LICENSE). You may inspect, modify,
build and play it privately, share noncommercial source forks, and submit
patches or pull requests from forks under the license's terms. Distribution of
compiled game builds, standalone covered assets and covered mods, hosting the
game for others, and commercial derivatives require separate written permission.
Contributions are subject to the license's contribution grant.
Separately licensed third-party material retains
its own terms. Unreal Engine and Meta XR are obtained separately under their
respective licenses; this repository does not grant rights to those products.

See [CONTRIBUTING.md](CONTRIBUTING.md) for setup, contribution terms and validation,
and [SECURITY.md](SECURITY.md) for local credentials and Android File Server.

## Get started

1. Install Git LFS before cloning (`git lfs install`), then run `git lfs pull`
   and `git lfs fsck` in the checkout.
2. Use the verified Windows toolchain: Unreal 5.7.4, Visual Studio C++ tools and
   separately restored Meta XR 203.0. Blender 5.0 is for asset authoring; Android
   tooling is for Quest work. Follow [Environment](docs/ENVIRONMENT.md) for exact
   existing versions and commands. The build scripts apply the tracked Meta
   compiler compatibility fix.
3. Open `CitySimVR/CitySimVR.uproject`. Read [the issue queue](docs/DEVELOPMENT_TASKS.md)
   and the relevant Gitea issue before changing source. Stage named files and
   review the staged diff; do not include another developer's changes.

Source, original art, imported assets and project configuration belong in Git/LFS.
Build output, caches, player saves, local credentials and the vendor plugin do not.
Preserve personal city files separately when moving a playtest to another machine.

## Build and playtest

Development delivery prioritizes **Windows desktop, then PCVR, then Quest**.
Push a scoped branch and open a draft Gitea PR against `main` before playtest.
The maintainer's capacity-one Drone worker builds Editor with warnings as errors
and runs affected exact cases from `Tools/CI/Tests.txt`. See
[the CI workflow](docs/DRONE_CI.md) for ownership and persistent receipts.

At a useful delivery point, promote the intended successful check to
`windows-playtest`; verify its actual receipt, applicable startup/smoke checks,
ZIP/hash and revision before requesting human playtest. Quest packaging needs
a named VR/mobile risk or explicit milestone. Shared source alone does not
require an Android package. Device observations remain unconfirmed until observed.

Local build wrappers under `Tools/Build` are fallbacks for recorded CI gaps or
focused debugging. Launch commands under `Tools/Launch` use the maintained local
package. Candidate builds, maintained-package promotion, public tester releases
and device deployment are distinct operations with their own authorization.
The separate [tester repository](https://github.com/shusain/city-simulation-vr)
hosts maintainer-published playtest builds; it is not a source mirror.

## Project guide

- [Contributor setup and offline Python checks](CONTRIBUTING.md)
- [Tools by function](Tools/README.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Simulation defaults](docs/DEVELOPMENT_CONTRACTS.md)
- [Development workflow](docs/DEVELOPMENT_WORKFLOW.md)
- [Current issues](docs/DEVELOPMENT_TASKS.md) and [completed work](docs/CHANGELOG.md)
- [Assets and provenance](docs/ASSET_ORGANIZATION.md)
- [Semantic automation naming](docs/SEMANTIC_NAMING.md)

Historical evidence and archives retain their original identifiers and links.
They do not define the current implementation queue or establish unobserved
headset behavior.

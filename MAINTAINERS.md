# Publishing playtests

This is an independent distribution repository. Source stays in the separate game project/Gitea repository. Only instructions, issue forms, release notes and the three reviewed helper scripts enter Git; runtime ZIPs belong in GitHub Releases.

## Quick release from the game source folder

Close the Unreal Editor, write reviewed release notes in a local file, and choose a fresh build ID. Run in PowerShell as the Windows user signed into Git Credential Manager:

```powershell
& .\Tools\Release-TesterBuild.ps1 -BuildId playtest-YYYY.MM.DD.N -ReleaseNotesPath .\Artifacts\playtest-notes.md -Publish
```

The command builds/cooks/packages Windows, exports a source-free runtime ZIP, validates inventory and checksums, extracts it into an isolated profile and runs the cold-start entry-menu check followed by desktop menu/render/save smoke (20 captures). It updates the README download link, commits/pushes **only this distribution repository** and its build tag, uploads a draft prerelease, checks GitHub's asset sizes/SHA256 digests, publishes it, then anonymously downloads and validates all three attachments. The same Windows ZIP supports Desktop and SteamVR PCVR. No Quest install, source commit/push or GitHub CLI is required.

Omit `-Publish` to prepare and validate a local candidate without a commit/push/upload. To publish that unchanged candidate, or retry an interrupted upload:

```powershell
& .\Tools\Release-TesterBuild.ps1 -BuildId playtest-YYYY.MM.DD.N -ReleaseNotesPath .\Artifacts\playtest-notes.md -Resume -Publish
```

`-Resume` skips building/exporting, requires identical notes, and reruns extracted validation. Build IDs and published assets are immutable. Matching partial drafts are resumed; conflicting assets/notes/tags stop the command instead of being overwritten. After publication, an unchanged retry verifies the existing release. Use a new build ID for code, package or document changes. Results are in `packages/<id>/release-result.json`, anonymous downloads in `packages/<id>/verification`, and private extracted QA evidence under the source project's `Artifacts/QA-D73`.

Keep public TESTING/KNOWN_ISSUES/MAINTAINERS accurate before running. The release command preserves prose edits and synchronizes only the three managed `tools` helpers from source templates. Changes to template prose must also be applied deliberately to this repository. Release notes should distinguish automated checks, user-confirmed behavior and pending physical/headset/laptop checks. The release command does not replace required shared-code Android integration or device acceptance.

## One-time setup / authentication

The configured repository is public `shusain/city-simulation-vr`, with Issues enabled. A fresh distribution checkout needs an HTTPS GitHub origin and main branch; the helper verifies both against `-Repository` (the source release command defaults to this repository). Use installed Git Credential Manager to sign in before publishing. Credentials are read only into memory, never logged or bundled. No global Git safety setting is changed.

## Lower-level steps

`Tools/Build-PCVRSandbox.ps1`, `Tools/Prepare-TesterDistribution.ps1` and `Tools/Test-TesterDistribution.ps1` remain available separately from the source project. The distribution helper previews by default:

```powershell
./tools/Publish-Prerelease.ps1 -Repository shusain/city-simulation-vr -BuildId BUILD-ID
./tools/Publish-Prerelease.ps1 -Repository shusain/city-simulation-vr -BuildId BUILD-ID -UploadDraft
./tools/Publish-Prerelease.ps1 -Repository shusain/city-simulation-vr -BuildId BUILD-ID -Publish
```

Both mutation flags commit/push reviewed distribution docs and the tag. `-UploadDraft` stops after server-hash validation; `-Publish` also publishes and verifies anonymous downloads. Never force-push shared history or replace release assets. Attach the named Windows ZIP, release-manifest.json and SHA256SUMS.txt; GitHub's automatic source-code downloads are not playable. Each asset must be under2GiB. Optional Quest export remains a separate, explicitly validated sideload deliverable. See [GitHub Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases).

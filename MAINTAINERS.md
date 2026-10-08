# Publishing playtests

This folder is an independent Git repository. Its allowlist ignores packages, extracted games, logs, saves and symbols. Keep the game source repository and its Gitea remote separate.

The source project's `Tools/Prepare-TesterDistribution.ps1` prepares `packages/<build-id>` and `releases/<build-id>.md`. It exports only staged runtime files plus tester documents/launchers. Do not copy a project checkout here. Existing repository documents are preserved when preparing another package; update them deliberately when needed.

## One-time GitHub setup

Create an empty GitHub repository under the chosen owner, with Issues enabled and the desired public/private visibility. Do not initialize it with a separate README. From **this folder**, after reviewing the files:

```powershell
git add .gitignore .citysim-distribution README.md TESTING.md KNOWN_ISSUES.md MAINTAINERS.md .github tools releases
git commit -m "Prepare playtest distribution"
git remote add origin https://github.com/OWNER/REPOSITORY.git
git push -u origin main
```

Use GitHub CLI (`gh`) authenticated to that owner/repository for the optional upload helper, or create the release and upload assets through GitHub's website. GitHub CLI is not required for packaging or testing.

## Each build

1. Run `tools/Test-Package.ps1 -PackageDirectory packages/BUILD-ID` and inspect the release notes. The Windows ZIP and its contents are hashed and checked against the runtime-file policy; unexpected files fail validation.
2. Commit the new release notes, create a tag matching the build ID, and push **this repository**:

```powershell
git add releases/BUILD-ID.md
git commit -m "Prepare BUILD-ID"
git tag BUILD-ID
git push origin main BUILD-ID
```

3. Preview the upload command, then explicitly create a **draft prerelease**:

```powershell
./tools/Publish-Prerelease.ps1 -Repository OWNER/REPOSITORY -BuildId BUILD-ID
./tools/Publish-Prerelease.ps1 -Repository OWNER/REPOSITORY -BuildId BUILD-ID -UploadDraft
```

4. Review the draft's notes/assets on GitHub and publish when ready. The helper never publishes a draft, pushes commits/tags, changes visibility or overwrites an existing release. If a partial upload fails, finish that draft through GitHub rather than rerunning a creation that would replace it.

Use named ZIP/APK downloads, `SHA256SUMS.txt` and `release-manifest.json` as release attachments. Do not commit those binaries to Git or Git LFS. Each asset must be under2GiB; the helper checks that limit. Private releases require testers to have repository read access. Issue forms do not need extra labels configured.

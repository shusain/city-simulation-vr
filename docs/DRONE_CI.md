# Drone Windows build worker

Drone repository: https://drone.shaun-husain.com/CitySimVR/citysim/settings. The existing server/Linux runners stay in place. `.drone.yml` routes Unreal work only to a Windows amd64 exec runner with label `citysim:ue57`.

## Default development workflow

User clarification2026-10-18: group approved source exports into one pinned batch and one validation run instead of running full CI for each asset. Preserve every exact source head, approval hash and integration handoff in the batch receipt. Superseded individual jobs are canceled, not passes. Native adoption still receives the focused checks and Windows candidate needed for the integrated runtime change. Group related approved native imports into one integration batch and run its affected checks/candidate at the batch delivery point; do not default to a separate full run for every asset.

User directive2026-10-16: use Drone for routine builds/tests and let its single Windows worker queue the work. Scoped commits, branch pushes and draft PRs for requested implementation are authorized before playtest; do not ask again solely to publish the CI input. This replaces the older PR-after-playtest sequence for validation drafts. Gameplay acceptance and normal review/check/merge gates remain in place.

User clarification2026-10-18: **Windows-only packaging is the ordinary delivery/playtest default.** Keep Editor/focused checks per C++ change; package at useful delivery points. Shared gameplay code or assets do not automatically require Android. Batch periodic Quest coverage at larger integration milestones with a named VR/mobile risk, or select it for UI/input/presentation changes requiring headset validation, concrete platform defects/build risks, explicit requests or release requirements. Record the reason/platform in the current Gitea issue before promoting Quest/both. Hands-on PCVR may use the Windows candidate without an Android build; desktop-only UI changes need no automatic headset pass. Deferred Quest coverage does not hold desktop acceptance open; actual device observations remain unconfirmed until observed.

1. Read the issue queue and selected Gitea issue/comments, inspect Git state and identify the smallest reviewable change plus necessary prerequisites. Reuse a suitable isolated branch/worktree, or base one on current remote `main`; do not publish every local primary-checkout commit or reset/rebase another chat's work. Inspect the complete base-to-head diff, required LFS objects and ignored inputs. The worker supplies the established Meta plugin; uncommitted code/assets or local fixture files do not appear automatically in CI.
2. Run cheap relevant static/source checks locally. Put the affected exact automation paths in `Tools/CI/Tests.txt`; preserve appropriate baseline coverage without selecting broad namespaces/full suites. If a check requires extra fixture inputs/flags, support that scoped check in CI or record a necessary local exception; a green starter manifest does not cover it implicitly.
3. Arrange the native-window handoff below, then push a scoped feature branch and create/update one draft PR against `main` using saved tea login `lan`. Reuse the PR on subsequent fixes. Ordinary feature branches avoid `ci/**` so the PR webhook is the single pre-merge validation job; pushes to `main` run a separate post-merge check. Attach created PRs to the chat when supported; retain the direct Gitea URL if app attachment rejects it.
4. Record the PR URL, submitted SHA, actual tested SHA, Drone build URL/number, status and artifact path on the current Gitea issue. Let the queue run. Continue independent work on nonoverlapping inputs; do not start the same Editor build/tests locally or enqueue duplicate retries. A later push has a different revision/job; preserve that mapping. Retry only a diagnosed failed/canceled job after its cause is resolved. Use the existing authenticated Drone session for job actions, without extracting credentials or changing server authentication.
5. Inspect the actual result: successful stage/step exit codes, `Result.json` at the expected SHA, Editor build and every requested test success line. A queued/running job or green check for an older SHA is not a pass for the current change. Fix scoped failures, push to the same PR and verify its new job. Summarize actual results and pending checks; avoid repeated unchanged polling or keeping the chat occupied solely to watch a long build.
6. At ordinary delivery, promote the intended tested build to `windows-playtest` through Drone. Use `quest-playtest` for a recorded Quest validation need above; use `both-playtest` only when that milestone needs both candidates. Verify candidate receipt/ZIP SHA, extracted bytes, applicable packaged smoke/startup and relevant captures. Give the user that identified candidate with an isolated profile for playtest; do not substitute a locally newer maintained executable. Additional affected packaged checks must be explicitly supplied; this job's menu smoke is not every gameplay feature's acceptance test. After acceptance, follow normal PR review/merge closeout. Do not repeat packaging solely because the merge commit has an identical tree.

Local Unreal compilation/testing is an exception for concrete debugging, asset imports/engine-only work, a check not yet expressible in CI or CI unavailability. Record the gap and coordinate the same native window. Quest build/sign/verification uses quest-playtest for the recorded platform need, not every shared-code change. A local Android build is a fallback for a recorded CI gap, while device deployment remains separately authorized. Docs-only/offline-source work needs proportional static review, not an automatic local Unreal compile. CI output is a candidate; maintained-package replacement, device deployment and releases retain their own gates.

## Queue and shared native window

Issue plans and detailed build handoffs now live on Gitea; the compact [issue queue](DEVELOPMENT_TASKS.md) only links them. **Parser compatibility:** keep live standalone bold `window CLAIMED` markers in the primary `docs/DEVELOPMENT_TASKS.md`, the wrapper’s input. Do not relocate active markers solely into Gitea or this runbook. The `Drone CI` prefix remains the shared CI-owner token. Detailed queued jobs, PR/SHA/status and resume/release ownership belong on the issue. See [workflow](DEVELOPMENT_WORKFLOW.md#native-reservation-compatibility).

The offline step enters through `Tools/CI/Test-CityTooling.ps1`. Select an
existing Python 3.11+ environment with `Tools/requirements.txt` using `-Python`
or `CITYSIM_TOOL_PYTHON`; otherwise `.venv` and PATH are tried. Dependencies must
work without interactive user-site packages. The current worker's installed
bundled Python remains a final fallback; no install or runner restart is needed.
All offline receipts/child logs persist under `CITYSIM_CI_ROOT/Tooling/<build>-<revision>-<run>`;
failed child stderr is printed. `CI-Result.json` identifies submitted/tested
revisions and runtime. This step never starts Blender or Unreal.

Capacity1 serializes CI pipelines on `citysim-JARVIS`. It does not reserve the workstation against local Unreal tools. The wrapper currently fails when another task has a bold native `window CLAIMED` header or a local Editor/UBT/UAT process; it does not wait for that local owner to release. Do not bypass this guard or claim that local/native work is automatically queued.

- Before the first CI job starts, refresh the issue queue/reservations and issue handoffs, queued/running Drone jobs and native process guards. After explicit release of other owners, reserve the CI window with a bold header beginning `Drone CI` and containing `native/source/build window CLAIMED` plus the current date. Put the real feature/task IDs, PRs, SHAs and queued build URLs in its body. The current wrapper recognizes the `Drone CI` prefix as its CI owner; using a feature ID alone as that claim would reject its own job. Maintain one shared CI claim while the queue drains; other chats append their job handoff rather than taking a competing local claim.
- If another task already owns local native/source work, preserve that ownership even when it has no live process. To enqueue before release, first verify the Windows worker has no running job, use `Tools/CI/Set-CityDroneRunner.ps1 -Action Stop`, disable its `CitySimVR-DroneRunner` scheduled task against logon restarts, and record the held state, queued jobs, explicit release condition and responsible resume handoff in the live plan/local receipt. Never stop an active CI job or someone else's process. Do not leave a held worker without an identified resume owner/action. If an idle hold cannot be arranged, defer submission until the existing owner releases; do not intentionally generate busy-guard failures.
- After that explicit release, recheck claims/processes, reserve the CI window and run `Tools/CI/Set-CityDroneRunner.ps1 -Action Start`; it enables the same scheduled task. Resume the existing queued jobs, not new builds or daemons. Leave the worker enabled after completion. The verified D126 handoff used this procedure for Drone4 behind D127; any follow-up must be active/authorized and linked in the task, not assumed to exist.
- While CI owns the window, do not run local Unreal imports, Editor/UBT/UAT, shared native studies or competing builds. Nonoverlapping source/asset authoring can continue against its recorded inputs without taking another native claim or changing CI's ephemeral snapshot. Preserve player sessions and their files. Release the CI window only when queued/running Unreal jobs and their owned native processes are finished and the CI lock is clear. No repeated package/test pass is needed merely to release the claim.

## Repeated Windows firewall prompts — B009

Drone isolates USERPROFILE/LOCALAPPDATA for every job. Unreal5.7 normally auto-launches UnrealTraceServer into that temporary profile, so Windows sees a new executable path on each run. The observed CI firewall entries include30 TCP/UDP rules for those per-workspace trace servers. Application firewall rules require a full path; a wildcard workspace allow rule does not cover future clones.

Routine CI does not collect Unreal Insights profiles. Editor tests therefore use `-notraceserver`; the Windows/Quest wrappers forward the opt-in `-NoTraceServer` switch to UAT as `-AdditionalCookerOptions=-notraceserver`, and CI packaged smoke uses the same runtime flag. Ordinary local wrapper defaults and manual Insights profiling are unchanged. The flag suppresses trace-server auto-launch; it does not disable tracing inside the application or all possible network listeners. No firewall profiles, notifications, existing rules or worker privileges are changed. A genuinely required network service still needs its own appropriately scoped rule.

Verify a new CI workspace's command line/test result, absence of the `UTS: Unreal Trace Server launched successfully` marker and absence of a new workspace trace-server firewall identity. Existing retained entries are evidence, not a reason to delete unrelated firewall rules. See [Epic's trace-server flag](https://dev.epicgames.com/documentation/unreal-engine/unreal-insights-reference-in-unreal-engine-5) and [Microsoft's application-rule path requirements](https://learn.microsoft.com/en-us/windows/security/operating-system-security/network-security/windows-firewall/rules).

## Unattended packaged-game smoke — B012

Windows packaging jobs run all automated game processes from the fixed worker-only scratch directory `Artifacts/CI/WindowsSmoke/Runtime`. Each job copies its retained WindowsCandidate there and verifies every file's length/SHA256 before launch; `SmokeRuntime.json` records the complete source/runtime mapping. Both menu smoke and the selected two-size Client tests use that same executable. ZIPs and the original candidate stay in the unique build directory. This scratch directory is not a human playtest launcher or the maintained game; profiles/logs/captures stay in the job's existing isolated outputs.

The fixed executable has one explicit inbound **Block** rule on all profiles. These local tests need no incoming network connections. Windows therefore retains its normal firewall protection and notification settings while the standard-user worker reuses one known application identity. No broad port/folder allow, elevated runner, background firewall broker or per-job UAC is needed. Manual launches of separately extracted candidates retain normal firewall behavior.

One-time setup, from an administrator PowerShell window, using the actual persistent worker CI root:

```powershell
Tools/CI/Set-CityCiFirewall.ps1 -Action Install -CiRoot 'C:/Users/shaun/Development/city-sim-builder/Artifacts/CI'
```

`-Action Install -WhatIf` previews the exact program/rule; `-Action Status` is read-only and works unelevated. `-Action Remove` removes only this named/owned rule and requires administrator rights. Installation leaves global profiles, notifications, existing rules and the worker task unchanged. CI checks the effective rule before packaging and again before the game starts; missing or changed setup fails with instructions instead of attempting elevation or launching an unconfigured executable.

The scratch slot has its own cross-session lock. Replacement refuses a running process, an unowned directory or a reparse point, and stays under the configured local Artifacts/CI root. Only the marked scratch copy is replaced. Do not manually launch a game or store player files there. Future networked tests would need a separately reviewed network policy; the present pipeline remains local.

## Jobs

- `windows-checks`: pushes to `main`/`ci/**` and PRs targeting those branches build Editor with warnings as errors and run the exact cases in `Tools/CI/Tests.txt`. Feature branch pushes outside `ci/**` wait for a PR, avoiding duplicate checks on each branch push.
- `windows-playtest`: explicitly promote a build to target `windows-playtest` (or `both-playtest`). It rebuilds/checks that commit, runs `Build-PCVRSandbox.ps1` with a candidate archive destination, then the existing narrow packaged menu/render smoke. It retains a Windows ZIP and SHA256. This Drone promotion creates a candidate; it does not install/promote it to the maintained game, merge a PR or publish a release.
- `quest-playtest`: promote to `quest-playtest` (or `both-playtest`). After Editor/focused checks, it calls the existing Android ASTC sandbox build into a unique QuestCandidate archive. It verifies APK package identity, ARM64, min32/target35, signature, embedded payload inventory and byte equality with every container from this cook. Result.json records the APK path/SHA256 and signer fingerprint; QuestVerification.json records container hashes.
- `both-playtest`: an opt-in milestone promotion when both platform candidates are needed; never the routine per-change default. One promotion selects both packaging pipelines. Capacity1 and the shared CI lock serialize their isolated checkouts on the same Windows worker; never start a second daemon or parallel local build. Ordinary PR/main checks still do not package every platform.
- Downloadable artifact hosting remains separate. Files are retained on the worker and their location is printed in Drone logs; Drone does not automatically host them. No packaging job installs to a headset or publishes a release.

Optional `Tools/CI/ClientTests.txt` lists exact packaged-only feature checks. Windows candidate promotion runs those at1280x800 and1024x720 with separate `Client-*` profiles and retains logs/reports/screenshots beside the candidate; ordinary PR checks still run only Tests.txt. Empty/absent ClientTests skips additional feature captures.

The three baseline tests cover placement transactions, deterministic terrain and citizen schedules. Each gameplay PR should adjust the tracked list to its affected cases. The runner requires every listed exact path to report success, including discovery; no zero-test green build. The list is not a full regression suite or a physical-device check.

## Quest toolchain and candidate verification

Quest uses the same signed-in Windows exec worker, Unreal5.7.4/MetaXR203.0, Java21, SDK35, NDK27.2.12479018 and verifier build-tools36.1.0. Drone rewrites USERPROFILE/HOME/LOCALAPPDATA for each clone; the CI wrapper resolves the original account profile through its Windows SID/ProfileList entry and passes the installed SDK explicitly. `CITYSIM_ANDROID_SDK` is an optional nonsecret override. Preflight checks named SDK/compiler/verifier prerequisites and the existing development debug keystore. Before packaging, the existing key is copied into an access-restricted directory under this clone's isolated home, then removed with the temporary workspace; it never enters Git, logs or retained artifacts. Do not generate a new signing identity or put keys/passwords into YAML. Signing remains the established Development APK signing, not store/release signing. The first Quest preflight in build9 rejected the temporary profile; its Windows stage passed and remains evidence while the corrected Quest job is retried separately.

`Build-QuestSandbox.ps1 -AndroidSdkPath <installed sdk> -SkipEditorCompilation -ArchiveDirectory <candidate> -CookOutputDirectory <job output>/Cooked/Android_ASTC` forwards to the established Android wrapper. CI uses SkipEditorCompilation only after its exact Editor compile/tests; game ARM64 compilation, ASTC cook, stage, package and archive still run. UAT's supported CookOutputDir sends cook output to the shorter persistent job directory. Include the concrete Android_ASTC final component: the cooker uses the path verbatim, while staging appends the platform unless it is already present. Build11 failed cooking a261-character engine plugin path in the deep clone; build13 fixed cooking but exposed this staging-path difference. Retain both failures. This path option avoids changing assets or Windows/worker settings. Existing local invocations retain their original defaults and config restoration.

`Tools/CI/Test-CityQuestCandidate.ps1` runs the existing package/signature/API validator, compares embedded OBB containers to this job's staged files, and emits APK/signer/container SHA256 receipts before the temporary clone is removed. APK/receipts persist outside the clone. Check actual Drone success plus Result.json and QuestVerification.json before calling it packaged. Signed package validation is not headset startup, rendering, comfort or manual acceptance. Do not install or overwrite the maintained Quest/Windows packages as part of CI. A CI candidate still reflects the pushed remote revision, which may predate unpublished local gameplay.

## Windows setup

Run from this development checkout in the usual Windows user session:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools/CI/Set-CityDroneRunner.ps1 -Action Prepare
```

This stages the pinned official `drone-runner-exec` v1.0.0-beta.10 binary, verifies its archive SHA256, and prepares an ignored, ACL-restricted `Artifacts/CI/Runner/config`. Set **the existing Drone server's** `DRONE_RPC_SECRET` value in that local file. Do not put it in chat, source control, YAML or command-line arguments. `build.env` contains only nonsecret worker paths.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools/CI/Set-CityDroneRunner.ps1 -Action Start
```

Start registers `CitySimVR-DroneRunner` as a hidden task at this user's logon, owning the actual daemon executable directly, and starts it. It uses the interactive user's SDK/settings/GPU profile, not LocalSystem. The worker runs only while that Windows session is available. Check `Artifacts/CI/Runner/runner.log` for a fresh successful server ping and verify an actual queued job before claiming build operation. No inbound firewall rule is needed for the worker's outbound HTTPS connection. Keep TLS verification enabled. For a config change, use `-Action Stop` followed by `-Action Start`; Stop checks the executable path and verifies that no owned daemon remains, including children left by older shell task wrappers. Start rejects duplicate owned daemons.

The worker accepts only `CitySimVR/citysim`, one pipeline at a time. Exec runs repository commands directly as this user. Keep fork PR execution disabled in Drone; allow only trusted project changes on this worker. Confirm repository access/trust and timeout on the existing server if the first job is blocked; do not broaden access automatically. The currently observed repository timeout is60 minutes; raise it only if a measured cold build requires more.

The pinned Windows exec runner is a beta release. Actual PR, candidate and merged-main jobs have verified this installation; see the results below. Do not repeat environment setup at session entry.

Worker connection VERIFIED2026-10-15: the live Kubernetes Secret and running server RPC setting match the local config, and authenticated HTTPS RPC ping returns200. The initial `Unauthorized` loop came from orphan daemon processes retaining old settings: stopping the old scheduled PowerShell wrapper did not terminate its native children. The corrected task owns the daemon directly. All old owned processes were terminated; direct task-stop/no-orphan behavior was verified, followed by a successful restart, fresh authenticated ping and polling with capacity1. The listener is127.0.0.1:3000. No server, Kubernetes, secret or ingress changes were needed. Infrastructure source location: `X:\shaun\container-configs\microk8s-configs\drone-cicd-server`; live deployment `default/drone-server`, node `ubuntu-workhorse`. Secrets were compared in memory and never printed.

Both YAML stages compile with the actual runner; its generated PR clone script was inspected; PowerShell syntax, both preflight modes and Windows PowerShell5 preflight/log validation pass. The validator accepts archived real Unreal success lines and rejects six negative cases (zero tests, wrong path, failure, nonzero process exit, fatal error after success, prefix-only match). [Sanitized connection evidence](evidence/D126-drone-worker.json); full local evidence `Artifacts/CI/Validation/Preparation.json`. PR41 is merged and actual jobs2/3/4 pass; these are verified results, not a reason to rebuild unchanged inputs.

## Build isolation and evidence

Drone clones into its temporary workspace. Pushes and non-PR promotions must match the submitted SHA. PR promotions retain the PR ref/merge checkout, use the same submitted-head ancestry check as PR checks, and record the actual tested merge. Drone's built-in PR clone merges the submitted head into its target branch; CI verifies ancestry and records both the actual tested merge SHA and submitted head. CI checks tracked cleanliness and Git LFS availability/integrity before Unreal runs. Meta XR203.0 is copied from the known local ignored vendor plugin; the tracked compiler fix is reapplied inside the CI clone. The source plugin and development checkout are not edited. Unreal5.7.4 remains installed at the existing path.

Persistent logs, isolated `-UserDir` profiles, test reports and `Result.json` live in `Artifacts/CI/Builds/<build>-<commit>-<mode>-<attempt>/` outside the temporary clone. Windows candidates contain `WindowsCandidate`, `PackagedSmoke` and `CitySimVR-Windows.zip`. Quest candidates contain `QuestCandidate/Android_ASTC/*.apk`, `QuestPackage.log`, `QuestVerification.log` and `QuestVerification.json`; the APK is the directly usable artifact, with its relative path/hash in Result.json. DDC is reused under `Artifacts/CI/DerivedDataCache`; project Binaries/Intermediate are currently ephemeral, so cold native compilation remains a pilot limitation. No cache clearing or automatic artifact pruning.

`unreal-build.lock` serializes invocations of the CI wrapper across sessions; local tools do not yet acquire that lock. Follow the queue/native-window procedure above. The live-plan/process guard is conservative coordination, not an atomic lock on unmodified local scripts. Running packaged user games can remain open; CI output/profile paths are separate and nothing overwrites the maintained default.

Local preflight (no build, tests, Git/LFS download or plugin writes):

```powershell
$env:CITYSIM_CI_ROOT = "$PWD/Artifacts/CI"
$env:CITYSIM_META_XR_SOURCE = "$PWD/CitySimVR/Plugins/MetaXR"
Tools/CI/Invoke-CityCI.ps1 -PreflightOnly
```

## References

- [Drone Windows exec installation](https://docs.drone.io/runner/exec/installation/windows/)
- [Exec pipelines and PowerShell](https://docs.drone.io/pipeline/exec/overview/)
- [Runner routing](https://docs.drone.io/pipeline/exec/syntax/routing/)
- [Ephemeral workspace lifecycle](https://docs.drone.io/pipeline/exec/syntax/workspace/)
- [Official runner releases](https://github.com/drone-runners/drone-runner-exec/releases)

## First queued job: clone authentication

PR #41 initially queued Drone build1; the Windows worker received it, but Git authentication failed before Unreal ran. Drone marks this repository public while this Gitea installation requires authentication for public Git clones. The live Drone deployment now sets DRONE_GIT_ALWAYS_AUTH=true so its existing OAuth clone credentials are supplied; subsequent jobs verify checkout. No credential was added to YAML or source history. The matching infrastructure manifest edit could not be written on the NAS (Access denied); a nonsecret patch is retained in the development checkout at Artifacts/CI/ServerAlwaysAuth.patch for source reconciliation.

## Verified operation — 2026-10-16

[PR41](https://git.shaun-husain.com/CitySimVR/citysim/pulls/41) merged at ca06f2a, with the same tree as tested head7fdfecb. [Drone2](https://drone.shaun-husain.com/CitySimVR/citysim/2) passed PR Editor/three exact tests. [Drone3](https://drone.shaun-husain.com/CitySimVR/citysim/3) passed Windows packaging/menu smoke; ZIP/extracted startup/seven captures were verified, and the user confirmed the candidate worked as expected. [Drone4](https://drone.shaun-husain.com/CitySimVR/citysim/4) passed merged-main Editor/three exact tests after its queued native-window handoff. [Merge receipt](https://git.shaun-husain.com/CitySimVR/citysim/pulls/41#issuecomment-517), [candidate receipt](https://git.shaun-husain.com/attachments/bba156fe-aafa-43db-a0b4-8734003a4cea), logs under Artifacts/CI/Builds. The worker remains enabled, native window released and one-time follow-up paused. Those earlier jobs do not establish physical headset behavior. Quest CI is added by the scoped follow-up below; hosted downloads remain separate; the retained NAS source patch remains separate source reconciliation.

# Development workflow

Start with [the issue queue](DEVELOPMENT_TASKS.md), then read the selected Gitea issue body, all comment pages, dependencies and linked evidence. The issue owns scope, design, status, acceptance and the next action. This document owns reusable process; [shared contracts](DEVELOPMENT_CONTRACTS.md) owns cross-feature defaults; [Drone](DRONE_CI.md) owns build/queue operations; [ENVIRONMENT](ENVIRONMENT.md) owns verified toolchain/commands. Do not restart environment setup.

## Records and task identity

Gitea issue numbers are the only current task identifiers. Reuse the matching issue; create an issue before selecting new feature, asset, defect or validation work. The repository queue contains only linked Active / Next up / TODO entries and a short status or dependency. Put detailed plans, decisions, source versions, build receipts, playtest requests and handoffs on the issue. Update the issue before changing its queue status. A listed future option is not authorization to implement it.

[BUGS.md](BUGS.md) is only a linked list of open defects. Matching Gitea issues own expected/actual behavior, originating build, reproduction/investigation status, evidence and focused completion criteria. Do not reopen delivered features merely for separate defects or missing coverage. [CHANGELOG.md](CHANGELOG.md) records dated completed features and verified fixes. Historical D/B names, filenames, evidence, branches and archives remain valid references, not a second numbering scheme. Archives are historical and never a second active queue.

Task-specific contracts, plans and review evidence live on their Gitea issue or original fix PR; shared process/commands remain in reusable runbooks. Retired task-named document originals and receipt destinations are indexed in [the migration audit](evidence/legacy-task-documents-migration.json). Historical source attachments and machine receipts retain their original names/bytes.

## Defect tracking

Reuse the matching Gitea issue for the same defect; create one only when no existing owner fits. Keep expected and observed behavior, exact known build/platform, reproduction steps or missing inputs, originating feature, investigation/evidence, focused next step and completion criterion on the issue. Distinguish reported, reproduced, investigating, fix awaiting verification and verified closed. A plausible cause or mitigation does not close a defect; failed test fixtures do not establish a production defect.

BUGS.md contains only links to open defects, including an existing feature issue when it already owns that defect. Historical B-prefixed IDs remain searchable references. Closed source defects may be recorded on their source issues; a source fix does not imply native integration, artistic approval or headset acceptance. Where a historical fix has only its original Gitea PR, preserve its resolution on that PR’s discussion instead of creating a duplicate retrospective issue.

On verified closure, retain the resolution/evidence on Gitea and dated completion in CHANGELOG.md, then remove the bug from BUGS.md. Close only the satisfied scope; preserve separate parent work and unconfirmed physical observations. Missing coverage alone is not a defect. New capabilities selected during a fix get their own feature issue. Documentation moves do not reopen closed defects, accept a pending fix or authorize generation/build/device work.

## Planning and ownership

User direction2026-10-14: detail remaining work before implementation; separate asset generation/review from engine work so independent tools can proceed in parallel. A completed foundation closes when its scoped delivery is evidenced; optional future features receive new issues. An unfinished physical check moves to a named validation issue and stays unconfirmed. Deferred research is not completed measurement and does not block ordinary iteration.

| Lane | Outputs and completion evidence | Tools / dependencies |
| --- | --- | --- |
| Design | Concrete problem, proposed behavior, boundaries, dependencies and smallest useful acceptance check | Repository/source/retained evidence review; resolve mechanics before authoring logic |
| External asset authoring/review | Editable source/presets, stable export IDs, preview sheets or audio auditions, manifest/technical checks, reviewed item/hash references | Blender, SuperCollider or another suitable authoring tool; no gameplay compile/package required to evaluate source art/audio |
| Unreal integration/gameplay | Import/material/cue/catalog mappings or authoritative C++/shared UI, scoped draft PR, Drone Editor/focused checks and identified candidate | Coordinate Source/Content/catalog inputs; let Drone queue builds, alongside normal local development iteration; source previews do not establish renderer/runtime behavior |
| Delivery/playtest | Affected packaged targets at an integration point, exact revision/build, actual runtime evidence and named outstanding physical observations | Drone windows-playtest by default; quest-playtest for a recorded platform need, both-playtest only when the milestone needs both; ordinary launchers/maintained-package and device gates preserved; package verification is not headset acceptance |

**Parallel work:** Tool independence helps, but file/output ownership is decisive. An asset task uses its own study blend/presets/manifest/export/review directory and reads a frozen reference; it does not rewrite production generators/shared manifests or import into Content while another task owns those inputs. Separate Blender and audio tasks can run alongside C++ work. Two tasks using Blender can also be independent with distinct files; two tasks using different tools can still conflict over a shared manifest. Record the owner, protected inputs and handoff destination; preserve running play sessions. Coordinate expensive renders/builds if resource contention is observed, without launching a benchmark program.

**Handoff:** Identify source revision/hash and selected item IDs, export paths, units/pivots/entrances/material slots or audio sample rate/channels/loop points/gain, provenance/rights, preview/feedback receipts and known limitations. The integration task reads current native/catalog state before applying the scoped change. Reviewable source output can close its authoring issue while integration waits. Source approval applies to the identified revision; material shading, spatial mix and actual game interaction require their own engine evidence. Optional isolated Unreal previews/Chaos studies belong to an engine study window, even if their eventual output is a baked asset; they do not require a game package merely to review the study.

**Issue readiness template:** State origin and problem; lane/tools; current/proposed behavior; concrete deliverable/boundary; input/output owner and dependencies; smallest source/engine/package check; handoff; status and next step. Planning issues stay future until selected and detailed. Create a focused implementation issue/slice when design establishes its actual behavior; do not use these headings as authorization to start every feature.

**Defects versus features:** Record a concrete mismatch between expected and observed existing behavior on its matching Gitea issue with the originating feature/build, reproduction status and focused acceptance; add only its link to BUGS.md while open. New mechanics/art/UI capabilities belong in their owning Gitea issue. Unexercised hardware/clean-machine coverage is a validation limitation, not itself a defect; select a bounded validation task when a distribution milestone needs it. Use the same CHANGELOG for completed features and verified bug fixes. Linking a bug to its originating feature preserves context without keeping that feature open.

## Build, publication and platform selection

User clarification2026-10-22: local development tools, including Unreal Editor builds, focused tests, imports, debugging and screenshot capture, are a normal iteration path. Drone capacity1 queues CI jobs in isolated workspaces. Submit scoped jobs without a manual native claim, workstation process check, CI lock or worker hold/resume handoff. Local development may continue alongside CI. Coordinate only actual shared file/output conflicts or observed resource contention; preserve other tasks and player sessions. Keep the existing worker enabled; do not start duplicate runners or jobs. Use Drone for final Editor/focused test validation of the submitted revision; C++ changes still require its warnings-as-errors Editor build and affected exact cases. Local evidence identifies its source revision and does not replace required final CI checks.

Packaging and human playtest are selected by the change's acceptance needs, not required for every feature, issue closure or merge. Simple, bounded changes, especially those without overall game-system effects, may be accepted from reviewed local screenshots/captures or other focused evidence plus applicable final Drone checks. Record why that evidence is sufficient and any remaining limitations on the issue. Multiple independently reviewed features/fixes may merge and close before one useful combined platform playtest milestone. Require a packaged playtest when behavior, integration risk, platform/input/presentation needs or an explicit request calls for it. When needed, promote the tested revision through Drone: `windows-playtest` for desktop/PCVR, `quest-playtest` for a named Quest need, `both-playtest` only when both are needed. Normal review, acceptance and merge authorization still apply; physical observations remain unconfirmed until observed. This clarification supersedes earlier routine-CI-only iteration and mandatory-candidate/playtest sequencing.

Approved asset source exports may share one identified batch validation (user2026-10-18); do not require a full native run per exported asset. Retain the individual approved versions/hashes/source heads and link the batch result. Separate source approval and this shared source check from native integration and gameplay acceptance. Group related approved native imports into one integration batch and run its affected checks/candidate at the batch delivery point; do not default to a separate full run for every asset.

For requested implementation: local iteration/evidence → scoped branch/draft PR → final Drone Editor/focused checks → scoped acceptance and reviewed/authorized merge. Add a platform candidate and human playtest before acceptance only when the change needs them. Scoped branch pushes and draft validation PRs are authorized before playtest. Inspect the complete remote-base diff and necessary LFS inputs; preserve unrelated local commits, dirty work, player files and sessions. Read [the Drone workflow](DRONE_CI.md#default-development-workflow) for exact operations and queue operation. A docs-only direct-main change is allowed when explicitly requested, as for #187; it is not a general gameplay exception.

User priority remains **Windows desktop → Windows PCVR → Quest standalone** for relevant feature delivery. Workstation baselines and the bounded CPU round are complete; reported settings/controls/comfort checks are accepted. Actual PCVR telemetry, laptop performance and #58 thermal matrices are deferred research, not an automatic next pass. Per-city map feature has completed its bounded populated-map integration check and user Desktop80 acceptance. Optimization follows only for a concrete necessary blocker. For later changes, choose affected compile/functional/package checks without repeating full suites or all-platform work by default. Keep explicit launch profiles/shared UI actions; verified commands remain in ENVIRONMENT.

Default ordinary delivery/playtest to Drone `windows-playtest`. C++ needs Editor with warnings as errors and only the exact affected checks in `Tools/CI/Tests.txt`; package at useful delivery points, not every edit. Shared C++/assets do not alone require Android packaging. Select Quest for a larger integration milestone with a named VR/mobile risk, UI/input/presentation requiring headset validation, a concrete platform defect/build risk, explicit request or release requirement. Record the reason and platform in the issue before `quest-playtest`; use `both-playtest` only when both are needed. PCVR can use the Windows package. Deferred Quest coverage does not block desktop acceptance; headset results stay unconfirmed until observed. This 2026-10-18 direction supersedes older blanket all-platform wording in historical issue briefs.

Let the existing capacity1 worker serialize jobs. Local iteration may run alongside CI; coordinate actual shared file conflicts and do not duplicate already sufficient checks or start another daemon. Record PR/build URLs, submitted/tested SHA, status, evidence and next action; queued/running is not a pass. Verify candidate receipt, applicable smoke, ZIP/hash and startup before requesting human playtest of that exact revision. Candidates remain separate from maintained packages/player profiles. Maintained promotion, releases and device deployment retain their gates; package verification never establishes headset rendering, input or comfort.

Preserve existing evidence. Optimize only for an observed blocker or demonstrated feature limit. Larger-map work can include one bounded representative check and a focused fix; it does not launch an automatic benchmark, thermal or capacity program. Backwards compatibility is optional during early development: document breaking IDs/formats/assets for testers, preserve user bytes, and keep current-version saves and valid current assets working.

## CI queue and local development

Drone capacity1 queues CI jobs in isolated workspaces. Submit scoped jobs without a manual native claim, workstation process check, CI lock or worker hold/resume handoff. Local development may continue alongside CI. Coordinate only actual shared file/output conflicts or observed resource contention; preserve other tasks and player sessions. Keep the existing worker enabled; do not start duplicate runners or jobs. Historical native-window markers are no longer CI gates.

## Handoff and completion evidence

On the issue, report the task number/status, delivered behavior/files, source/input revisions, actual checks and results, PR/build/candidate identities and relevant attached evidence. Separate required current acceptance from deferred research. Keep physical input/device observations explicit until supplied. Record deviations/reasons and the next dependency-ready issue. Close only satisfied scope; source review can finish while native integration waits. On completion, move dated acceptance/evidence to CHANGELOG and remove the issue from the queue.

```text
Gitea issue / status:
Behavior delivered / files changed:
Exact source / PR / submitted and tested SHA:
Editor / focused automated checks actually run:
Windows candidate / package smoke / ZIP hash / profile:
Quest reason / package / actual device observation (or deferred):
Human acceptance and remaining checks:
Deviations and reason:
Next dependency-ready issue:
```

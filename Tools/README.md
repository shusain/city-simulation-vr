# CitySim tools

Choose a tool by its function. Issue numbers belong in provenance and evidence, not new maintained tool names. The accepted asset migration is recorded in [issue #150](https://git.shaun-husain.com/CitySimVR/citysim/issues/150); the remaining automation cleanup is [#283](https://git.shaun-husain.com/CitySimVR/citysim/issues/283).

## Supported entry points

| Purpose | Entry point | Notes |
| --- | --- | --- |
| Routine Editor checks and candidate builds | `CI/Invoke-CityCI.ps1` through Drone | Follow [the shared queue procedure](../docs/DRONE_CI.md); do not launch routine local builds. |
| Windows packaging fallback | `Build/Build-PCVRSandbox.ps1` | Existing parameters/defaults and root forwarding command are retained. |
| Quest packaging fallback | `Build/Build-QuestSandbox.ps1` | Use only for a recorded CI gap; root command forwards. |
| Desktop and PCVR launch | `Launch/Start-DesktopSandbox.ps1`, `Launch/Start-PCVRSandbox.ps1` | Launches the maintained game; root commands forward. |
| Isolated Editor launch | `Launch/Start-CitySandboxEditor.ps1` | Existing Desktop/PCVR profiles and PrintOnly; coordinate native ownership. |
| Authorized device deployment | `Distribution/Deploy-{CitySimVR,QuestSandbox}.ps1` | Existing APK/serial parameters; deployment retains its separate gate. |
| Asset and artifact maintenance | [Maintenance index](Maintenance/README.md) | Explicit pinned operations and preserved historical retirement recipes. |
| Blender preview sheets | `Assets/Blender/Render-CityBlendPreviewSheet.ps1` | Saved-blend mesh previews and indexed sheets. The original root command forwards here. |
| Compose existing render manifests | `Assets/Preview/CityPreviewSheets.py` | Shared importable `compose()` API and CLI for both renderers; root `Render-CityPreviewSheet.py` remains available. |
| Temporary native asset galleries | `Capture/Capture-City{Damage,Hospital,Park,School,SportsPark,Station,Waste}Preview.py` | Explicit Unreal Editor captures; original root script names forward here. Coordinate the native window first. |
| Standard UI icon bank | `Create-CityUiIcons.ps1` | Explicit authoring operation; rasterizes/imports sources and writes native assets. |
| Approved condition icons | `Import-CityConditionIcons.py` in Unreal | Verifies the existing pinned approval/source hashes before calling the shared icon module. |
| Editorial ink/repair textures | `Assets/Textures/Prepare-CityEditorialArt.py` | Pinned approved sources; historical source IDs remain provenance. |
| Storefront room textures | `Assets/Textures/Prepare-CityStorefrontRooms.py` | Pinned approved rooms, uniform scale and edge padding. |
| Editorial/storefront native imports | `Assets/Unreal/Import-City{EditorialArt,StorefrontRooms}.py` | Explicit native operations under the coordinated build window. |
| Staged soundscape bank | `Audio/Soundscape/Stage-CityAudioSources.py`, `Import-CityAudioSources.py` | Validated source staging and explicit native import; root commands forward. |
| Exact Unreal test-log verification | `Validation/Test-CityAutomationLog.ps1` | Requires every expected exact test path plus a successful process exit. |
| Offline tooling checks | `Validation/Test-CityTooling.ps1` | Runs real small Python children, offline icon mocks and path/log/report checks. Never launches Unreal. Requires Python 3.11+ and `Tools/requirements.txt`; selects `-Python`, `CITYSIM_TOOL_PYTHON`, `.venv` or PATH. |

Run offline checks from the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools/Validation/Test-CityTooling.ps1
```

Each run creates a fresh `Artifacts/ToolingValidation/<run>/Result.json` and retains its logs. Drone runs this command before its existing Editor checks.

Drone enters through `CI/Test-CityTooling.ps1`. Both drivers accept `-Python` or
`CITYSIM_TOOL_PYTHON`, then prefer checkout `.venv` and PATH. Dependencies must
work with Python's user site hidden. The existing worker retains its installed
bundled interpreter only as a final compatibility fallback; contributor setup
does not require Codex. No automatic install or worker restart is performed.
See [contributor setup](../CONTRIBUTING.md). CI receipts and child logs persist
under `CITYSIM_CI_ROOT/Tooling/<build>-<revision>-<run>`.

## Shared code

| Location | Responsibility |
| --- | --- |
| `CityToolPaths.py` | Find the nearest checkout from a file or directory by its project/Tools markers. Does not depend on `.git` directory layout or script depth. |
| `Common/CityTools.psm1` | PowerShell checkout discovery, Windows process argument quoting, hidden owned-process launch/timeout/exit checks, preserved console logs and fresh JSON report loading. |
| `Assets/Unreal/CityUiIcons.py` | UI icon texture import and material operations with explicit icon selections; importing the module performs no asset writes or command-line parsing. |
| `Assets/Manifests/CityArtManifest.py` | Read-only starter/seasonal JSON loading and source/geometry contracts, shared by native importers and offline adoption checks. No Unreal import or asset writes. |
| `Assets/Manifests/CityAssetCapabilities.py` | Validated semantic asset switches shared by authoring, promotion, import and catalog tools. Task labels never enable behavior. |
| `Assets/Manifests/CityApprovedSources.py` | Pinned export identity/hash and both artifact-list verification shared by texture producers. |
| `Assets/Manifests/CityEditorialArt.py` | Explicit newspaper/repair keys and usages shared by derivation and import; issue numbers only identify approved sources. |
| `UCityEditorAssetLibrary` in the project Editor module | Package-relative import filenames preserving source history; adopted by maintained texture/sound imports and the explicit asset move. |
| `Capture/CityCaptureSession.py` | One temporary-PIE capture callback, shared timeout/error handling and owned-actor cleanup. Importing performs no Unreal calls. |
| `CityBuildingStudyIO.py` | Checked Blender FBX round trips, material restoration and geometry contracts. |
| `Assets/Blender/CityAssetPreview.py` | Blender evaluated-mesh copies, camera fitting, studio lights and rendering; root `CityAssetPreview.py` forwards the existing API. |
| `Assets/Preview/CityPreviewSheets.py`, `CityUnrealPreview.py` | Shared indexed sheets and existing isolated Unreal study helpers. |
| `CityArtKit.py` and domain helpers | Existing reusable authoring geometry, materials and export operations. |

Pass **raw, unquoted arguments** to `Invoke-CityToolProcess`; it quotes them once for the Windows argument parser. It launches a hidden owned process and streams both outputs concurrently to literal files opened with CreateNew, preserving original bytes and existing evidence even when paths contain brackets. It rejects nonzero exits and stops only its own process on timeout. `Read-CityToolReport` requires a report written since that process started and valid non-null JSON. The caller still owns report-schema checks, source approval and native-window coordination. These helpers do not reserve the Unreal window or authorize an import/build.

The two industrial material/normal wrappers now use these helpers and accept `-EngineRoot`; their original modes, timeouts, artifact locations and import/capture semantics remain. Do not replay their completed imports as part of tooling validation.

The maintained Blender preview wrapper now uses the same helpers. `-TimeoutSeconds` bounds each render/compose process (default900); both modes preserve logs, including `-ListObjects`, whose console output is still printed. Use a new or empty `-OutputDirectory`: nonempty folders are rejected before launching any child. Rendering requires a fresh successful manifest matching the input blend's path/hash, then a fresh complete sheet index. Existing rendering/layout algorithms, default views/engines and reference names are retained. Three curated composition callers import `Assets.Preview.CityPreviewSheets` directly; old entry points remain available. Offline tests use synthetic PNGs and a small local console protocol fixture; they do not establish Blender/Unreal rendering or device appearance. Python/Pillow is the existing composition dependency and is also required for the offline preview tests.

## Temporary PIE captures

The seven gallery entry points above retain their mesh selection, camera positions, feature-specific scene steps and semantic report/image basenames. They now share session cleanup and live under `Capture`. Both canonical and forwarding scripts have `main()` guards; importing them does not start PIE, register callbacks or write files. These are explicit native operations, not a reason to rerun completed studies automatically.

Captures refuse a pre-existing PIE session and preserve existing output images, reports and error files. To repeat a view, set `CITYSIM_CAPTURE_OUTPUT` to a **fresh directory inside this checkout's `Artifacts`**, then launch the chosen script through the established Unreal capture command. An explicit Python call can instead use `main(output_directory=<fresh path>)`. The default `Artifacts/Preview` destination uses semantic report/image basenames. Historical numbered captures remain intact. Do not delete existing evidence to make a rerun pass. This is an intentional change from the old scripts' automatic output deletion.

The shared session attempts callback unregistration, end-play, every explicitly owned actor's destruction and Editor quit. A timeout or feature exception writes a diagnostic sidecar; cleanup exceptions append diagnostics while remaining cleanup continues. It never saves a map/city or deletes player files. Native-window ownership, an isolated Editor/profile and review of the resulting images remain the caller's responsibility. Offline tests simulate all seven galleries and forwarding paths with synthetic outputs; they establish control flow and cleanup, not rendered appearance.

## Moving and maintaining scripts

Starter and seasonal entries use an optional `capabilities` list: `building_refinement` selects authored building geometry, `building_surfaces` selects authored surface materials/exact slots, `paired_park_lods` requires paired native park LODs, and `reviewed_building` protects reviewed geometry, collision and pivots. An unmarked primitive has none. Unknown, duplicate or malformed values fail before selection. Use `set_capability` when producing or replacing an entry; preserve other capabilities unless that operation replaces them.

The current production entries carry these capabilities explicitly. Existing `task`/`*_task` fields and nested prior entries retain provenance only. Task-only historical reviewed manifests are not automatically upgraded or interpreted as current capabilities; use the current production manifest or explicitly review their contracts. `-BuildingRefinementOnly` and `-BuildingAdoptionOnly` select nonempty capability sets rather than hardcoded historical batch sizes. The current starter sets contain eight refinement and sixty reviewed entries; the old refinement command still expected fourteen and rejected the current manifest. Existing historical report paths and pinned source/approval guards remain unchanged. This change does not authorize rerunning a completed promotion/import.

Use functional areas as scripts are adopted: `Build`, `CI`, `Assets/Blender`, `Assets/Unreal`, `Audio`, `Capture`, `Validation`, `Distribution` and `Maintenance`. Shared modules stay beside their runtime/domain. Establish callable functions and `main()` guards before importing entry points; do not split source text or extract AST prefixes to reuse implementations.

Many root scripts are completed studies, pinned integrations or one-shot promotions. Their filenames alone do not make them supported general-purpose commands. Inspect their hardcoded source hashes, write targets, receipts and native-owner guards before reuse; extract a shared operation when a current workflow needs it. Do not automatically replay or rewrite historical approvals. Move callers and their path resolution together, with focused checks, rather than bulk-moving the directory.

See [asset organization](../docs/ASSET_ORGANIZATION.md) for semantic source/native locations, exact relocation manifests, historical exceptions and the Unreal-aware migration procedure. Numbered editorial/room wrappers are retired; use the functional paths above.

The seven numbered icon/editorial/room compatibility commands have been removed.
Use the canonical semantic commands above. The old-to-new map in
`Assets/Manifests/CityAssetOrganization.json` remains historical evidence.
`CI/Test-CityAutomationLog.ps1` still forwards to the validation implementation.
Historical evidence filenames and generated source issue/version directories
remain unchanged. New automation output/profile names and the `Drone CI`
reservation convention are described in [semantic naming](../docs/SEMANTIC_NAMING.md).

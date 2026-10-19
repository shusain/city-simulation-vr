# D74 adjacent service-result reuse — 2026-10-11

The economy's service result now supplies the immediately following subsystem phase. The normal64 native4K controls reduce mean simulation-step time by10.4% at5x and13.1% at1x;80 research improves12.4% at5x. All three same-executable pairs have identical full authoritative endpoint fingerprints. Normal Windows/Quest integration gates pass; broader D74 performance and the separate D80 save failure remain open.

`FCityEconomySimulation::Advance` has an optional trailing result pointer, overwritten before each synchronous after-step callback. The subsystem moves that result into its initial service snapshot and rebinds the water timing observer. Each catch-up step builds its own fresh result; the forwarding buffer is local to the current Tick. Existing standalone callers retain the default API behavior. Scenario progress and treasury billing change no service inputs; forecasts operate on private population copies. Later care/population/traffic/weather/development/incident refreshes still evaluate current inputs, retaining distinct general and utility staffing rules, finite allocations, rounding and deadlines.

The comparison switch is limited to running generated populated benchmarks. Both parser and runner reject saved-city, synthetic and paused controls; a raw flag alone cannot affect normal play. The older reference-service-query mode also retains its original initial evaluation. Counters prove every step used the requested path. The full-state fingerprint runs after sampling ends using normal save serialization, including exact integer finances. Only the wall-clock accumulator fraction is normalized; fixed-step endpoints must match. Hash equality proves equivalent serialized outcomes, independently of reload validation.

## Normal64 comparisons

Both paths use the same executable, SHA256 `26DFAF14ADF7D9E5AD298ECF2B9945C8BCAB5EB4A60FE46AC4B64AADF01F35C9`, with current D79 production art. Ryzen7 9800X3D/RTX5080; D3D11 offscreen native3840×2160,100% scene scale,4×MSAA,Stats/Day/Clear,493 lots/1036 road cells/5960 residents,128 routing candidates/512 visible research caps. Capture process checks show one benchmark game and no Editor/build/import/Blender process; screenshots are disabled during measurement.

| Scope |1x reference|1x reused|5x reference|5x reused|
|---|---:|---:|---:|---:|
| Capture seconds |96|96|75|75|
| Days |62–102|62–102|100–256|100–256|
| Mean fixed-step ms |43.052|37.393|50.609|45.340|
| Service scopes ms/day |23.235|17.432|21.998|16.845|
| Initial service/scenario ms |5.641|0.070|5.433|0.081|
| Whole simulation tick peak ms |69.837|64.150|81.571|76.594|
| Monthly whole-tick peak ms |67.358|60.943|71.296|65.971|
| Frame mean ms |2.746|2.744|3.030|2.994|
| Frame p95 / p99 ms |3.00 /3.20|3.05 /3.20|3.00 /3.30|3.00 /3.30|
| Worst frame ms |146.504|138.892|155.163|148.794|
| Frames over33.33ms |0.114%|0.114%|0.630%|0.623%|
| Wall time in those frames |2.493%|2.248%|12.703%|11.610%|
| Evaluated / reused initial services |40 /0|0 /40|156 /0|0 /156|
| Maximum steps / catch-up ticks |1 /0|1 /0|1 /0|1 /0|

One service evaluation per day is removed. Other event-driven refreshes remain: tracked subsystem evaluator calls are122→82 at1x and469→313 at5x, plus one economy evaluation per day. Service-scope totals include initial/scenario bookkeeping and occasional extra refreshes; child timers are not added again. `fixedStep` excludes the preceding economy evaluation/forecast. The service total falls25.0% at1x and23.4% at5x.

Full-state SHA1 is `83BDD0D845F302C479EDCCA8FD8532A46EEC6AD6` for both1x paths and `B2C92FA693F58D99037BDB381C9CAF4C2E1A2AE3` for both5x paths. Both clocks, finances, household/employer ledgers, routes, incidents, development and other serialized values match. Six monthly checks at5x/two at1x pass, with no handled ensure. The worst frame still exceeds the measured simulation-tick peak; D74 whole-frame attribution and laptop/PCVR/Quest measurement remain open. These results do not accept sustained30FPS or larger production dimensions.

## Larger research comparison

The guarded80×80 executable, SHA256 `2E62C476D649894547E533C2B52ED2DD8EDAAE86929E94F6B5E08657F03DB333`, retains current D79 art and the same native4K/quality/camera/weather/128-routing/512-visible profile.834 buildings/1600 roads/12020 residents cover85.34% of cells; normal64 covers82.86%. Both75s5x captures cover days100–256/months4–10. The package is benchmark-only, disables city-save writes and is explicitly nonpublishable.

| Scope |Reference|Reused|
|---|---:|---:|
| Mean fixed-step ms |99.981|87.578|
| Service scopes ms/day |49.859|37.358|
| Care ms/day |23.674|23.673|
| Waste ms/day |12.811|12.812|
| Traffic update/advance ms/day |12.242|12.262|
| Whole simulation tick peak ms |164.741|157.311|
| Monthly whole-tick peak ms |142.282|128.344|
| Frame mean ms |4.020|3.893|
| Frame p95 / p99 ms |3.40 /3.75|3.40 /3.75|
| Worst frame ms |277.349|279.515|
| Frames over33.33ms |0.836%|0.810%|
| Wall time in those frames |25.067%|22.421%|
| Evaluated / reused initial services |156 /0|0 /156|
| Maximum steps / catch-up ticks |1 /0|1 /0|

Full-state SHA1 matches `8FB86B7CBCD17C17BF7710C570B40F85832AC592`. Tracked subsystem service calls fall468→312, plus156 economy evaluations in each path. Service work falls25.1%; every later evaluation and all authoritative outcomes remain. Six monthly checks pass in both, peaking0.045ms in the reused path. There is no demonstrated improvement to the worst whole-frame stall: its peak slightly exceeds the control. Additional simulation and whole-frame work remains before D72 can accept a larger production map. The current workstation/offscreen experiment does not establish laptop/XR performance.

## Regression and separate D80 finding

Warnings-as-errors Editor compilation and272-file formatting pass. All230 rendered CitySim regressions pass, including the protected400-day saved-city replay at1x/3x/5x and the month16 settlement guard. The new paired dense regression compares every service semantic field each day, including source budgets/pressure/shortage causes, and full saved state at monthly settlements, edits and periodic checkpoints. It exercises paused road removal/rebuild, illness, weather change, utility demolition/reconstruction, housing/zoning edits, speed changes and multi-step catch-up. Disposable cloned models independently compare forwarded services with freshly evaluated services across three synchronous catch-up callbacks and monthly billing. Both paths reload the same valid pre-edit checkpoint and continue identically.

The extended edited fixture separately exposed a save rejection: yielding emergency crews can be too close to ordinary yielding vehicles, and normal loading reports “Traffic vehicles violate the same-lane safe gap”. The reference and reused paths have identical state, so this is independent of service reuse. The preserved failing reference save, exact lane pairs and normal packaged rejection are in `Artifacts/D74-StepServices/ReferenceRoadEditFailure.json`, `LaneGapDiagnostic.json` and `ReloadDiagnosticRunner.log`; the latter links its full play log. D80 owns the pass/curb-transition versus restore-validation fix. The valid earlier checkpoint does not establish recovery of this later failure. No physical occurrence is reported, and player saves remain protected. Prioritize D80 before the next care optimization and D72 acceptance.

The installed Meta203 startup emits twelve known property-initialization diagnostics, matching the previous suite; project/test failures, unexpected errors and handled ensures are zero. Expected startup diagnostics are retained in logs rather than changing the vendor baseline.

## Reproduction and integration

Use `Tools/Run-DesktopBenchmark.ps1 -Populated -LateGame -Lots 1000 -Speed 5 -Weather Clear -Width 3840 -Height 2160 -CitizenRoutes 128 -VisibleTraffic 512 -DurationSeconds 75 -WarmupSeconds 6 -CaptureStartStep 100`, adding `-ReferenceServiceReuse` for the control. For1x use96 capture seconds,5 seconds warmup and no capture-start gate. For80 use the guarded capacity executable with `-ExpectedGridSize 80 -Lots 6400`. Run serially; restore the normal64 package after research. `Artifacts/D74-StepServices/Compare.ps1` verifies matching executable hashes, profiles, endpoint fingerprints and outcomes; `Reports.json` retains raw reports/metadata. `Summary.json`, test/build logs, formatter checks and process checks retain the exact evidence.

The normal64 Windows archive is restored after research and passes a fresh native4K launch with32 routing candidates/64 visible vehicles,21 reused initial services and zero redundant initial evaluations. The restored executable is302324736bytes, SHA256 `89D4B7C224B09F281DDA0A59A4A993E41603390F0E0D6993E1791DF542EB2955`; it is a separate link from the measured64 archive, with unchanged source/policy.26 cooked3840×2160 synthetic-button/UI/save captures pass against it; City Info is also visually inspected. Windows restore packaging takes55s. Normal Quest packaging takes178.38s; signed embedded ARM64/min32/target35 payload validation passes. APK is190771702bytes, SHA256 `CEDB2DD747421F88B2EA1B2AC97ED62B853BA3D0547D47A02D1436F1983F47EE`.

The protected original city retains SHA256 `50C23550DD3A8A4F07A6C7468A78E96AC65FE56D84000F9B9FEF2DBA4B6B0DA6`; the pre-D79 diagnostic executable retains `CA4F82CCF1C03F7BA36130969A6B52A2E92BB5A5CBCAA1EE4D63BFDAEBEFBDD2`. The isolated D80 reference save is preserved at SHA256 `592D29BA52CB2040303933805B18CDE2939BCAD36D2F7020C2E44850E2273402`. Consolidated checks, exact command/log paths, package identities and limits are in `Artifacts/D74-StepServices/Implementation.json`. No headset installation, publication, commit or push. Physical input/current-code/laptop/XR performance remain pending; the D79 installation remains the headset baseline. Next resolve D80, then care/waste/traffic and the remaining whole-frame tail before D72 production dimensions.

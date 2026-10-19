# Desktop routing CPU slice — 2026-10-10

D74 now reuses immutable weighted road adjacency and a cell-segment-to-edge index. Each request still runs the existing weighted search with access filtering and deterministic row-major ties. No path results or live capacity, congestion, staffing, allocation or deadlines are cached. New builder snapshots replace the data on edits/reset/reload; copies share it safely. Normal gameplay remains64x64 with32 citizen routing candidates,64 visible vehicles and2048 simulated trips.

## Method and evidence

Native3840x2160 offscreen D3D11,100% scene scale,4x MSAA, Stats HUD, fixed Day/Clear weather,128 routing candidates/512 visible vehicles. Generated current-water-rule mixed cities occupy most of each map with actual catalog buildings, connected roads, mature qualifications and real trips. They use generous treasury and disabled zoning growth/disasters; these are controlled late-game stress fixtures, not proof of natural-budget progression. Each capture has5s warmup and75s measured wall time, including monthly settlement. Runs are serial, without concurrent builds, screenshots or another game session.

The64 fixture has493 buildings,1036 road cells,5960 residents and82.86% developed coverage. The80 fixture has834 buildings,1600 roads and12020 residents. Peak actual traffic is970/1071 records at5x. Requested lot limits1000/6400 are not actual building counts. Each pair uses the same executable, fixture, camera, quality and budgets. `-UncachedRouting` disables prepared adjacency and segment lookup while keeping spatial caching; it constructs access-filtered link records per search and uses full edge scans for conversion. Counter checks prove the actual comparison mode after warmup. Normal and saved-city gameplay cannot be disabled by that benchmark flag.

`Artifacts/D74-Routing/Reports.json` links seven validated reports, executable hashes, workloads, clock steps, frame tails, slow-frame counts/time, memory and phases. `BuildSummary.ps1` checks pair identity and counters. The original pre-cache64 profile is supplementary; its98.07ms fixed-step mean closely matches the current97.86ms reference. Earlier preliminary cached captures are supplementary. The first attempted control was rejected because the snapshot predated benchmark activation; it is excluded. Mode binding and a regression now cover activation without a geometry change.

## Results

| Grid / mode | FPS | p99 | Worst frame | Actual clock | Frames over33.33ms | Time in those frames |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
|64 reference5x|298.9|3.20ms|223.1ms|5.02x|157|24.74% /18.56s|
|64 cached5x|319.4|3.25ms|202.6ms|5.02x|157|19.77% /14.84s|
|64 cached1x|381.5|3.00ms|154.2ms|0.99x|31|3.63% /2.72s|
|80 reference5x|174.4|204.75ms|435.7ms|4.99x|156|51.10% /38.33s|
|80 cached5x|204.8|175.00ms|360.7ms|4.99x|156|42.44% /31.83s|
|80 cached1x|327.4|3.35ms|299.8ms|0.99x|31|7.56% /5.67s|

Clock estimates use completed steps times2.4 game seconds divided by capture duration; the finite boundary accounts for estimates slightly above/below requested speed. Both64/5x modes complete157 steps; both80/5x modes complete156. High average FPS does not erase the pauses: the number of slow frames stays the same while their cost falls. These are app frames, not compositor telemetry or a headset/laptop budget.

| Grid,5x | Fixed-step reference → cached | Scheduling/traffic | Live services per refresh | Needs per refresh |
| --- | ---: | ---: | ---: | ---: |
|64|97.86 →74.02ms|44.70 →21.70ms|11.97 →12.02ms|3.55 →3.57ms|
|80|201.57 →159.60ms|78.81 →38.18ms|26.04 →26.07ms|10.57 →10.80ms|

Inclusive per-call scopes are nested, not additive independent totals. Scheduling includes care/waste/emergencies/freight/citizen updates, traffic update/advance and completions. Prepared search counts are45864/54282 with zero reference builds; controls have exactly those reference-build counts and zero prepared searches/indexed segments. Cached conversion performs636400/770173 indexed segment lookups. Preparation reference means0.0635/0.0950ms become a sub0.001ms pointer lookup; conversion means0.0218/0.0302ms become0.0016/0.0020ms. Weighted search remains live. Capture counters are authoritative for call counts; sub-clock-resolution timing samples may be omitted.

## Verification and decision

All222 rendered regressions pass, including all16 access masks, exact weighted paths/vehicle edges/directions/portal distances, raised highway/ground separation, road upgrades/disconnection, empty state/reload, copied-snapshot isolation and late benchmark activation. Warnings-as-errors Editor compilation,265-file formatting and helper syntax checks pass. The protected D78 city still reaches96 buildings from72,920 residents and$32245 identically at1x/3x/5x over400 game days; private placements cost zero and its SHA256 remains unchanged. Population recovery still requires repairing existing water/damage outages. Test/evidence logs are under `Artifacts/D74-Routing`; final integration package identities and cooked UI checks are recorded in CHANGELOG and `Implementation.json`.

Keep64 as the normal playable baseline.80 remains a stronger research candidate, but0.30–0.36s pauses require further CPU work and physical/laptop review before a production option.96/128 are not remeasured after this slice; previous results remain dated evidence. D72 still owns versioned per-city dimensions, compatibility and platform policy.

Next D74 work: attribute changing-state service allocation and repeated lookup/copy costs, and reduce redundant needs/service calculation only with explicit invalidation for actual staffing, weather/output, demand, operation, construction and edits. Service and needs costs remain essentially unchanged in the paired runs. Also separate remaining care/waste/scheduling work from weighted search. Preserve immediate paused edits, actual clock/deadlines, finite supply/shortage causes, public payroll/qualifications, reservations, freight/care/custody/waste and emergency priority. Shared-code Android integration does not establish fresh physical headset behavior. No public publication or headset installation is part of this slice.

Reproduce with `Tools/Run-DesktopBenchmark.ps1` parameters above, plus `-UncachedRouting` for the reference. Use `Build-PCVRSandbox.ps1 -CapacityGridSize 80` for guarded research and restore the normal64 package afterward. `-SkipEditorCompilation` is available only after a current verified Editor build; game compilation/cook/archive still run. ENVIRONMENT documents commands and observer/counter scope.

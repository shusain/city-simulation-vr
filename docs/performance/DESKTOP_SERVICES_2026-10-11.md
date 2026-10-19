# Desktop service evaluation CPU slice - 2026-10-11

D74 now prepares reusable values within each water request and education evaluation. Water computes frontage and each available source's pressure once, then sorting and delivery reuse them. It rereads source operation and remaining finite budgets for every request. Education prepares an ordered nearby-school list once per home within the current evaluation; each citizen still consumes current free places in the original school order with unchanged subject qualifications. No allocation, payroll, output, deadline or growth policy is cached across evaluations. Normal gameplay remains64x64 with32 routing candidates,64 visible vehicles and2048 simulated trips.

## Attribution and method

The initial80 profile after routing caching measured12.71ms consumer allocation,7.95ms education,2.71ms public staffing,1.55ms providers and1.62ms water-network assembly per service call. Needs market calculations took11.12ms, versus0.04ms zone checks and0.11ms desirability. This justified targeting repeated water pressure and per-citizen school access first. `Artifacts/D74-Evaluation/Attribution.json` retains the original profile and executable identity; it is supplementary to the same-executable comparisons below.

Six final captures use native3840x2160 offscreen D3D11,100% scene scale,4xMSAA, fixed Day/Clear weather and Stats HUD, with128 routing candidates/512 visible vehicles. Each runs75s, serially without builds/screenshots/another game session. Warmup is6s for the final64/5x pair and5s for the other captures. The initial64/5x pair completed156 versus157 steps at its75s boundary; those captures remain supplementary and are excluded from the matched final comparison. Current-water-rule late-game fixtures have real mixed catalog lots, roads, mature qualifications and operational traffic:64 has493 buildings/1036 roads/5960 residents;80 has834/1600/12020. Generous funds and disabled zoning growth/disasters make these controlled stress fixtures, not natural-budget progression tests.

`-ReferenceServices` selects the original repeated water-pressure and school-access calculations, retaining spatial and routing caches. The runner verifies the actual mode and exclusive counters. Normal play and saved-city profiling cannot be disabled by that command-line flag. Each prepared/reference5x pair uses the same executable, fixture, camera, quality, budgets, weather and clock phase. The summary checks start/end steps/months, residents/lots, road count and final power/water demand/capacity and operational home/shop totals. Exact allocation/student identities and shortage behavior are covered by equivalence regressions. `Reports.json` links all six reports and their metadata; `BuildSummary.ps1` rejects mismatched pairs.

## Results

| Grid / mode | FPS | p99 | Worst frame | Actual clock | Time in frames over33.33ms |
| --- | ---: | ---: | ---: | ---: | ---: |
|Reference64-5x|323.5|3.20ms|188.3ms|5.02x|18.40% / 13.81s|
|Prepared64-5x|337.4|3.65ms|163.1ms|5.02x|14.28% / 10.72s|
|Prepared64-1x|385.0|3.00ms|129.4ms|0.99x|2.54% / 1.91s|
|Reference80-5x|217.0|3.80ms|347.6ms|4.99x|39.23% / 29.42s|
|Prepared80-5x|245.8|3.60ms|311.4ms|4.99x|30.92% / 23.19s|
|Prepared80-1x|335.9|3.35ms|240.2ms|0.99x|5.26% / 3.95s|

These are wall-clock app frames, not headset compositor or laptop telemetry. A high average or low p99 can coexist with substantial pauses: once slow simulation frames represent less than1% of frames, p99 mostly describes ordinary rendered frames. Use worst frames and the fraction of elapsed time spent over budget as well. Clock estimates use completed steps times2.4 seconds divided by measured capture time; finite capture boundaries explain small deviations from requested speed.

| Grid,5x | Fixed step reference -> prepared | Derived service refresh | Consumer allocation | Education | Needs market |
| --- | ---: | ---: | ---: | ---: | ---: |
|64|74.20 -> 54.43ms|12.02 -> 5.50ms|6.57 -> 2.02ms|3.02 -> 1.06ms|3.46 -> 3.44ms|
|80|158.53 -> 118.31ms|25.72 -> 12.39ms|12.41 -> 4.26ms|7.83 -> 2.21ms|10.69 -> 10.67ms|

Scopes are inclusive and nested. Service subphases include initial and derived service evaluations, while `serviceRefresh` counts derived calls only. Each fixed step normally performs three service evaluations in this fixture. The demand market/forecast and remaining scheduling work retain their existing rules and remain CPU targets; this slice does not claim to eliminate them.

| Grid,5x | Prepared water calls | Reference water calls | Prepared home-school lists | Reference citizen school loops |
| --- | ---: | ---: | ---: | ---: |
|64|193912|387824|107073|1565784|
|80|338230|676460|221558|3200952|

Water counters count public evaluator calls, including nested reference requests; education counters use different units (home-list preparations versus eligible housed citizens entering the original loop). They prove the active paths, not interchangeable per-call speed measurements. Each mode reports zero counters for the opposite path. Observer counters/timing reset after warmup and rebind the current snapshot when the timing owner resets. Normal play performs no timing clock reads.

## Validation and limits

All224 rendered CitySim regressions pass, including two new prepared/reference equivalence tests. Water covers current/legacy rules, reversed source ordering, pressure thresholds, exhausted supply, outages/recovery, zero/empty sources, repeated finite allocation and physical-head/shortage diagnostics. Education compares student identities, finite school usage, home places and full coverage through road disconnection/reconnection, qualification changes, staffing/outages, limited capacity and unusable housing. Existing activation checks also prove saved/normal gameplay immunity to the reference flag and water-observer rebinding on capture reset.

Warnings-as-errors Editor compilation,267-file formatting and helper parsing pass. The initial compile failure was confined to the new test fixture's missing population include and temporary mutable preview; corrected code passes the final build/suite. The protected D78 city still adds24 private buildings to reach96 over400 game days identically at1x/3x/5x, with zero new private construction spending. Existing water/damage outages keep population at920; restoring actual service remains necessary. The protected input hash remains unchanged and no player save is written.

Normal64 Windows is restored after80 research, and Windows/Quest packaging, APK validation and final native4K cooked UI/save evidence are recorded in CHANGELOG and `Artifacts/D74-Evaluation/Implementation.json`. These gates establish local integration, not fresh physical headset/input acceptance. No release/source push or headset install is performed.

Keep64 as the playable default.80 is a research candidate; remaining pauses and laptop/physical review still block production enlargement.96/128 are not remeasured after this slice. Next D74 work should attribute the demand forecast's copy/reconciliation/payroll loops and remaining scheduling costs, then replace repeated lookups with scoped indexes or narrower invalidation while preserving exact financial rounding, reservations, qualified staffing, finite supply and deadlines. D72 still owns versioned per-city dimensions and compatibility; D67/D23 own headset measurements.

Reproduce64/5x with `Tools/Run-DesktopBenchmark.ps1 -Lots 1000 -Populated -LateGame -Speed 5 -Weather Clear -Width 3840 -Height 2160 -CitizenRoutes 128 -VisibleTraffic 512 -DurationSeconds 75 -WarmupSeconds 6`, adding `-ReferenceServices` for the control. Build80 with the guarded capacity helper and use `-ExpectedGridSize 80 -Lots 6400 -WarmupSeconds 5 -ExecutablePath <research executable>`;1x also uses5s warmup. Restore a normal64 package afterward. ENVIRONMENT documents the flags, counter units and integration commands.

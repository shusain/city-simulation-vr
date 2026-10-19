# Desktop CPU optimization and dense-city capacity — 2026-10-10

This follows the user's acceptance of native desktop resolution and request to optimize CPU work, then measure larger maps using densely developed late-game cities. Measurements use the Ryzen7 9800X3D/RTX5080 workstation, Windows11, packaged Development/D3D11 at3840x2160,100% scene scale, secondary scale100%,4xMSAA, daytime/Clear, Stats panel open, audio off, VSync/frame cap off. They measure offscreen workstation throughput, not physical display refresh, laptop or headset performance.

## Completed CPU changes

- Reuse derived services/needs state on unchanged render-only frames. Explicit actions still refresh immediately, and fixed simulation steps still update authority. The invalidation key covers placement owner/revision, city session, presentation changes, funds and fixed step. The second slice below removes the raw station half-day key; actual completion invalidates through presentation generation while the station clock continues normally.
- Rebuild/reconcile the road graph only when the placement owner/revision changes. Reset/load/benchmark replacement invalidate the owner.
- Replace the weighted route search's linear frontier scan with a binary heap, preserving weighted costs, access masks, road levels and deterministic ties.
- Reuse terrain/water mesh buffers through building-only presentation rebuilds; new terrain parameters or city sessions rebuild them. Existing per-frame weather/rotor presentation remains.

The normal game remains64x64 with32 citizen routing candidates,64 visible slots and2048 simulated trips. No economic/spawn rules, production save schema or Quest quality budgets changed.

## Same-day paired CPU evidence

The basic fixture has744 buildings,3200 residents and883 road cells. Both executables use the same generated scene and75-second captures after5-second warmup, including settlement. These comparisons isolate the CPU changes; the newer mixed/current-water fixture below serves a separate capacity question.

| Profile | Before FPS | After FPS | Before/after worst frame | Before/after slow time |
| --- | ---: | ---: | --- | --- |
|1x,32 route /64 visible|70.3|432.5|211.9 /76.3ms|5.04 /2.73%|
|5x,512 route /512 visible|30.1|288.3|968.3 /290.6ms|59.79 /30.06%|

At1x, both execute31 fixed steps and peak at139 traffic records. Derived refresh calls fall5338→93; fixed-step mean falls96.90→51.97ms. At5x, completed steps rise136→157 as fewer long hitches reduce Unreal's time-clamping slowdown; peak traffic rises777→884. These are matched requested profiles, not identical completed work. Actual clocks/trips are recorded. Rare long frames still matter despite very high average FPS and low p99.

CPU raw reports live under `Artifacts/Benchmarks/`:

- Before1x: `Desktop-20261010-120840-744-Stats-Day-3840x2160-Neutral-TabletopShadows`.
- Before5x: `Desktop-20261010-121011-744-Stats-Day-3840x2160-Neutral-TabletopShadows`.
- After1x: `Desktop-20261010-123456-744-Stats-Day-3840x2160-Neutral-TabletopShadows`.
- After5x: `Desktop-20261010-123626-744-Stats-Day-3840x2160-Neutral-TabletopShadows`.

Before executable SHA256: `782D7BAC85D4AB58DA1A65587B2D4D6D1BC9694F0B52EB839F33F40ADEB8697E`. After CPU comparison executable: `B12D55FD9BA6607951D22362F520AC53EAA06C13A63FE92CAA59A68FA80705CB`. Subsequent fixture/report changes preserve the optimized gameplay paths; each capacity run records its own executable hash.

## Dense full-grid method

The mixed fixture uses real catalog footprints, connected local streets/avenues, medium/high homes, shops and factories, fully upgraded power/water utilities, schools, police/fire, hospital, waste/recycling and parks. It covers at least75% of cells with roads/buildings and requires at least75% of homes/shops operational. Current water-pressure rules are enabled. Real authority creates residents, jobs and trips. Mature qualifications, completed opening deliveries,60 bootstrap steps, generous treasury and disabled growth/disasters are declared inputs. This stresses an operating mature city; it does not prove natural-budget progression or congestion balance.

Each main run captures75 wall-clock seconds after5-second warmup and includes a monthly settlement. Main capacity comparisons use128 route candidates/512 visible slots, with the normal2048 simulated-trip limit. Traffic records are combined trips/service vehicles, not distinct cars or rendered-instance counts. The whole board is framed at its usual overview size; larger grids contain more, smaller cells in that view. Close zoom, storms, new disasters, changed road layouts and future richer assets need separate measurements.

FPS and frame intervals use a monotonic wall clock; the runner verifies frame sums against capture duration. Slow time is elapsed time belonging to frames longer than33.33ms. Monthly timing is the entire tick containing settlement, not settlement-only time. CPU phases are inclusive/nested and must not be summed. Actual start/end steps disclose slowdown due to clamping during hitches.

Larger grids use isolated compiled-size research executables, not a production size selector. Only Windows Development game code receives the override. Editor/Shipping/Android remain64; research startup requires isolated benchmark arguments, city-save writes are disabled, and tester export rejects `CapacityBuild.json`. The build helper forces ModuleRules re-evaluation with `-NoUBTMakefiles` to prevent stale dimensions. This is the documented D72 research deviation; per-city versioned dimensions and compatibility remain required for player-facing larger maps.

## Dense results and practical limits

All rows below use the functioning current-water mixed fixture. Road/building coverage is82.9–85.3%; populated counts are measured, not inferred from cell counts. These layouts contain different housing/civic proportions, so growth in work is not exactly proportional to map area.

| Grid | Buildings / residents | Speed | Route / visible | FPS | p99 ms | Worst ms | Slow time % | Actual simulation rate |
| --- | --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
|64x64|493 /5960|1x|128 /512|357.4|3.0|300.8|10.7|0.99x|
|64x64|493 /5960|5x|128 /512|145.1|230.6|388.3|64.9|4.99x|
|80x80|834 /12020|1x|128 /512|283.1|3.4|529.4|20.6|0.99x|
|80x80|834 /12020|3x|128 /512|95.6|407.0|770.2|73.7|2.67x|
|80x80|834 /12020|5x|128 /512|2.87|738.5|784.2|100|4.02x|
|80x80|834 /12020|5x|32 /64|2.93|754.1|781.2|100|4.11x|
|96x96|1183 /16900|1x|128 /512|206.5|4.0|799.8|32.7|0.86x|
|96x96|1183 /16900|5x|128 /512|1.33|1375.5|1375.5|100|2.31x|
|96x96|1183 /16900|5x|32 /64|1.31|1344.9|1344.9|100|2.28x|
|128x128|1991 /22800|1x|128 /512|98.6|4.5|2409.0|63.7|0.56x|
|128x128|1991 /22800|5x|128 /512|0.48|2743.3|2743.3|100|0.97x|

Actual simulation rate is completed steps ×2.4 seconds divided by captured wall time. It exposes loss of requested speed under hitches and bounded/clamped catch-up. The99th percentile can miss rare long stalls when many short frames occur between them; worst frame and slow-time share show the responsiveness cost.

The80/96 normal traffic controls do not rescue5x: both normal and expanded traffic collapse below30 FPS.80 holds about1x at ordinary speed, but the3x setting achieves only2.67x despite averaging96 FPS, with770ms stalls.96's1x average remains high but approximately800ms pauses are already significant.128 shows2.4-second stalls and simulation slowdown even at1x, despite averaging99 FPS. No larger-grid search is justified before reducing these bursts.

**Initial matrix decision (before the second slice below):** Keep64 as the current playable baseline.80 is a modest ordinary-speed prototype candidate (56% more cells), requiring CPU burst improvements and hands-on review before it becomes a normal option. No tested larger size meets the30-FPS5x goal in this initial matrix. The observed fast-forward failure bracket is64→80, not an exact universal maximum;72 and other layouts/hardware remain unmeasured. A3x option is less severe than5x at80, but is not full-speed or free of long stalls. Production dimensions and compatibility remain D72 after the remaining D74 CPU work.

Per service refresh, mean cost grows28.9→105.9→298.3ms at64/96/128. Mean fixed steps grow179.6→628.3→1996.8ms at1x. The fixed-step phase includes nested work. Render thread averages about1.5–1.7ms in these1x rows; measured game-thread phases establish the CPU problem. Whole-frame RHI GPU telemetry can include waiting and does not attribute all long-frame time to shaders.

Next investigate stable water/topology/provider search reuse, duplicate service passes after weather, station half-day invalidation and the remaining routing/education/waste/settlement work. Separate stable spatial data from live staffing, capacity, allocation and emergency state; do not discard authoritative steps or change economic/population caps to make a benchmark faster.

## Integration and remaining checks

Final warnings-as-errors Editor build passes101.63s;210 rendered regressions succeed with0 failures/210 performed/normal exit. The four new checks cover paused/fixed-step invalidation, weighted deterministic routing, actual terrain-buffer invalidation and dense mixed-footprint/frontage coverage.253-file formatting, script parsing and scoped Git whitespace checks pass. The actual96 research package is rejected by tester export before creating a destination.

Normal64 Windows packaging passes54.71s; Android ARM64/Vulkan/ASTC packaging passes99.28s. Native3840x2160 cooked Desktop UI/save/graphics/panel checks pass with20 captures; Overview was visually reviewed. The restored normal package's separate10-second populated report verifies64x64, normal32/64 budgets and its executable hash; it is restoration proof rather than a monthly comparison. Logs are `Artifacts/D68-D72-Final-EditorBuild.log`, `D68-D72-Final-Tests.log`, `D68-D72-Final-Tests-Summary.json`, `D68-D72-Final-Format-Check.log`, `D68-D72-Final-Script-Checks.json`, `D68-D72-Restore64-Windows-Package.log`, `D68-D72-Final-Android-Package.log`, `D68-D72-Final-UI.log` and `D68-D72-Restored64-Proof.log`. UI evidence is under `Artifacts/D68-D72-CPU-Final`.

Final normal Windows executable302025216 bytes/SHA256 `79AA58452BB1AA0D63FCF8D7EED6F674538F7011935CE350D68EA26EA98D14DC`; APK186450138 bytes/SHA256 `AB299C285EB34E76A8F0618A0EB98EB49957F5B0D5453468A327F6BA0B7F2DB5`. Identities are in `Artifacts/D68-D72-Final-Package-Identities.json`. Launch the normal local build with `Tools/Start-DesktopSandbox.ps1`. Source working-tree changes are preserved and remain uncommitted/unpushed; GitHub remains `playtest-2026.10.09.2`.

User acceptance covers native-resolution appearance and previous PC smoothness. New current-code physical desktop, laptop, PCVR/stereo/refresh and Quest device checks remain pending; no XR launch or Quest install occurred. Current maps/saves and traffic defaults are unchanged. Richer D71 meshes, storms, close zoom, disasters, natural-budget progression and other city layouts can move the measured limits.

## Reproduction

See [ENVIRONMENT.md](../ENVIRONMENT.md#dense-late-game-and-isolated-larger-grid-capacity-measurements) for the normal fixture, experimental build/run commands and restoration of the64 target. Use75 seconds for monthly comparisons, and capture screenshots in separate short runs. Preserve user saves and run serially with no concurrent builds.

`Artifacts/D68-CPU-Final-Paired-Summary.json` links the four paired CPU reports. `Artifacts/D72-Dense-Final-Summary.json` links all11 full current-water captures, metadata/executable hashes, family counts, operational homes/shops, power/water capacity, traffic, clocks, memory and CPU phases. Full-grid64/128 visual checks are separate10-second captures: `Desktop-20261010-130031-Late-64-1000-1-Stats-Day-3840x2160-Neutral-TabletopShadows` and `Desktop-20261010-132026-Late-128-16384-1-Stats-Day-3840x2160-Neutral-TabletopShadows` under `Artifacts/Benchmarks`.

Preliminary legacy-water mixed64 captures, inadequate-service/current-water fixture iterations, the stale96 build/run and the first genuine96 compile failure are excluded. The stale build was detected by an invalid requested-lot range; the research save guard's unreachable-code warning was corrected with a preprocessor else branch. Neither failure establishes96-grid performance. Earlier pre-terrain-cache/short screenshot runs remain supplementary. No player save, public release or installed headset is changed by these measurements.

## Second CPU slice and release decision (before spatial caching)

Removed a service evaluation immediately discarded by the post-weather derived refresh. Also removed raw half-day clock changes from the derived-state cache key: station construction/upgrade completion still runs at half-day cadence and explicitly invalidates real state changes. An extended regression exercises an empty half-day, a full fixed step and actual between-step station completion. All210 rendered regressions pass; warnings-as-errors Editor compilation and253-file formatting pass. Simulation/economy rules and traffic defaults remain unchanged.

Repeated the same native4K/current-water mixed fixtures with75-second wall-clock captures,5-second warmup and real monthly settlement. Each profile below retains its layout, residents, camera and quality. Fast-forward cases perform different amounts of actual work as throughput improves; the summary records start/end steps and actual clocks rather than claiming identical completed simulation.

| Grid/profile | Before/after FPS | Before/after p99 | Before/after worst frame | Before/after actual game speed | Before/after fixed-step mean |
| --- | --- | --- | --- | --- | --- |
|64,5x,128 routes/512 visible;493 buildings/5960 residents|145.1 /202.5|230.6 /167.5ms|388.3 /345.7ms|4.99 /4.99x|210.0 /178.9ms|
|80,1x,128/512;834 buildings/12020 residents|283.1 /301.1|3.40 /3.45ms|529.4 /468.0ms|0.99 /0.99x|339.2 /278.2ms|
|80,5x,128/512; same city|2.87 /46.58|738.5 /592.1ms|784.2 /707.1ms|4.02 /4.46x|407.4 /348.0ms|
|80,5x,32/64; same city|2.93 /46.09|754.1 /579.7ms|781.2 /699.3ms|4.11 /4.58x|395.0 /335.7ms|

At64/5x the same156 steps complete, derived refreshes fall515→358 and slow-frame time falls64.88→49.15%. At80/1x the same31 steps complete, refreshes fall93→62 and slow-frame time falls20.61→14.60%. At80/5x expanded, completed steps rise126→140 and slow-frame time falls100→86.95%; the normal-budget control rises129→143 steps with87.51% slow time after the change. These time percentages include the entire duration of long frames and expose pauses obscured by average FPS.

**Decision at the second slice:** This focused performance/capacity round is complete. Ship the normal64x64 Windows/Quest baseline with the CPU improvements.80 now exceeds30 average FPS even at5x, but still misses the requested clock speed and has roughly0.7-second pauses. It remains a research/prototype candidate; a smooth larger-map option requires further D74 burst work and D72 versioned per-city dimensions/save compatibility.96/128 were not repeated after this second slice; their earlier results above remain dated baseline evidence, not fresh current-code measurements. No exact universal maximum, laptop result or headset frame budget is established.

Raw before/after reports, metadata/executable hashes, actual clocks, coverage, population and traffic are in `Artifacts/D74-Second-Paired-Summary.json`. After captures are `Desktop-20261010-142555-Late-80-6400-1`, `142746-Late-80-6400-5`, `143017-Late-80-6400-5` and `143302-Late-64-1000-5`, each with the suffix `-Stats-Day-3840x2160-Neutral-TabletopShadows`. The final normal64 build is restored before release. Package/device/publication evidence belongs to CHANGELOG; physical current-code Desktop/PCVR/Quest, external-machine and laptop checks remain pending.
## Third CPU slice: event-driven spatial caching

City-owned geometry memoization now serves building frontage, water-source road distances, union floods and civic/provider reach across all service evaluations, including economy calls. Road-access checks use each validated building footprint rather than scanning every map cell to rediscover it. Repeated render-only derived work and the weighted heap search remain from the earlier slices.

Road creation/removal/class upgrades, raised-road class rebuilding, building creation/demolition and terrain replacement explicitly clear geometry. Natural growth uses the same placement mutator. Reload/reset create a fresh owner, preventing reuse across cities with matching saved revisions. Funds and zoning changes reuse unchanged geometry while authoritative snapshots/actions still update. Capacity, staffing, weather, source operation, pressure output, demand, finite allocation and failure causes remain live; no water allocation is memoized. Canonical start cells, hop budget and connected-road policy identify reach queries; frontage keys include anchor, rotated size and frontage policy. Source-set/range changes choose the appropriate new key.

Limits are bounded and scale with cells: frontage entries at4×cell count, reach entries at cell count/8 and total stored reach distances at32×cell count. The initial128-entry/16×distance limit caused repeated evictions at80:64,384 reach builds during the preliminary5x capture. Corrected limits give **zero frontage/reach builds and zero geometry invalidations in all ten final static captures**, with nonzero cache hits. The undersized and earlier64 captures remain in `Artifacts/D74-Spatial-Preliminary-Reports.json`; the interrupted80 run is excluded. The final table uses only the post-sizing packages.

All captures retain the workstation/quality/fixture methodology above:75-second wall-clock capture,5-second warmup, current water rules,128 candidate routes/512 visible slots and real settlements/traffic. Layouts are64:493 buildings/5960 residents/82.86% developed;80:834/12020/85.34%;96:1183/16900/84.78%;128:1991/22800/84.38%. Fast-forward throughput changes the number of completed steps; raw reports record clocks, populations, service outcomes and traffic rather than claiming identical ending states. Fixed-step quantization accounts for small deviations near the requested1/3/5x clocks.

| Grid / speed | FPS | p99 ms | Worst ms | Actual speed | Slow frames % | Slow-frame time % |
| --- | --- | --- | --- | --- | --- | --- |
|64 /1x|376.6|3.1|165.2|0.992x|0.11|4.39|
|64 /3x|311.9|3.2|245.4|3.008x|0.40|20.61|
|64 /5x|271.0|9.8|243.2|5.021x|0.77|31.86|
|80 /1x|323.5|3.4|324.4|0.992x|0.13|8.66|
|80 /3x|203.8|4.0|530.0|3.008x|0.62|41.53|
|80 /5x|117.0|297.2|543.5|4.990x|1.78|66.32|
|96 /1x|252.9|3.9|574.0|0.992x|0.16|16.69|
|96 /5x|11.0|865.2|958.8|3.396x|12.89|96.09|
|128 /1x|158.8|4.3|1490.4|0.730x|0.19|41.67|
|128 /5x|9.3|2167.1|2167.1|1.486x|7.48|96.82|

Compared with the second-slice64/5x result, FPS improves202.5→271.0, service evaluation27.7→12.0ms, fixed-step178.9→111.1ms, p99167.5→9.8ms and worst345.7→243.2ms; slow-frame time falls49.15→31.86%. At80/5x, FPS46.6→117.0 and actual clock4.46→4.99x; service55.9→26.8ms, fixed-step348.0→233.6ms, p99592.1→297.2ms and worst707.1→543.5ms. Slow-frame time still totals66.32%, so a high average does not establish smooth interaction.

The matching **same-executable**80/5x `-UncachedSpatial` control averages49.3 FPS/4.512x actual speed, service53.7ms/fixed-step342.0ms and worst699.4ms. It performs27,596,366 frontage builds and86,103 reach builds with zero cache hits. The enabled run performs94,475 reach hits with zero builds. The control retains the new footprint road-access algorithm; this toggle isolates geometry memoization rather than reverting all optimizations.

| Grid,5x | Fixed-step mean | Live service mean | Scheduling/traffic mean | Reach cache hits |
| --- | --- | --- | --- | --- |
|64|111.1ms|12.0ms|51.0ms|70286|
|80|233.6ms|26.8ms|87.8ms|94475|
|96|455.2ms|53.2ms|136.4ms|102632|
|128|1234.6ms|157.3ms|485.1ms|87600|

These are inclusive per-call scopes, not additive independent means. Fixed-step includes derived calls and other phases; service is measured within derived refresh as well as the separate initial service/scenario scope. Scheduling includes hospital/waste/emergency/freight/citizen updates, traffic routing/advancement and completions. It is **not pure route search**. Monthly timing includes the entire settling tick. At128/5x, p99 reaches the histogram overflow fallback and equals the measured peak; do not infer finer tail precision beyond that measurement.

**Current decision:** Normal gameplay, Editor, Shipping, Android, saves and tester defaults remain64x64/32 routing candidates/64 visible/2048 simulated trips.80 now keeps pace at all tested speeds on this workstation and is a stronger D72 research candidate, but its0.54s worst pause needs further work and physical/laptop review.96 maintains1x but falls below30 average FPS and loses5x clock speed;128 loses clock speed even at1x and has1.5–2.2s pauses. Larger per-city dimensions/save compatibility remain unimplemented. No universal maximum or headset/laptop frame budget is established.

The next D74 slice should attribute route preparation, repeated entity/citizen lookups and changing-state allocation inside services/scheduling before choosing targeted caches or bounded nonurgent work. Cache-copy costs may also deserve measurement. Preserve finite supply, public payroll/qualifications, emergency priority, reservations, freight/care/custody/waste and immediate paused edits. The queued gameplay sequence remains D76 private funding, D75 faster growth, then D77 density demand.

Verification: final warnings-as-errors Editor compilation and212/212 rendered regressions pass, including full-grid road-access equivalence after rotated placement/disconnection/highway upgrade/load and cached-versus-uncached water allocation/outage/pressure/legacy-range checks. Final Windows/Android package and cooked UI evidence are recorded in CHANGELOG. No current-code physical headset/Desktop or laptop result is claimed. `Artifacts/D74-Spatial-Reports.json` links all eleven validated reports/metadata/executable hashes and detailed phases; normal64 is restored after research. The public `.10.1` release remains the earlier verified package.

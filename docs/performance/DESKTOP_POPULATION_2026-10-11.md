# Desktop population and forecast CPU slice - 2026-10-11

D74 prepares ordered vacancy searches and payroll membership within each reconciliation/settlement. Civic searches have separate cursors per qualification; retained workers consume slots first. Skilled searches only advance after retention, and entry searches start after skilled transfers can release entry slots. Housing uses the same ordered first-available rule; a local membership set avoids rediscovering already-created households. Employer-to-citizen lists preserve citizen order for public wage remainders and private proportional rounding. These indexes expire with each call. Eligibility, leave, detention, wallets, finite slots, reservations and financial calculations remain live. Forecast copies remain isolated; authoritative reconciliation and settlement share the same preparation. No cadence, growth, price, save-schema, map-size or traffic-budget change is made.

## Attribution and method

The initial80 profile found forecast copying around0.10ms, reconciliation6.52ms and projected settlement3.72ms per needs forecast. Public/private payroll took2.54/0.81ms inside settlement. Care averaged5.34ms per call across three passes; waste6.72ms across two. Freight queuing was0.003ms, citizen scheduling0.78ms and emergency dispatch0.12ms. This supported targeting forecast hiring/household searches and payroll first while retaining care/waste as scheduling targets.

Expanded economy timers reveal another service evaluation and forecast before the subsystem's `fixedStep` scope. The prior service report's three service/two forecast counts describe that subsystem scope, not the complete economy-plus-subsystem tick. Current fixtures perform four service evaluations and three forecasts per day. `economyBilling` isolates treasury billing; it does not include all other monthly work. `maximumStepsPerTick`/`catchUpTicks` distinguish single-step cost from catch-up bursts.

Six final75s native3840x2160 offscreen D3D11 captures use100% scene scale,4xMSAA, Stats HUD, Day/Clear,128 routing candidates and512 visible vehicles. Comparisons run serially without builds/screenshots/another game; the user's other session authors Blender assets, and this slice changes no art source/content. The64 fixture has493 mixed buildings/1036 roads/5960 residents;80 has834/1600/12020. Generous funds, absent zoning growth and disabled disasters make these controlled operational stress tests, not ordinary-budget progression or laptop/headset acceptance. Normal gameplay retains64x64/32 candidates/64 visible/2048 simulated trips.

`-ReferencePopulation` selects beginning-of-list vacancy searches and repeated payroll scans in the same executable. Water/education/spatial/routing preparation remains enabled in both modes. Normal/saved-city execution is immune to the raw comparison flag. Exclusive counters prove both needs and economy forecast modes. Each5x pair matches executable hash, actual starting/ending steps/months, residents/lots, roads, service outputs and declared quality/budgets/weather. The actual clock is recorded, not inferred from the selected speed.

An initial80 pair started on days72/73 and is excluded from paired results (`SupplementaryReports.json`/`Initial-*` logs). An initial64 launch requested4096 lots and was rejected by its1000-lot guard; it produced no timing result. The final runner uses1000/6400 requested lots, and `-CaptureStartStep 100` with6s minimum warmup gates5x captures on the same simulation-day boundary. The runner rejects a missed start; the comparison rejects unequal end phases. Additional1x captures use5s warmup without the boundary gate, and cover settlement. They are separate speed scenarios, not exact-phase1x/5x pairs. No gameplay timer is changed to align captures.

## Results

| Case | FPS | Fixed-step mean | Needs market per refresh | Worst frame | Time in frames over33.33ms | Actual clock | Capture steps |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
|80-Prepared5x|266.33|99.18ms|1.50ms|274.31ms|24.84%|4.99x|100->256|
|80-Reference5x|235.41|128.60ms|12.39ms|320.67ms|33.24%|4.99x|100->256|
|80-Prepared1x|339.39|72.89ms|1.43ms|210.23ms|3.89%|0.99x|62->93|
|64-Prepared5x|345.32|52.38ms|0.77ms|321.15ms|13.08%|4.99x|100->256|
|64-Reference5x|336.04|60.19ms|3.90ms|271.35ms|15.37%|4.99x|100->256|
|64-Prepared1x|387.59|40.17ms|0.71ms|120.06ms|2.14%|0.99x|62->93|

| Case,5x | Forecast hiring | Housing/prune/balances | Public payroll | Private payroll | Population copy | Economy forecast reconciliation | Prepared/reference forecasts |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
|80-Prepared5x|0.19ms|0.31ms|0.07ms|0.12ms|0.10ms|0.80ms|468/0|
|80-Reference5x|7.12ms|1.05ms|2.65ms|0.74ms|0.10ms|8.45ms|0/468|
|64-Prepared5x|0.12ms|0.16ms|0.04ms|0.07ms|0.03ms|0.41ms|469/0|
|64-Reference5x|2.07ms|0.36ms|0.76ms|0.32ms|0.03ms|2.59ms|0/469|

Times are per call unless marked otherwise; forecast subphases nest in market/settlement. Public payroll includes constructing the ordered membership index; private payroll includes production and wage distribution. `fixedStep` excludes the economy's preceding service/forecast/billing work. Do not sum nested means or compare an inclusive monthly tick with billing alone. FPS/p99 can conceal infrequent long pauses; worst frames and slow-frame elapsed time remain necessary measures.

## Remaining priorities

| Grid,5x | Services/scenario approximately per day, including economy | Hospital care per day | Waste per day | Traffic update/advance per day | Citizen scheduling per call | Treasury billing per month | Maximum steps per tick / catch-up ticks |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
|64|22.03ms|9.02ms|7.81ms|10.32ms|0.40ms|0.12ms|1/0|
|80|49.35ms|23.63ms|12.74ms|12.06ms|0.80ms|0.19ms|1/0|

These groups overlap with their inclusive parent scopes. Hospital care includes the pre-reconciliation pass. The64 prepared run has a321.15ms app frame,317.38ms simulation tick and310.88ms fixed-step peak; scheduling reaches280.94ms. Its measured child-call peaks did not explain the tail in this slice. **Later attribution2026-10-11:** log review finds a handled month10 population/economy reconciliation ensure with242ms diagnostic cost (184ms in its reference control). A new timing-only capture isolates the monthly check at192.33ms, versus completion0.08ms, traffic copy0.23ms and live settlement0.32ms. See the [settlement report](DESKTOP_SETTLEMENT_2026-10-11.md). Repeated whole-city balance refresh was a source-level candidate, but measured completions do not justify prioritizing it for this fixture. Endpoint equivalence and225 passing regressions did not cover this later integration invariant. Retain these frames as diagnostic evidence; neither64 five-speed run is clean correctness acceptance. Do not claim all stalls improved:64's prepared worst exceeds its271.35ms reference worst even though its mean and slow-frame elapsed fraction fall.

Also audit reuse of the economy's service result at the immediately following subsystem phase, then index remaining service staffing and care/waste lookups where measurements justify it. Preserve intermediate deliveries, emergency/custody/education/construction deadlines, finite supplies, exact wages/rounding, immediate paused edits and monthly settlement. Consider bounded display-only refreshes or staggered nonurgent work afterward; changing authoritative ticks to multiple days is a separate gameplay/compatibility decision. Higher-speed coarsening is not needed for the gains measured here. Keep64 as default; remaining pauses block80 production acceptance.96/128, representative laptop, PCVR and Quest performance remain unmeasured for this slice; D72 owns versioned production dimensions.

## Validation and integration

All225 rendered regressions pass, including the new population equivalence case: initial arrivals; retained civic/private workers; four qualification tiers and changing qualifications; skilled transfers releasing entry slots; finite housing/employment; medical leave/detention; outages/recovery/demolition; varied wages, zero/free prices, cash-limited public/private payroll, freight/legacy purchase paths and existing reservations. Five lifecycle rounds exercise240 paired settlement scenarios twice each and compare every serialized population identity/wallet/ledger/reservation/trip plus derived leave. Forecast outputs match and input population remains unchanged. The existing guard test proves normal/saved comparison immunity. The protected D78 city still runs400 game days at1x/3x/5x with successful R/C/I growth; its input hash remains `50C23550DD3A8A4F07A6C7468A78E96AC65FE56D84000F9B9FEF2DBA4B6B0DA6`. No player save is written.

Warnings-as-errors Editor compilation,268-file formatting and helper parsing pass. Integration package identities, signed ARM64/SDK validation, cooked4K UI/save checks and normal64/default-budget proof are recorded in CHANGELOG and `Artifacts/D74-Forecast/Implementation.json`. Packages establish local integration; physical input/headset comfort and the remaining D78 recovery/cost/chart checks stay pending. No public/source publication or headset install is performed. The new timing observers are optional and null in normal play.

Reproduce a5x pair with `Tools/Run-DesktopBenchmark.ps1 -Lots 1000 -Populated -LateGame -Speed 5 -Weather Clear -Width 3840 -Height 2160 -CitizenRoutes 128 -VisibleTraffic 512 -DurationSeconds 75 -WarmupSeconds 6 -CaptureStartStep 100`, adding `-ReferencePopulation` for the control. For80, build the guarded capacity archive and pass `-ExpectedGridSize 80 -Lots 6400 -ExecutablePath <research executable>`. Restore normal64 packaging afterward. `Artifacts/D74-Forecast/Compare.ps1` retains the exact guards and report/metadata paths; `Reports.json` records all six final captures.

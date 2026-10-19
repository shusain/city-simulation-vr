# Verified development backlog

Archived 2026-10-01. Historical snapshot of completed cards and accepted slices before documentation consolidation; do not update this file or use its next-step/status text as current direction. Start with [DEVELOPMENT_TASKS.md](../DEVELOPMENT_TASKS.md); record completed work in [CHANGELOG.md](../CHANGELOG.md). Pending device and performance checks remain active until confirmed. The snapshot body below is retained as historical evidence.

## D39 — Saved-city stagnant growth and actionable budget recovery

**Diagnosis:** Pulled the user's Quest save into a read-only local reproduction copy, loaded it through the actual Unreal catalog/subsystem, and confirmed a real treasury blocker. Month17: funds0, last income267, bills525, unpaid258; roads240 (164 Local/19 Avenue), two power/two water160, other buildings85 and public wages40 reconcile to525. Loaded residential demand28, spare power90/water82. Saved input `Artifacts/Diagnostics/D39-stagnant-city-original.json`, SHA256 `7F4DC9FA997680060269BDAA901A4CB74DDB5B544F9EAF7BC30FF68F8B7CC014`; compact evidence `D39-budget-recovery-saved-city.json`. The source copy remains unchanged; headset save/policy are not edited.

**Behavior/files:** `CityEconomySimulation.{h,cpp}` adds derived `FCityBudgetProjection` and a shared exact cost/income calculation. Monthly billing consumes the same calculation after substituting actual completed sales. `CitySimulationSubsystem.{h,cpp}` exposes current/pending-policy forecasts without mutation. `CityNeedsPresentation.{h,cpp}`, `CityBoardActor.cpp` and `CityVRToolController.cpp` replace UPKEEP LOSS with UNPAID BILLS, show last-month income/bills/unpaid amount, current costs by category, forecast monthly net and concrete tax/unused-infrastructure/billing actions. The Taxes footer previews the pending policy's net before Apply and names the demand tradeoff. Recovery text moves utilization/demand rails downward and expands their existing background using rendered bounds. CityFirstPlayableGuide and growth/needs descriptions name City > TAXES and waiting for a paid month. Any monthly expense shortfall continues to block growth; construction still requires treasury funds. Rates, costs, utility floor, payroll rules and schema30 remain unchanged.

**Checks:** Editor Win64 Development warnings-as-errors and all96 CitySim tests pass with rendering enabled,0 failures/reconciliation failures. New `CityBudgetRecoveryTests.cpp` checks cost totals, preview/billing equality, no policy/treasury mutation, no retroactive payment, and clearing the guard at the next paid billing. This run passes `-CitySimBudgetSave=<read-only copy>` and proves actual player-command recovery: residential25%/commercial10% pays bills and creates a new zoned home by month19, income649/bills525 and funds41 after construction,12→13 buildings. Existing presentation/guide tests assert exact diagnosis, real menu navigation, billing timing and separation of forecast/last-month values. ContextPanels verifies recovery text does not overlap bars and remains inside the panel background. All education/utility/payroll/traffic/save/safety tests pass. Logs `Artifacts/Diagnostics/D39-budget-recovery-{editor,tests}.log`.

**Integration:** Android ARM64/Vulkan/ASTC package, saved-city-preserving install and cold launch pass on Quest2G0YC5ZG1401P3. APK184,062,322 bytes; SHA256 `6947E1E80C97CC394A98C71A3E865FD3778290358892538C7D5F2F4AE11804A9`. PIDs10691/10765 produced184 startup lines,0 fatal/assert/ensure/error matches. Headset save hash still equals the original pulled copy. Metadata `Artifacts/Diagnostics/D39-budget-recovery-build.json`; logs `D39-budget-recovery-{editor,tests,package,deploy,runtime}.log`. Quest chart/Taxes/tutorial readability, player recovery and school progress remain active in DEVELOPMENT_TASKS.md; broad balance/performance acceptance is pending. Next: D39 device budget recovery, then D46 education feedback and D47 parks.

## D46/D39 — Utility minimum supply for early growth

**Feedback:** User could not reach education testing because of budget pressure and power/water supply falling to50. The previous25% paid-staffing floor on nominal200 caused that reduction. Basic education already qualifies utility staff.

**Behavior/files:** `CityEconomySimulation.h` adds authored `MinimumUtilityOutputPercent=50` and a shared bounded percentage rule. `CityGrowthSimulation.cpp` applies it per connected, undamaged provider: at least100 power or water at default nominal200, up to200 at full paid staffing. Small authored capacities retain their own percentage scale. Road/fire outages still disable supply; emergency allocation priority, costs, salaries, taxes and school capacity retain their current contracts. `CityBoardActor.cpp` uses the shared percentage and adds exact current/nominal supply and minimum-supply inspection. Schema30 and saved identities/learning remain compatible.

**Checks:** Editor Win64 Development warnings-as-errors and all95 CitySim tests pass with rendering enabled,0 failures/reconciliation failures. `CityEmergencyTests.cpp` adds `CitySim.Growth.UtilityMinimumSupply` for empty staffing, unpaid/full/partial wages, smaller tuned capacities, fire damage, road outage/restoration and per-provider aggregation. Existing public-payroll settlement now asserts paid Basic workers supply200 each and settled unpaid workers retain100 each; departments still have zero unpaid capacity. Existing education, payroll, emergency priority, persistence and traffic suites pass. Edited text checks pass. Logs `Artifacts/Diagnostics/D46-utility-floor-{editor,tests}.log`.

**Integration:** Android ARM64/Vulkan/ASTC package, saved-city-preserving install and cold launch pass on Quest2G0YC5ZG1401P3. APK184,058,538 bytes; SHA256 `6F53129377719D3AC190D18C998D4665C4F94BB90C624E9ADD53BBE3C10DC4CA`. PIDs6969/7008 produced184 startup lines,0 fatal/assert/ensure/error matches. Metadata `Artifacts/Diagnostics/D46-utility-floor-build.json`; logs `D46-utility-floor-{editor,tests,package,deploy,runtime}.log`. Early cash flow, budget recovery and education checks remain active in DEVELOPMENT_TASKS.md; this automated slice does not claim device balance acceptance. Next: D46 education feedback, then D47 parks.

## D46 — Schools, saved education and higher-paid skilled work

**Scope:** Implementation, assets, desktop visual behavior and Quest installation are verified. Manual school/menu/education/wage checks remain active. User accepts D45 fire and police working well, with crime event indicators deferred to D52.

**Behavior:** Appended stable School definition (3x2,$1200,25 construction days after delivery,power10/water8,upkeep35 plus4 Basic public staff). Actual paid staffing scales48 resident education places over24 road hops; shared evaluators allocate only unmet eligible residents, cap overlapping desirability bonus8, and remove benefits under road/utility/construction/damage/staff outages. Learning advances once per served billed month, retains progress during outages and promotes None→Basic or Basic→Secondary after6 served months. Schools stop at Secondary. Medium/high private businesses reserve floor(jobs/4) Secondary-qualified skilled roles. Graduates can transfer from private entry roles; public staffing keeps priority. Target skilled wage10 versus entry5 is capped by education and paid proportionally from real employer cash, with stable integer remainders and exact conservation. Forecast and settled ledgers share the same rules.

**Presentation:** Build > SCHOOL, City > EDUCATION, school capacity/staff bars, home access/qualification/progress and business skilled-role/target/actual payroll inspection. Two appended tutorial goals teach schools and developing skilled workers, retaining prior safety history. Build gains one bounded target; fixed-drawer spacing is5.5 and tests verify every expanded button fits and does not overlap for fixed/hand layouts. School is a compact classroom/entry/book-emblem model with shared Roof/Trim slots; closed charred derivative uses existing damage swaps.

**Files:** New `CityEducationSimulation.{h,cpp}` and `CityEducationTests.cpp`; CityPopulationSimulation, CityEconomySimulation, CityGrowthSimulation, CityEnvironmentSimulation, CityFireSimulation, CitySimulationSubsystem, CityFirstPlayableGuide, CityPersistence, CityPaletteModel, CityVRToolController and CityBoardActor headers/implementations. GuidedObjectives and ToolInputRouter tests. New Create-CitySchoolArt, Update-CitySchoolCatalog, Render-CitySchoolPreview and Capture-CitySchoolPreview scripts; focused SchoolOnly support in common import/damage tools. CitySchoolKit/CitySchoolDamageKit.blend, two FBXs/UAssets, manifest and catalog; primary/backlog/handoff/changelog, citizen architecture and asset pipeline docs.

**Persistence:** Schema30 stores citizen training months/last credited month/skilled-role flag and business skilled capacity. Earlier schemas preserve IDs/wallets/education and start learning at the saved month; no past credit is invented. The next appended guide milestone is recalculated for earlier saves. Invalid/future clocks, unqualified roles, overfilled role capacity and invalid progress reject restore/load before commitment.

**Checks:** Editor Win64 Development warnings-as-errors and all94 CitySim tests pass with rendering enabled,0 failures. New tests cover capacity/overlap, partial staffing, utility/road outages, repeated-month idempotence, graduation, bounded benefit, saved state, Basic/Secondary role eligibility, exact full and cash-limited differential wages, real catalog/city operation, school save/restore/schema29 migration, whole-city equality at1x/3x/5x and transactional rejection of future clocks. Updated guide tests verify both appended milestones. Existing safety/traffic/economy/save suites remain passing. The first run passed all new education tests but caught an obsolete9-entry menu limit; final menu bounds/overlap checks pass. Python syntax and edited text checks pass.

**Asset/visual evidence:** Normal and damaged meshes both228 triangles/two sections,24x16m lot, correct bounds/ground-center/material assignments; source damage retains114 faces and zero boundary edges. Focused import reports2 meshes/0 errors in `Artifacts/AssetImport/D46-school-import.json`; geometry reports `D46-school-{geometry,damage-geometry}.json`. Inspected Blender preview `Artifacts/ArtPreview/D46/SM_Building_School_A.png`. Inspected actual imported normal/damaged roofs from both sides in temporary PIE: `Artifacts/Preview/D46-Schools-{Front,Back}.png`, `D46-Schools.json`; gallery/map discarded without saving. Desktop evidence does not establish Quest readability/comfort.

**Integration:** Android ARM64/Vulkan/ASTC package, install retaining saved cities and cold launch pass on Quest2G0YC5ZG1401P3. APK 184,055,038 bytes; SHA256 `5B166279AF3D80119CCF423CC9BF7307F48FAA0C53ABA7C0E3FF86A08CA8C400`. PIDs2962/3025 produced184 startup lines,0 fatal/assert/ensure/error matches. Metadata `Artifacts/Diagnostics/D46-school-build.json`; logs `D46-school-{editor,tests,art,damage-art,import,catalog,pie-preview,package,deploy,runtime}.log`.

**Remaining:** Quest school/menu/overlay/progress/wage/cadence review at1x/3x/5x, partial learning save-resume and outage recovery. No child/student agents or college/Higher education in this slice. School/skill finance balance remains D39, representative performance D23. Next D47 parks after feedback; D52 crime/map indicators remain queued.

## D44/D45 — Safety systems accepted on device (2026-10-01)

User confirms the revised fire timing is much better, has observed both prompt truck arrival/free recovery and distant response/manual repair, and calls the fire parts validated. This closes the active D45 fire system/device card. Police is working well so far; user accepts continuing to schools and will revisit visible crime events with D52's broader map icons/indicators. Existing detailed implementation, automated and Quest integration evidence is retained below. No additional scripted device cases or formal D23 thermal/performance run are inferred from this report. Broad progression/balance remains D39; future upgrades/disasters/indicators remain D48/D51/D52.

## D45 — Slower fire damage and spread pacing

**Scope:** Implementation, automated behavior and Quest installation are verified. User reports that a nearby engine arriving within20 seconds still leaves repairable damage and clusters spread too quickly. Revised headset pacing remains active before D46 schools.

**Behavior:** Unattended damage per game day drops9→2 through authored `FireDamagePerStep` (clamped1–100). Suppression remains1 damage/day for three days, free recovery limit30%. Burnout age increases12→50 days,28.8→120 seconds at the default2.4-second day/1x. A9-day wait (21.6 seconds) plus full suppression leaves22% and recovers free;13 days leaves30% and recovers,14 leaves32% and requires repair. Spread interval increases3→5 days, scheduled from each source's ignition age, so new/spread fires wait a full interval. Base chance, wind/rain/protection, orthogonal adjacency and active/damage caps remain. Natural ignition cadence is unchanged. Game speed scales fire and traffic together; quoted seconds are1x examples. Existing accumulated/resolved damage is retained, schema29 unchanged.

**Files:** `CityEconomySimulation.h`, `CityFireSimulation.{h,cpp}`, `CityFireTests.cpp`, `CityEmergencyTests.cpp`; primary task/backlog/handoff/changelog and citizen architecture. No presentation meshes/materials changed.

**Checks:** Editor Win64 Development warnings-as-errors and all91 CitySim tests pass with rendering enabled,0 failures. FireResponseWindowAndSpreadCadence covers9/13/14-day response delays, extended burnout, source-age scheduling, full interval after spread, saved/coarse continuation and no treasury spending. Real road/outage regression observes nearby arrival7/recovered damage0 versus distant arrival17/damage34. Existing paid-repair fixtures explicitly start with structural damage or await the new burnout deadline. Chained crews return then freshly dispatch from their station to a still-burning distant fire, preserving scene-to-scene range limits. All prior save/migration, wind, native mesh/capacity and response checks pass. Initial regressions caught an inadvertently protected spread fixture and an old chain fixture assuming the distant fire had already burned out; both fixtures now exercise the intended conditions. Edited-file whitespace/conflict checks pass.

**Integration:** Android ARM64/Vulkan/ASTC package, city-preserving install and cold launch pass on Quest2G0YC5ZG1401P3. APK 184,005,462 bytes; SHA256 `7DACDC0201F8218795CCBE1BD5AE5710EBD7FDA4B52730FB3A5AFADA6825443C`. PIDs26446/26483 produced188 startup lines,0 fatal/assert/ensure/error matches. Metadata `Artifacts/Diagnostics/D45-fire-pacing-build.json`; logs `D45-fire-pacing-{editor,tests,package,deploy,runtime}.log`.

**Remaining:** Quest nearby20-second response and dense-cluster containment at1x/3x/5x, delayed/out-of-service paid damage, and broader D44 service checks. Existing resolved damage still requires paid repair. D39 owns progression/cadence balance, D23 performance/comfort. Next D46 schools after current feedback.

## D45 — Prompt-fire automatic recovery

**Scope:** Implementation, automated behavior and Quest installation are verified. Manual headset recovery check remains active before D46 schools. User accepts the previous roof/traffic update overall.

**Behavior:** Ignition creates damage as before; when three suppression steps finish, damage up to30% clears automatically without a repair fee. The same authoritative damage record controls operation and native mesh selection, so superficial fires return to the original building and available housing capacity. Damage above30% and unattended burnout remain disabled until paid repair. Real travel, delayed arrival and accumulated damage still determine the result. Existing already-resolved damage/paid repair deadlines are retained. `FireMinorDamageAutoRepairLimit` is authored tuning (0 disables, maximum99); save schema29 is unchanged. Burning inspection explains the recovery limit.

**Files:** `CityEconomySimulation.h`, `CityFireSimulation.cpp`, `CityBoardActor.cpp`, `CityFireTests.cpp`, `CityEmergencyTests.cpp`; active/backlog/handoff/changelog, citizen architecture and asset pipeline docs. No meshes/materials changed.

**Checks:** Editor Win64 Development warnings-as-errors and all90 CitySim tests pass with rendering enabled,0 failures. New MinorFireRecoveryBoundaries covers final damage4/30/31/100, inclusive threshold, partial suppression, mid-suppression restore, no treasury charge, rejected unnecessary repair, fresh repeated ignition, burnout and disabled recovery. Existing severe-fire tests retain paid repair/deadline coverage. Native presentation regression verifies damaged imported mesh while burning, original pool restoration, stopped water, operating housing and capacity10 after prompt suppression. Delayed full-city arson retains damage only above the threshold. Edited text whitespace/conflict checks pass.

**Integration:** Android ARM64/Vulkan/ASTC package, install retaining cities and cold launch pass on Quest2G0YC5ZG1401P3. APK 184,001,126 bytes; SHA256 `A4D952266297AF86837C2488C06117C4A9C3D2D340FAAC135224ABB260C7A2F1`. PIDs11213/11252 produced188 startup lines,0 fatal/assert/ensure/error matches. Exact metadata `Artifacts/Diagnostics/D45-minor-recovery-build.json`; logs `D45-minor-recovery-{editor,tests,package,deploy,runtime}.log`.

**Remaining:** Quest nearby versus delayed/distant fires at1x/3x/5x, original mesh/operation and no REPAIR bill after prompt suppression. Old resolved damage still needs paid repair. D23 performance, D39 cadence/balance and broader D44 service checks remain. Next D46 schools after current feedback.

## D45 — Closed damaged roofs and emergency traffic yielding

**Scope:** Implementation, imported geometry, desktop rendering, automated behavior and Quest installation are verified. On2026-10-01 the user accepts the roof/traffic update overall and requests prompt-fire recovery before schools. Dedicated performance/comfort/cadence gates remain D23/D39. Previous lights are also accepted.

**Behavior:** Retained every original face in all15 damaged building/stage variants, recalculated normals and kept the charred/sagging presentation. The previous generator deleted upper roof panels, allowing the board to show through hollow meshes; both reported Quest screenshots were inspected. Normal cars/trucks and returning crews stop and pull aside for actual FireResponse/PoliceResponse journeys. Responses wait for previous curbside state before passing, hold approaching normal junction queues and reserve connectors first. Opposing crews use stable ID priority; endpoint clearance allows police and fire approaching one building to both arrive. Traffic waits for a safe gap, merges while stopped, then resumes. Walkers keep sidewalk flow. Scene arrivals settle onto their travel side; water shares the same lateral pose helper. Existing fixed-step records and instance pools are used, without new per-vehicle Actors/ticks.

**Files:** `Tools/Create-CityDamageArt.py`, new `Tools/Capture-CityDamagePreview.py`; CityDamageKit.blend, 15 damaged FBXs/UAssets and manifest measurements. C++ CityTrafficSimulation/CityTrafficTrips, CitySimulationSubsystem, CityBoardActor, CityPersistence and traffic/save headers; CityTrafficSimulationTests, CityVisibleTrafficTests and failure diagnostics in CityEmergencyTests. Primary task/backlog/handoff/changelog plus asset, road and citizen architecture docs.

**Checks:** Editor Win64 Development warnings-as-errors and all89 CitySim tests pass with rendering enabled,0 failures. New checks cover car/freight/oncoming pull-over, waiting for lateral clearance, overtaking, queued safe rejoining, emergency return priority, connectors, opposing/shared-portal responses, native instance transforms, save/migration and fine/coarse continuation. Existing station recovery, chained response and water activation checks pass. Schema29 stores response/yield flags; older saves derive response priority from associated trips. Full-city restore rejects source/priority mismatches; curbside records must be stopped and valid. Geometry checks retain original face counts and boundary counts, reject degenerate faces and keep all15 meshes at90–644 triangles. Focused import reports0 errors. Edited text whitespace/conflict checks and Python syntax checks pass.

**Visual evidence:** `Artifacts/DeviceCapture/com.shaun.citysimvr.smoketest-20261001-011026.jpg` and `...011135.jpg` show the original issue. Inspected `Artifacts/Preview/D45-DamagedRoofs-{Front,Back}.png`: all15 imported variants hide the green floor through their roofs from both directions. `D45-DamagedRoofs.json` records exact meshes/positions. The helper creates a temporary gallery, copies it into PIE, then discards it without saving the map. Desktop captures do not establish Quest appearance/comfort.

**Integration:** Android ARM64/Vulkan/ASTC package, install retaining cities and cold launch pass on Quest2G0YC5ZG1401P3. APK 183,999,098 bytes; SHA256 `5B1A1B5F24820ED133D80058D96238A8DB762FE89E0BA01A1025E90D9790510A`; 188 current-process startup lines,0 fatal/assert/ensure/error matches. Exact metadata `Artifacts/Diagnostics/D45-roofs-yield-build.json`. Logs `D45-roofs-yield-{editor-final,tests-final,art,import,preview,package,deploy,runtime}.log`; import report `D45-roofs-yield-import.json`, source geometry `Artifacts/AssetImport/city_damage_geometry.json`. The first regression run caught an ordinary-freight priority error; subsequent opposing-response checks caught shared-entrance deadlock. Both are corrected and covered by the final passing suite.

**Remaining:** Quest roof solidity and yielding/resumption at1x/3x/5x, narrow-road/junction clarity, pause/onward save/load, remaining D44 menu/shortage/restoration checks. Current trip routes use lane0; richer lane choice/shoulders remain later road work. D23 owns performance, D39 cadence/balance. Next D46 schools after feedback.

## D45 — Non-emitting parked lamps and complementary response ramps

**Device acceptance (2026-10-01):** User confirms “the lights look good enough now.” This closes the current lamp appearance check; broader performance/cadence gates remain in D23/D39.

**Behavior:** Parked decorative lamps now emit zero; decoded lens color is scaled to.18 so it looks dim even under environment lighting. Moving police/fire lamps use one normalized two-second curve and its exact inverse, selected by actual red versus blue/warm-white cap colors. Each side ranges0–4; combined scalar brightness remains4, including both crossovers. This prevents both sides falling dark together. Lens shading, geometry/sections, vehicle trips, accepted particles and schema28 remain unchanged apart from the darker lens tint.

**Files:** `Tools/Create-CityCivicMaterials.py`, `Tools/Capture-CityEmergencyPreview.py`, `CityArtKitTests.cpp`, M_CityCivicDetail and saved response material instances; active/backlog/handoff/asset/architecture docs.

**Checks:** Editor warnings-as-errors and all86 CitySim tests pass with rendering enabled,0 failures/reconciliation errors. CivicLightEmission checks zero parked emission, dim lenses, the same-curve inverse connection, actual imported lamp-side selection and full-cycle brightness at201 samples (peaks and zero crossings included), plus existing mask/material/pool rules. Repeated authoring keeps22 expressions before/after. Logs `Artifacts/Diagnostics/D45-beacon-pair-{editor,tests,material,material-repeat}.log`; `D45-beacon-pair-generation.json`.

**Visual evidence:** Inspected five live desktop PIE captures using actual response pools: red peak, other peak, both crossovers and the original parked Roof material under identical lighting. Peak frames show the other lens dim; both crossover frames retain visible illumination; parked lenses are dim/non-emitting. `Artifacts/Preview/D45-BeaconPair-{Red,Other,CrossA,CrossB,Parked}.png`, `D45-BeaconPair-Preview.json`; log `D45-beacon-pair-preview.log`. Capture temporarily isolates/enlarges existing components, switches to the actual mesh material for the parked comparison and closes without saving. Desktop evidence does not establish Quest appearance/comfort.

**Integration:** Android ARM64/Vulkan/ASTC package, install retaining cities and cold launch pass on Quest 2G0YC5ZG1401P3. APK 183,987,898 bytes; SHA256 `E79EFD1614187334F7EDF9ED8998E32D6DBD5C13E93FF3C025F3D58445B51A0F`; 188 app-process lines, 0 fatal/assert/ensure/error matches. Exact metadata: `Artifacts/Diagnostics/D45-beacon-pair-build.json`. Logs `Artifacts/Diagnostics/D45-beacon-pair-{package,deploy,runtime}.log`. The user accepts the current lights on 2026-10-01. Active D45 now tracks roof/yield feedback; next D46 schools.

## D45 — Civic beacon material wiring and PIE verification

**Behavior:** Repaired missing vertex-color wires in the civic material's surface and emission paths. Unreal VertexColor's RGB output is unnamed; the previous named RGB calls failed silently, leaving emission multiplied by zero. Create-CityCivicMaterials.py now checks every connection, repairs rooted graphs in place and reuses tagged nodes. Lamp RGB from static-mesh sRGB bytes is decoded with standard Power2.2 before color/emission. Vertex alpha still confines emission to lamp geometry.

Moving response pools override only Roof with MI_CityPoliceResponseDetail / MI_CityFireResponseDetail, BeaconPulse1. Red and blue/warm-white sides alternate smoothly over a two-second render-time cycle between .05 and4 brightness, independent of simulation speed. Parked decorations use BeaconPulse0 and steady4. No new meshes, sections, light Actors or per-vehicle tick work; accepted particles and real emergency responses are unchanged.

**Files:** `Tools/Create-CityCivicMaterials.py`, `Tools/Capture-CityEmergencyPreview.py`, `CityBoardActor.cpp`, `CityArtKitTests.cpp`; M_CityCivicDetail and two new response material instances. Development/handoff/asset/architecture docs updated.

**Checks:** Editor warnings-as-errors and all86 CitySim tests pass with rendering enabled. CivicLightEmission checks real color/mask/decode graph connections, actual response-pool material overrides, parked/moving parameters and lamp vertices in the emissive Roof section. The graph inspection regression is editor-only; Android builds exclude editor-only API calls. Repeated material authoring keeps19 expressions before/after. Evidence `Artifacts/Diagnostics/D45-roof-emission-{editor,tests,material,material-repeat}.log`, `D45-roof-emission-generation.json` and original broken-graph report `D45-roof-emission-before.json`.

**Visual evidence:** Actual fire/police response pools captured in live desktop PIE at opposite cycle phases (~14.44 and15.44 game seconds). Inspected both PNGs: colored emission and alternation are visible. The capture temporarily isolates/enlarges existing components, verifies their moving materials, then closes PIE without saving. `Artifacts/Preview/D45-Emergency-{Red,Blue}.png`, `D45-Emergency-Preview.json`, log `D45-roof-emission-preview.log`. These images verify desktop material behavior; Quest appearance/comfort remains pending.

**Integration:** Android ARM64/Vulkan/ASTC package, install retaining cities and cold launch pass on Quest 2G0YC5ZG1401P3. APK 183,987,578 bytes; SHA256 `FE999CCD7021BF00A6D8D3F5F1FB2BF0A58C15F2DCD58109D451DB78CE9DC9F2`; 188 app-process lines, 0 fatal/assert/ensure/error matches. Exact metadata: `Artifacts/Diagnostics/D45-roof-emission-build.json`. Logs `Artifacts/Diagnostics/D45-roof-emission-{package,deploy,runtime}.log`. Active D45 retains the headset emission/alternation check; nearby-fire handoff and geometry are user-accepted. Next D46 schools.

## D45 — Nearby-fire handoff accepted on device

User confirms the same engine put out a second nearby fire. The larger lamp geometry is clearly visible, but its Unreal material shows no emissive color; that material issue remains active in D45. Spray timing at all speeds and formal performance checks remain pending.

## D45 — Visible spray arrival and scene-to-scene fire response verified

**Behavior:** Water uses the displayed engine's interpolated road pose and waits until that pose reaches its endpoint. Fire/Crime views retain emergency vehicles; other overlays suppress the associated spray. A completed response selects the closest unclaimed active fire within 12 connected car-road hops from the current scene, breaking ties by incident ID. The crew retains its trip/station identity and travels the real road route; claimed, distant and disconnected targets are skipped. Lost station operation or ordinary-bill shortfall prevents onward assignments. No eligible target leads home to the associated station, including after several scenes.

Schema28 validates scene-origin fire responses and station-bound returns. Older station-origin journeys remain valid. Fire-engine emissive roof caps now measure .75x.72x.56 m, raised above their housing; overall engine height is 2.78 m. Triangle counts, two material sections and the accepted particle intensity remain unchanged.

**Files:** `CityEmergencyResponses.cpp`, `CityPopulationSimulation.{h,cpp}`, `CitySimulationSubsystem.{h,cpp}`, `CityPersistence.h`, `CityEmergencyPresentation.h`, `CityBoardActor.{h,cpp}`, `CityEmergencyTests.cpp`, `CityArtKitTests.cpp`, `Tools/Create-CityCivicArt.py`; civic/damage Blender sources, FBXs, manifest and imported meshes.

**Checks:** Editor warnings-as-errors and all 85 CitySim tests pass, 0 failures/reconciliation errors. Tests run with rendering enabled and check real emitter activation, final interpolated arrival/pause, overlay visibility, competing crews, range/connectivity/bill/utility fallbacks, eventual return, partial-road restoration and full-city save/load during onward response. Raised-lamp source preview inspected; four civic and 15 damaged meshes reimport with exact bounds/pivots/material assignments and 0 report errors. Evidence: `Artifacts/Diagnostics/D45-scene-chain-{editor,tests,civic-art,civic-preview,damage-art,civic-import,damage-import}.log`; focused reports `Artifacts/AssetImport/D45-scene-chain-{civic,damage}-import.json`.

**Integration:** Android ARM64/Vulkan/ASTC package, install retaining cities and cold launch pass on Quest 2G0YC5ZG1401P3. APK 183,976,578 bytes; SHA256 `4387D887F15FBFD074927F51E42B0F9D59A30CC39573535A7FA5BCAF1140EF61`; 188 app-process lines, 0 fatal/assert/ensure/error matches. Exact metadata: `Artifacts/Diagnostics/D45-scene-chain-build.json`. Logs `Artifacts/Diagnostics/D45-scene-chain-{package,deploy,runtime}.log`.

**Pending:** Lamp visibility and revised response choreography require Quest review; D45 retains these checks. D52 queues repair/repairing/crime icons, with health/hospitals later.

## D45 — Fire and water intensity accepted on device

User confirms that the updated flame/water effects look good. Current particle size/lifetime/intensity are accepted. Remaining feedback is tracked in D45: early displayed spray, engine response chaining and hard-to-see fire-engine lamps. Requested repair/repairing/crime world indicators are queued as D52; health/hospitals remain a later system.

## D44/D45 — Safety feedback refinements verified

**Behavior:** Fire stations are fireproof for natural ignition, spread and targeted test tools. Loaded legacy station damage/repair clears; active station fires resolve while history survives. Police/fire reserve power/water ahead of ordinary consumers in stable ID order. Staff, road, wages and real capacity still constrain response. Inspector labels crew travel/scene/return and ordinary bills correctly; regression proves recovery followed by a fresh response.

Game settings > SAFETY TESTS selects Fire/Crime/Arson, then trigger targets a completed building. Tests use normal capped incidents, road arrival/return, spread, damage and paid repair, even before population gates. Test flags persist in schema27 and propagate with fire spread; natural onboarding remains unchanged. Arson commits both events atomically. Police-shortened deadlines save/restore correctly.

Three pooled Unreal particle pairs now use rates36/12/80, lifetimes2/4/.9 seconds, larger sprites and sustained alpha; emission3/1/1.6 for flame/smoke/water, approximately576 live particles total before frame granularity. Roof jet normalization650 cm. Police red/blue and fire red/warm-white roof caps use steady emissive strength4, two sections. Meshes police lot/car544/260 and fire lot/engine644/392 triangles; FBX colors, dimensions/ground pivots/assignments survive focused reimport. Burnt derivatives are synchronized.

**Files:** `CityGrowthSimulation.cpp`, `CityFireSimulation.{h,cpp}`, `CityCrimeSimulation.{h,cpp}`, `CitySimulationSubsystem.{h,cpp}`, `CityPersistence.{h,cpp}`, `CityPaletteModel.{h,cpp}`, `CityVRToolController.cpp`, `CityBoardActor.{h,cpp}`, `CitySafetyAssetsCommandlet.cpp`, `CityEmergencyTests.cpp`, `CityArtKitTests.cpp`; civic/material/import tools, Blender sources/FBXs/manifest and saved Unreal assets. Architecture/asset/handoff docs describe the current contracts.

**Checks:** Editor warnings-as-errors and all83 CitySim tests pass,0 failures/reconciliation errors; queue empty. New coverage exercises critical power/water allocation, small-city test fire/crime/arson with unchanged natural gates, atomic rejection, pause, incident save/load, shortened police deadline, crew return/reuse and legacy station recovery followed by another engine response. Source vehicle previews inspected, four civic and15 damage variants reimport with0 report errors and preserved material/bounds contracts; imported roof colors/emissive fire material and bounded particles pass tests.

**Quest integration:** Android ARM64/Vulkan/ASTC package, install retaining app data and cold launch pass on Quest 2G0YC5ZG1401P3. APK183,967,738 bytes; SHA256 `C730259C57428C6E815B09D4768555626B3BE10E0032715760E89F3CF73A82CA`;188 app-process lines,0 fatal/assert/ensure/error matches. Exact metadata: `Artifacts/Diagnostics/D45-safety-refinement-build.json`.

**Pending:** Revised headset effect/light visibility, shortage/recovery/repeat responses and test-menu usability. Rates/finance balance D39; sprite overdraw/comfort/thermal D23. Manual tests do not implement natural arson agents/disaster progression (D51). D46 schools follows the current check.

## D44/D45 — Initial response playtest observations

User observed multiple fires and some fire engines reaching buildings and extinguishing them; accepted the direction of the arrived-engine water spray. Reported weak fire/water visibility, no further dispatch after a burned station was repaired, utility shortages disabling departments, and requested fireproof stations, critical utility priority, scripted testing controls and clearer vehicle lights. The revised implementation/checks remain active in DEVELOPMENT_TASKS.md until verified.

## D44/D45 — Public workforce, road response and safety assets

**Verified implementation (2026-09-30):** Editor warnings-as-errors and all 81 CitySim automation tests pass; zero failures/reconciliation errors. Blender damage source/preview inspected; focused import/reimport validates all15 variants and semantic material persistence. Full schema26 city saves preserve wages, citizens, incident state and actual traffic progression; fine/coarse3x serialized-city equality passes. The road-outage test delivers the nearby engine on day7 with49% damage after a two-day road cut, versus day17/100% burnt-out damage for the distant route. These are deterministic fixture results, not device observations or balanced real-world rates. New particle/device acceptance remains in the active cards.

### D44 — Public staffing, payroll and police response

**Status:** User accepts both station models on Quest (2026-09-30); accepted appearance and earlier verified foundations are archived in DEVELOPMENT_BACKLOG.md. Public employment/payroll and road response are implemented; Editor/automation/import checks pass; Android package/install/cold launch passes; installed on Quest. Current device acceptance stays pending.

**Current contract:** Starter Police/Fire require four Basic-educated citizens; Power/Water employ two. Civic vacancies have priority over private work, retain established public workers and transfer in stable citizen order. All jobs join the city census; private ledgers remain distinct. Monthly salaries use the existing entry wage ($5/person), are included once in the treasury operating bill and transfer into citizen wallets. Ordinary operating bills have priority; partial/unpaid salaries scale effective staffing. Inspect public buildings for staff, paid/due wages and output. Utilities keep 25% unattended output to allow initial housing/hiring and recovery; paid staff raises it to full capacity. This bootstrap choice avoids requiring housed workers before the utilities that admit them exist. Department protection scales with effective staffing and existing road/power/water/upkeep requirements.

**Response:** Existing authoritative traffic carries distinct police cars/fire engines. Each starter station has one crew, busy during outward travel, time on scene and return. Dispatch selects the nearest available operating station by connected car-road route length; emergency routing can extend beyond the 24-hop prevention area. Sixteen emergency journeys globally bound route/render work. Road changes cancel obsolete traffic paths and retry the saved journey through current portals without reporting a false arrival. Arrived police shorten an active incident; dispatch alone grants no response effect. Cars retain recognizable emitting red/blue roof caps. Parked lot vehicles remain decorative.

**Persistence/checks:** Schema26 saves civic employer ledgers, paid wages, emergency station/incident associations and actual traffic route/progress. Older cities retain identities/wallets/history and acquire civic assignments on the next fixed step; no backdated salaries or invented response journeys. Validate employer/station/target references and reject bad saved state before changing the live city. Editor warnings-as-errors passes. All 81 CitySim tests pass, zero failures/reconciliation errors; focused import/reimport preserves bounds/pivots/materials. Package/install/cold launch and clean startup are verified under `Artifacts/Diagnostics/D44-response-*` and `D44-damage-*`.


**Next:** Complete this integration gate and the D45 fire presentation check, then D46 public schools. Paid station expansions remain D48.

### D45 — Fire particles, damaged structures and travel-sensitive recovery

**Status:** User accepts station appearance and requested Unreal emitters plus damaged meshes for every structure/stage. Implemented; Android package/install/cold launch passes; installed on Quest. Device particle appearance, readability and rate/comfort checks remain pending.

**Fire behavior:** Establishment threshold150 total residents and two-month warning remain. Base ignition8 per10-day check, industry itself+10 and hot weather+6. Add up to 20 from exact local crime risk and up to 20 from industrial footprint density in a six-cell square radius. Rain halves combined risk; staffed prevention reduces it by up to75%. Inspect names crime/nearby-industry contributions. Existing edge-adjacent, wind/rain-influenced spread, sorted sampling, three active fires,32 history records and eight damaged lots remain bounded.

**Travel/damage:** An engine must arrive through real road traffic before extinguishing or suppressing ongoing spread. Three days on scene extinguish; damage grows1 per responded day versus9 while waiting/travelling. The12-day burn deadline leaves100 damage on a burnt-out, disabled structure. Travel distance, road speed, congestion and availability therefore change the outcome. Crew remains busy until returning. Road/water/power/staff/pay outages have inspectable effects. Damage disables housing/jobs/providers and density upgrades; citizens and wallets survive. REPAIR charges the existing damage-scaled25% base cost (minimum$25) once, takes10 days and restores normal mesh/operation only on completion; current prototype costs/rates remain D39 tuning.

**Presentation:** Authored Unreal CPU sprite emitters (`PS_CityFire`, two flame/smoke emitters; `PS_CityWaterSpray`, one water emitter) consume actual incidents/arrived response. Three pooled fire/spray component pairs, capped rates/lifetimes and fixed bounds; no per-building Actors or physical fluid solver. Water spray runs from the arrived engine toward the burning roof. Pause freezes emitter updates; time/speed never lets particles decide authoritative damage. Fifteen Blender burnt variants cover all current structures and low/medium/high/construction stages; scorched materials, open/sagged roofs, no lit-window emission, preserved ground pivots/footprints and at most two sections. Buildings switch to the damaged mesh immediately and retain it during repair. These meshes are explicitly included in the Quest cook because selection uses the shared variant path.

**Checks/evidence:** Editor warnings-as-errors passes. Blender generator and source previews are inspected; focused damage import validates dimensions/pivots/sections. Reimport and all 81 CitySim tests pass. Tests cover single treasury/wallet transfers, partial/unpaid staffing, education eligibility, actual road arrival/return, near/far damage, road outage/retry, police arrival, crime/industry/rain factors, old-save migration, active/repair/in-flight saves and fine/coarse full-city equality. Verified artifacts: `D44-response-{editor,tests,package,deploy,runtime}.log`, `D44-{damage-blender,damage-preview,safety-materials,particles,damage-import,damage-reimport}.log`, `Artifacts/AssetImport/D44-damage-import.json`.


**Next:** Current headset feedback and D39 tuning; D46 schools after this safety slice. D51 broader disasters/arson remains later work.

**Quest integration:** Android ARM64/Vulkan/ASTC package, install preserving app data and cold launch pass on Quest 2G0YC5ZG1401P3. APK 183,962,186 bytes; SHA256 `19CE4D655993E94C0357981560DF8F0F6C73CFF86B367C07D22FE7675F92BE90`. Startup scan 184 app-process lines, 0 fatal/assert/ensure/error matches. Metadata `Artifacts/Diagnostics/D44-response-build.json`; logs `D44-response-{package,deploy,runtime}.log`. Device appearance/comfort and balancing remain pending.

## D44/D45 — Department appearance accepted

**User confirmation:** Both fire and police stations look good on Quest (2026-09-30), covering the smaller starter bodies, reserved lots/parking and recognizable vehicles/roof lights in the installed D45 build. Prior build/import evidence remains below and in changelog history. This acceptance does not establish fire rates, staffing/payroll, moving response, new particle effects or damaged meshes; those remain in active D44/D45 until their own checks are observed.

## D44 — Initial station placement and Crime View interaction

**Verified slice:** User confirmed placing a station and viewing Crime View on Quest, 2026-09-30. Initial schema-23 build: 183,477,154 bytes; SHA256 `CB0D53B5D1B4DC2E1842BF51FEEDF5C32BC9E2A78E98B15BA3C28C8884937506`; Editor build, 69 tests, Android install/cold launch and clean startup scan. Logs: `Artifacts/Diagnostics/D44-crime-*`.

**Follow-up:** The verified citizen/education/civic foundation below supersedes the placeholder/displacement estimate. Smaller starter lots/parking are current D44 refinement. Range/capacity/outage/balance acceptance, public staffing/payroll, police teaching completion and response presentation stay in DEVELOPMENT_TASKS.md. This device confirmation covers placement/view access, not full crime balancing.

## D44 — Citizen housing/education and first civic meshes

**Verified foundation:** Editor warnings-as-errors build, 72 tests, Android package/install retaining cities/cold launch and clean startup scan. User confirms the police building is an acceptable starting point and Crime View stats mostly make sense (2026-09-30). Installed foundation: 183,500,974 bytes; SHA256 `5FF6598B77FB8E0022476B854CB231E9D7933986946C6056EF1AB99C595D2924`; logs `Artifacts/Diagnostics/D44-citizens-*` and `D44-civic-*`.

**Citizen contract:** Twelve consecutive game months without employment removes a citizen's home and household assignment. A filled job resets that clock and permits rehousing into available operational capacity. Citizens retain lifetime IDs, wallets and education across moves; homeless citizens have home/household IDs zero. City population includes housed and homeless people. Demolished homes do not delete people; temporary utility capacity outages retain existing physical homes. New capacity admits newcomers only after counting existing citizens, including homeless citizens. Existing valid jobs are retained before filling vacancies. The fixed-step clock handles pause and catch-up; display refresh does not advance unemployment.

**Education foundation:** None / Basic / Secondary / Higher are saved per citizen. Existing and incoming citizens begin with Basic education. Shared eligibility compares education with a required level; prototype monthly salary ceilings are 1/2/3/4 times the entry wage. Existing Shop/Factory jobs require None and keep their current pay. Skilled job definitions, education progression and schools remain D46/future employment work; this slice does not fabricate student records.

**Migration:** Older citizens retain their IDs/wallets and receive Basic education with a fresh unemployment clock. Schema 23's already-recorded displaced cohort becomes independent homeless citizen records with new monotonic IDs and no invented wallet or employment history. Schema 24 saves individual clocks/education and checks the crime homelessness diagnostic against the census. Demolition/rehousing cancels incompatible purchase locks without moving money or goods.

**Blender assets:** `CityCivicKit.blend` and four FBXs: police/fire stations and police car/fire engine, 272/276/236/368 triangles, two material slots each. Police replaces the blue cube and retains the 3x2 logical footprint, cost/construction/services. Fire station and vehicles are imported assets for D45/response work; fire placement and response dispatch are not implemented here. Reproduce via `Create-CityCivicArt.py`; focused import/reimport via `Import-CityArtKit.ps1 -CivicOnly`; source previews via `Render-CityCivicPreview.py`. The shared manifest remains the assignment/footprint contract.

**Files:** Population records/reconciliation/census, service/economy/needs/crime snapshots, schema-24 persistence and city summary/inspection; `CityCitizenHousingTests` plus staffing/crime/panel/art regressions. Civic generator, manifest/FBX/Blender sources, focused importer and police catalog scripts/assets. No map rebuild or per-citizen ticking Actors.

**Verified desktop:** Editor warnings-as-errors build and all 72 CitySim tests pass (zero failures/reconciliation errors). Covers 299/300-step unemployment boundary, consecutive-clock reset, stable IDs/wallets/education, empty household/zero-home state, housing/jobs outages, no replacement duplication, eligibility/salary ceilings, old-save migration/corrupt rejection, full-city fine/coarse equality through the anniversary and actual job/rehousing recovery. Existing crime, money/goods, traffic, guidance and UI checks pass. Blender generation/previews inspected; focused import/reimport verifies dimensions, ground pivots, two sections/material assignments and sub-500-triangle budgets. Logs: `Artifacts/Diagnostics/D44-citizens-{editor,tests}.log`, `D44-civic-{blender,preview,dry-run,import,reimport,catalog}.log`; measured mesh report `Artifacts/AssetImport/city_starter_kit_import.json`.

**Quest integration:** Android package, install preserving app data and cold launch pass. Build: 183,500,974 bytes; SHA256 `5FF6598B77FB8E0022476B854CB231E9D7933986946C6056EF1AB99C595D2924`. Startup scan: 184 app-process lines, zero fatal/assert/ensure/error matches. Logs: `Artifacts/Diagnostics/D44-citizens-{package,deploy,runtime}.log`; build metadata `D44-citizens-build.json`. Final Editor-only regression fixture corrections do not change the packaged gameplay rules or assets.

**Remaining D39 device checks:** Inspect the new police mesh/frontage at tabletop scale and night, and read city homelessness/unemployment in the optional summary and Crime View. Building inspection now shows local crime/protection without attaching the city homeless count to a residence. With no jobs, wait one game year (about 12 minutes at 1x, 4 minutes at 3x, 2.4 minutes at 5x), then add an operational business and confirm employment/rehousing. Existing saves start a fresh unemployment year. Pause/save/load should preserve the counts. Crime range/capacity/outage balance and detailed comfort remain D39/D23.

**Limits:** Homeless citizens retain authoritative jobs/wallets but have no visible home-based commute/shopping routes; shelter/direct-shopping presentation is future work. Education does not yet rise through schooling, existing entry-level wages are unchanged, and emergency vehicle dispatch is not connected. City growth/density upgrades can introduce new cohorts with their own grace year; D39 must balance this cadence.

**Follow-up:** Smaller starter station bodies and parked vehicles are current D44 presentation work; paid civic upgrades are D48. Public staffing, police teaching completion and live response remain D44; fire placement/incident systems remain D45. Initial mesh acceptance does not verify unemployment anniversaries or detailed crime balance.

## D43 — Guided tutorial objectives and progression UI

**Accepted build:** 183,458,806 bytes; SHA256 `451646C9982177A98B53F071F4A639A1E99A8FFE2AB9403E2DF8EB975B0CAB06`; logs `Artifacts/Diagnostics/D43-history-*`. Superseded by D44.

**Status:** User accepted objective presentation overall on 2026-09-30 and authorized police/crime and fire next. Implemented ten saved teaching milestones, history/review, live recovery, goods/basic-service guidance and schema-22 migration. Editor build, all 66 tests, Android package/install/cold launch passed; logs: `Artifacts/Diagnostics/D43-history-*`. Detailed debug-toggle, save/recovery and speed/cadence checks remain D39/D23 rather than claimed as individually observed. New civic guidance belongs to D44-D47.

**Deliver:** An optional compact objective card outside the stats summary, showing one clear next action, why it matters, exact progress/completion criteria and a contextual hint to the relevant tool or target. Default sequence: connect a local road, provide operating power/water, zone and receive the first occupied homes, support jobs/goods, stabilize basic services, then introduce D44/D45 safety and D46/D47 expansion as implemented. Show only available actions; never direct players to an unimplemented or unaffordable mandatory unlock. Allow collapse/reopen and review of completed steps without permanent HUD clutter. Persist stable objective IDs, completion and current stage; reevaluate existing cities so already-completed actions count. Derive progress from authoritative evaluators, not clicks or rendered actors.

**Check:** Fresh/existing cities, out-of-order actions, undo/demolition, shortages, pause/speed/catch-up, reset and save/load produce consistent progress without duplicate rewards or softlocks. Completed teaching steps remain recorded; new service failures generate clear recovery hints. Device tests verify a player can identify and perform the next action without opening stats.

**Files:** `CityFirstPlayableGuide`, `CitySimulationSubsystem`, `CityPersistence`, `CityVRToolController`; guide/persistence/panel/freight tests. No reward/economy rules or future civic services added.

**Verified:** Fresh/existing and out-of-order milestones, idempotent completion, actual-arrival goods recognition, full fine/coarse save equality, outage history retention, schema-21 migration, corrupt-history rejection without city mutation, reset, actual font bounds and history input routing. Logs: `Artifacts/Diagnostics/D43-history-{editor,tests}.log`.

**Remaining targeted verification:** D39 tracks debug/lighting submenu controls, history/save/recovery edge cases and goods/basic-service progression at 1x/3x/5x. D23 tracks formal comfort/lifecycle/performance. User acceptance covers overall objective presentation.

**Limitations:** Collapse/review/debug/lighting preferences remain session-only. Older schemas cannot reconstruct unrecorded past teaching actions after those conditions have disappeared; migration uses current state and saved arrival counters. Detailed headset readability/comfort remains manual.

**Boundary:** Initial tutorial is guidance, not a mandatory modal walkthrough. Optional sandbox play remains available. Do not implement all future service systems inside this card.

## Later completed foundations

| ID | Verified behavior | Evidence / remaining scope |
| --- | --- | --- |
| D18 | Shared road-network water/service reach | Changelog 2026-09-26; user accepted headset behavior |
| D19 | Aggregate needs and growth diagnostics | Changelog 2026-09-26; saved-city regression accepted |
| D20 | Needs dashboard and diagnostic overlays | Changelog 2026-09-26; presentation/fade accepted |
| D21 | Icon hierarchy, navigation and tool selection | Changelog 2026-09-26; headset interaction accepted |
| D22 | Building silhouettes, entrances and road/art pipeline | Changelog 2026-09-26; user accepted; later art belongs to D38 |
| D25 | Deterministic weather/environment foundation | Changelog 2026-09-28; user accepted |
| D27 | Construction and residential density progression | Changelog 2026-09-27; transitions/save resumption accepted |
| D29 | Terrain/grid projection foundation | Changelog 2026-09-27; user accepted |
| D41 | Bottom drawer, Game Settings, trigger inspection and summary toggle | Changelog 2026-09-28; user accepted overall and final alignment; formal comfort/lifecycle comparison remains D23 |

## D38 — Accepted visual slices; further art deferred

The user accepted fixed/automatic lighting, synchronized sky, building details, night windows, improved road continuity, local grid reveal, precise trigger-toggle wind vane, construction detail and utility meshes through 2026-09-30. Shared materials, instancing and bounded construction components remain in place. Editor/Android checks and individual slice evidence are in the dated changelog entries. Fine grass at the latest scale has no explicit headset acceptance recorded. Remaining freight/shore/weather polish and that check stay in active D38; further art is deferred until more systems are built/tested.

## D37 — Continuous heat maps and spatial diagnosis

**Status:** User-accepted on 2026-09-29. Continuous unlit overlays, exact pointed-cell values, legends, terrain following, building fades and layer restoration are verified by Editor automation and the user's overall headset acceptance. Pollution, R/C/I pressure, power, water and motor-lane congestion are available. Formal comfort/performance checks remain in active D23. Session evidence: [changelog](../CHANGELOG.md#2026-09-29--d37-analytical-heat-map-views).

**Deliver:** Replace discrete pollution/demand markers with bounded continuous-looking overlays for pollution, residential/commercial/industrial pressure, utilities, and traffic/congestion. Entering an analytical heat-map view suppresses zoning tint, road surfaces/markings, and other competing map decoration while retaining only the minimal geometry needed for orientation; leaving restores the prior layers exactly. Add a compact legend and pointed-cell explanation. Preserve exact source values and use shape/intensity plus labels so color is not the sole cue.

**Check:** Overlay values match authoritative snapshots at sampled cells, clamp safely, rebuild only on relevant revisions, remain readable through faded buildings, and can be cleared without affecting simulation. Zoning and road visuals are hidden only while a heat map is active and are restored without stale state. Quest presentation stays bounded and the selected data remains visually dominant.

## Earlier completed foundations

| ID | Task | Depends on | Status |
| --- | --- | --- | --- |
| D00 | Recoverable local source baseline | Working smoke test | Complete (2026-09-24) |
| D01 | Permanent C++ module | D00 | Complete (2026-09-24) |
| D02 | City sandbox map and repeatable build selection | D01 | Complete (2026-09-25) |
| D03 | Logical grid and board picking | D02 | Complete (2026-09-25) |
| D04 | Placement transactions and preview | D03 | Complete (2026-09-25) |
| D05 | Roads, connectivity, and demolition | D04 | Complete (2026-09-25) |
| D06 | Controller tools and inspector | D05 | Complete (2026-09-25) |
| D07 | Comfortable board manipulation | D06 | Complete (2026-09-25) |
| D08 | Fixed-step city clock and economy | D05 | Complete (2026-09-25) |
| D09 | Utilities, zoning, and growth | D08 | Complete (2026-09-25) |
| D10 | Versioned local save/load | D09 | Complete (2026-09-25) |
| D11 | Scenario, pause, and reset loop | D07, D10 | Complete |
| D12 | First Blender kit and remapping round-trip | D07 | Complete (2026-09-25) |
| D13 | Quest benchmark and lifecycle gate | D11; D12 for art benchmark | Complete for prototype; full-content 20-minute rerun deferred |
| D14 | First-playable acceptance | D11, D12, D13 | Complete (2026-09-26) |
| D15 | Zoning rectangle and road live previews | D06, D09 | Complete (2026-09-26) |
| D16 | Inspectable service coverage overlay | D06, D09 | Complete (2026-09-26) |
| D17 | UI information architecture and panel baseline | D14, D15 | Complete (2026-09-26) |
| D28 | Reusable progress and fiscal controls | D19, D21 | Complete (2026-09-26) |
| D24 | Industrial zoning, pollution, and desirability model | D19, D22, D29 | Complete (2026-09-27) |
| D30 | Hydrology and terrain-aware placement | D29 | Complete (2026-09-28) |
| D34 | Glanceable needs dashboard | D17, D32 | Complete (2026-09-27) |
| D31 | Multi-zone density families and local development pressure | D24, D27 | Complete (2026-09-28) |
| D32 | Household economy, goods flow, and parks | D31 | Complete (2026-09-28) |
| D33 | Persistent citizen/household records and pooled transit foundation | D32 | Foundation complete (2026-09-28); follow-up is D36 |
| D35 | Persistent HUD and actionable growth blockers | D34 | Complete (2026-09-28) |
| D26 | Road hierarchy, lane graph, and continuous-segment presentation | D30, D33 foundation | Complete (2026-09-28) |

## Historical first-playable defaults

The earlier cards retain their original scope/checks. Later accepted changes in the changelog supersede them, including D18 road-network reach, D30 per-cell zoning and D41 trigger-selected inspection. These are historical records, not current implementation instructions.

These are provisional implementation defaults. Tune them later in data, not by duplicating constants across UI and simulation.

- Grid: 64 x 64; cells indexed 0..63 on X/Y; 8 logical meters per cell. Unreal logical dimensions use centimeters, so each cell is 800 cm before board scaling. Board local origin is its lower-left corner; a centered display adds an explicit offset.
- First displayed board: roughly 2 meters wide (root scale 200/51200). Clamp visual scale to 1-4 meters board width. Treat this as a UX experiment; the full 64-cell grid will be small until zoomed. The early ambiguous 16 x 16 white overlay was removed after headset feedback; zoning in D09 will communicate usable areas explicitly.
- `CellCoord`, `EntityId` (stable monotonic 64-bit ID), `BuildingDefinitionId`, and integer currency are authoritative. Instance indices and Actor pointers are not IDs or save keys.
- Placement types initially: road 1 x 1, home 2 x 2, shop 2 x 2, power 2 x 2, service 2 x 2. Buildings rotate in 90-degree steps; the schema must support non-square footprints. Add a 2 x 3 test-only footprint to validate rotation.
- Fixed-step simulation: 5 ticks/second, with at most 5 catch-up steps/render frame. Drop excess accumulated wall time under overload and report it in debug stats; never skip economic transactions while pretending those ticks ran. Speeds: paused, 1x, 3x. One month = 300 simulation ticks (60 seconds at 1x).
- Tool states: Inspect, Road, Home, Shop, Power, Service, Zone, Demolish, ManipulateBoard. Preview state is disposable. Commands are validated at commit time and change funds/occupancy atomically. UI consumes the same validation result/reason as execution.
- Cancel discards a preview or unfinished road gesture for free. **Committed undo is deferred**; demolition is a separate priced/refunded command. The initial plan's phrase "undo uncommitted edit" means preview cancellation only.
- Start funds 10,000. Initial tuning: road cost 10; home 200; shop 300; power 500; service 400. Refund 50% of the recorded placement cost on demolition; automatically grown buildings record zero player placement cost. No loan/debt feature; treasury clamps to zero with unpaid upkeep tracked explicitly.
- One runtime module first. Plain C++ state/command code where possible; a world subsystem owns state and ticking. Presentation observes changed IDs/cells. Start single-threaded with small data; add jobs only after profiling.

## D00 — Recoverable local source baseline

**Deliver:** Check current Git state. If still absent, initialize a local repository. Add Git LFS patterns for `.uasset`, `.umap`, `.blend`, `.fbx`, and large source textures; ensure LFS is available before adding binary content. Review `.gitignore` coverage and checkpoint source/config/docs/tools/content. If commit identity is missing, do not invent one; leave staging intact and record the missing identity. Do not add a remote or publish.

**Check:** Dry-run/staged listing contains no generated caches, SDKs, private logs, or vendor plugin. Check plugin restore instructions/hash and preserve the known-good APK under ignored Artifacts. If LFS is unavailable, resolve the local tooling before committing binaries; source implementation may continue while a checkpoint is explicitly pending.

**Boundary:** No gameplay changes. A Git baseline is for recovery, not a requirement to re-download the working plugin.

## D01 — Permanent C++ module

**Deliver:** Add `Source/CitySimVR/CitySimVR.Build.cs`, module entry point, `Source/CitySimVR.Target.cs`, and `Source/CitySimVREditor.Target.cs`. Register the runtime module in `.uproject`; pin UE 5.7-compatible build/include settings using the installed engine as reference. Dependencies start with Core, CoreUObject, Engine; add input/XR/UI dependencies only as used. Generated targets in Intermediate are not the deliverable.

**Check:** Editor Development builds; build script packages the unchanged template APK; device map still reaches play/OpenXR focus if hardware is connected. Record manual headset checks separately. Do not clear all Intermediate as a routine fix; remove only identified stale generated project targets if necessary, after validating their paths.

**Boundary:** No board, simulation, plugin upgrades, or art changes. This is the recommended first lighter-session assignment after D00.

## D02 — City sandbox map and build selection

**Deliver:** A separate `/Game/CitySim/Maps/CitySandbox` with template-compatible XR pawn/game mode, plain floor, lighting, and a placeholder tabletop. Keep `/Game/VRTemplate/VRTemplateMap` intact. Add deterministic editor automation for creating/updating the sandbox if feasible; binary assets must be produced by Unreal, not fabricated bytes. Provide a desktop test mode without requiring XR hardware.

Introduce a general build script/preset for smoke vs sandbox. Preserve the smoke wrapper. Explicitly set both cooked map and runtime default map; archive separately. Retain the existing package ID for development unless a deliberate separate sandbox package is documented. Avoid silently changing the installed smoke-test behavior without noting which map the APK starts.

**Check:** Both presets open the intended map; cook has no missing shared-pack references. Package and deploy sandbox once. Compile/load editor automation APIs against local UE documentation rather than guessing calls.

**Boundary:** A greybox board only. No procedural city generation.

## D03 — Grid, board transform, and picking

**Deliver:** Simulation coordinate/footprint types, bounded occupancy storage, and a board Actor mapping cells to world space. Add mouse ray and controller-ready ray interfaces to the same picker. Highlight one hovered cell. Root transform governs visual translation/yaw/scale; logical state never changes when moving the board.

**Check:** Automated cases for corners, just-outside bounds, parallel ray, transformed board, and rotated non-square footprint. Cell 63 is valid; 64 and negative coordinates are invalid. In the desktop sandbox, pointing at a visible corner yields the correct cell after rotation/zoom. Prefer inverse-transforming a world hit point; don't accidentally use world-space distance as local cell size.

**Boundary:** No placement yet; no save schema beyond the stable coordinate types.

## D04 — Placement transactions and preview

**Deliver:** Data-backed building definitions and shared validate/commit API returning a reason enum and changed cells/IDs. Show ghost, footprint, cost, and explicit invalid reason. Desktop click confirms; cancel discards. Render placed greyboxes with instancing and stable ID lookup. Implement treasury and placement costs only; recurring economy belongs to D08.

**Check:** Reject overlap, outside grid, insufficient funds, stale preview, and double-commit without partial changes. Valid commit deducts once and occupies all cells. Cancel costs zero. Rotation 2 x 3 maps correctly. Deleting an instance later must not make another building resolve to the wrong ID.

**Boundary:** Road-access and utility failures restrict operation/growth in later cards, not basic manual placement, unless explicitly described otherwise.

## D05 — Roads, connectivity, demolition

**Deliver:** Start/end gesture creates a deterministic X-then-Y orthogonal road path. Existing road cells are reused without another charge; other occupancy rejects the whole gesture. Price/validate the entire unique new-cell set before commit. Four-neighbor bitmask selects straight/corner/T/cross placeholder visuals. Initial fixed map entrance is boundary cell (0,8); roads touching it form the connected network. A building has road access when any footprint edge touches that network.

Demolition removes a selected entity, clears every occupied cell, applies the configured refund once, updates instance lookups, and invalidates connectivity. Roads may be removed cell-by-cell. Use bounded BFS for this small board before implementing a complex dynamic graph.

**Check:** Straight and L paths, crossing/reused roads, T junctions, invalid whole-path rollback, disconnected island, bridge-road deletion, edge adjacency (diagonal does not count), and instance-index changes. Package sandbox after this card to catch mobile rendering problems before adding VR UI.

## D06 — Controller tool palette and inspector

**Deliver:** Map Enhanced Input actions to inspect, select tool, confirm, cancel, and rotate preview. Dominant hand points; other hand opens a simple world-space palette. Support handedness swap. UI focus consumes input before tools; inspection reports entity ID/type/cost and current eligibility. Desktop and VR issue identical commands.

**Check:** One trigger press creates one command; clicking UI cannot also build behind it. Test cancellation mid-road, invalid reasons, left-handed mapping, and both controllers. Editor compile plus Quest package/install; user evaluates panel readability and hit targets.

**Boundary:** No polished artwork, complex menus, or hand-only controls.

## D07 — Board manipulation and comfort

**Deliver:** Explicit manipulation mode with one-hand drag, two-hand yaw/uniform zoom, height adjustment, recenter, and scale clamps. Keep pitch/roll level; head motion remains tracked independently. Release either grip cleanly reinitializes the gesture. Do not allow building during manipulation. Save board presentation preferences separately from city coordinates.

**Check:** Enter/exit mode while holding buttons, lost tracking, releasing one hand, min/max zoom, recenter at seated height, pointer accuracy after transform. Quest checks seated/standing and five-minute interaction comfort. Document results, not assumed ergonomic compliance.

## D08 — Fixed-step clock and economy

**Deliver:** World subsystem owns the pure state and bounded fixed-step loop. Model occupants/jobs in aggregates. Initial tuning per month: each occupied home supports 10 residents; each operational shop supports 10 jobs. Income is 5 per resident plus 20 per operational shop; upkeep home 5, shop 10, power 50, service 30. Centralize these values in definitions/tuning data. Track billed month IDs so pause/save/speed changes cannot double-bill. Unpaid upkeep records shortfall and blocks growth next month; do not silently erase the expense.

**Check:** Equivalent tick sequences at different render rates yield matching state; pause is stable; accelerated time bills once/month; hitch catch-up stays bounded; treasury never overflows or becomes negative; no per-entity tick actors. Current residents are fixtures until D09 supplies growth.

## D09 — Utilities, zoning, and growth

**Deliver:** Paint Residential/Commercial zoning on empty cells; zoning itself costs zero. Built footprint reserves its cells while retaining zone metadata. Fixed stable scan order plus a saved PRNG seed chooses candidates. Build a 2 x 2 structure only if all cells share the zone, are empty, touch connected road, and receive power/service. Spawn at most one building each 25 ticks.

Prototype utilities: a road-connected power building supplies up to 200 residents/jobs combined; a road-connected service building covers cells within Manhattan distance 12 of its anchor. Evaluate coverage at building anchor; use the same rule for preview, growth, and inspector. Capacity allocation follows stable entity ID order. No wire/pipe network yet.

Growth heuristic: bootstrap with one eligible home, then add shops while jobs < residents; add homes when job capacity >= residents. Home capacity is 10, shop jobs 10, so a connected serviced neighborhood can reach the 100-resident objective. Disabled buildings retain their structure; report no-road/no-power/no-service/shortfall reasons and halt growth. Keep this deliberately simple and tunable.

**Check:** Seeded run repeats; no overlapping births; service/capacity limits; utility demolition updates eligibility; disconnected zones don't grow; valid fixtures reach 100 residents with adequate space and funds. Check render updates occur only for changed entities.

## D10 — Versioned save/load

**Deliver:** Local save schema v1 with dimensions, definitions/IDs, placement costs, zoning, roads, buildings, balance/shortfall, clock/month, PRNG state, scenario state, and next entity ID. Serialize authoritative state; rebuild visuals/connectivity. Validate loaded state into a temporary model before replacing the live city. Write temporary save then atomically replace and retain last-known-good backup. Report failures in UI; never clear the current city on bad input.

**Check:** Round-trip equality and deterministic next growth step; malformed/truncated data; overlapping occupancy; unknown content ID; duplicate IDs; incompatible schema; interrupted write leaves an older valid save. Android sandbox storage test, not just Windows. No cloud save.

## D11 — First scenario and lifecycle

**Deliver:** Start/reset/pause/1x/3x/save/load controls, clear objective display, and scenario success for 100 residents plus three consecutive profitable simulation months. Profit means operating income minus billed upkeep, excluding construction/refunds. Reset confirms discarding the current unsaved city. Headset removal/app background pauses simulation and cancels input gestures; return requires fresh button edges.

**Check:** End-to-end empty start -> roads/utilities/zones -> growth -> success; reload mid-objective; losing profit resets the streak; background/resume doesn't fast-forward or commit stale gestures. This is the first complete gameplay loop.

## D12 — Blender kit and material remapping

**Deliver:** Follow `ASSET_PIPELINE.md`: calibration cube, one home, shop, utility, road junction kit, and shared material instances. Implement manifest-driven import validation/remapping with dry-run report. Use original simple Blender work first; ComfyUI is optional and cannot block this task.

**Check:** One-meter cube imports as 100 cm before board scaling; pivots/footprints align; names map to definitions; slot mapping survives re-export; missing slots fail clearly; normals/LODs look correct in headset. Save `.blend`, manifest, and exporter settings. No large asset batch until round-trip works.

## D13 — Performance and remaining M0 checks

**Deliver:** Reproducible 100/500/1,000-lot scenes with requested vs actually placed counts, configurable roads/vehicles, and measured device results. Record device OS, build hash/configuration, resolution, refresh rate, CPU/GPU times, memory, dropped frames, and 20-minute thermal behavior. Verify suspend/resume and input focus. Start with native 72 Hz; 11 ms CPU/GPU is a proposed headroom goal, not existing evidence.

**Check:** Benchmark setup is deterministic and fits available grid cells. High tiers may exceed the playable scenario's scope; label them stress tests. If a tier fails, reduce visibility/shadows/material variation/work per frame and repeat that tier; set an evidence-based city cap. Do not mask failure by lowering refresh rate or relying on reprojection without documenting a changed requirement.

## D14 — First-playable acceptance and next scope

**Deliver:** Clean-start scenario playthrough, concise tutorial prompts, readable service failures, saved city recovery, no stale selection after demolition/load, asset round-trip evidence, and performance results. Document remaining bugs ranked by impact.

**Check:** A new player completes the scenario in headset; build/deploy instructions work from the current source checkpoint with the documented plugin restore. Keep M0/M1/M2/M3 statuses tied to their recorded evidence. Only then plan expanded traffic, terrain, districts, more services, or mixed reality.

## D15 — Zoning rectangle and road live previews

**Deliver:** Replace the fixed zoning brush with a two-trigger inclusive rectangle gesture. Show every selected cell while the second corner follows the pointer, report dimensions/count, switch residential/commercial without losing the anchor, cancel without mutation, and commit the rectangle atomically. After the first road trigger, show the deterministic path continuously from the anchor to the current pointed cell, including valid/invalid feedback, cost, and cancellation.

**Check:** Rectangle bounds are identical in every drag direction; a one-cell selection remains valid; occupied or out-of-bounds rectangles do not partially repaint. Road preview cells match the authoritative X-then-Y validation path and disappear on commit, cancel, tool change, manipulation, reset, or load. Verify overlay height, color, readability, and pointer responsiveness in headset.

## D16 — Inspectable service coverage overlay

**Deliver:** While Inspect points at a service building, visualize the exact authoritative coverage cells without changing simulation rules. Use the shared Manhattan-distance predicate, clip at board bounds, state that distance is measured from the service anchor, and distinguish an operational road-connected service from a disconnected service's potential coverage. Cache the overlay by inspected entity rather than rebuilding hundreds of instances every frame.

**Check:** The center diamond, inclusive boundary, excluded diagonals, and edge clipping match the same predicate used by placement and growth. In headset, confirm the overlay is readable over residential/commercial zone colors, disappears when inspection leaves the service, and makes an apparent out-of-range growth failure understandable.



## D44 — Starter civic lots and parking (verified; user accepted overall 2026-09-30)

Police/fire starter bodies occupy 14x10 m within their original 24x16 m reserved lots, with parking and matching static vehicles. The user accepts the smaller footprint/expansion direction and fire-engine preview; requested red/blue police caps are implemented in the current D44/D45 integration, with the new appearance check retained in DEVELOPMENT_TASKS.md. Prior Editor/72 tests/import/reimport/Android install/startup evidence: `Artifacts/Diagnostics/D44-station-lots-*.log`, `Artifacts/AssetImport/D44-station-lots-import.json`, APK SHA256 `4B74B20D4F6F0758C27185021298D1EF4341C9452258965F382B4A90DB6A0729`. Vehicles remain static decoration; paid upgrades are D48.

## D44/D45 — Civic teaching and fire authority foundation (automated verification 2026-09-30)

Schema25 persists police/fire teaching milestones, seeded fire clocks/incidents, bounded damage and one-charge repair deadlines. Old schema24 cities migrate with fresh fire warning state. Fire risk/response/spread/recovery use sorted logical records and saved weather; damage disables capacity and employers, feeds actual unemployment/crime and preserves citizen identities/wallets. Focused four-case fire suite and the full 76-test suite pass with zero failures; Editor warnings-as-errors, civic geometry/import/reimport and RGB/alpha light-mask checks pass. Logs `Artifacts/Diagnostics/D45-{editor,tests,import,reimport}.log`; measurements `Artifacts/AssetImport/D45-civic-import.json`. Only the automated/source foundation is archived here: the active D45 card retains Quest integration, device appearance/interaction and rate/comfort checks. Staffing/payroll and moving response vehicles remain active D44 work.

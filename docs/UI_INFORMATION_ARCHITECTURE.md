# City UI information architecture

Updated 2026-10-02 with D49/D50 protected creation and save selection; D48 upgrades are user-accepted. D41 is accepted overall; formal comfort/lifecycle comparisons remain D23. Authoritative values and rules remain in the simulation layer.

## Entry menu environment and setup (D49)

The sandbox opens Continue, Load, New City and Settings against a dedicated static sky dome. Tools is removed. The board, sandbox floor/sky and other scenery are hidden in game; gameplay time/objective/needs/inspection/condition/debug presentation is masked separately, so its normal refresh cannot show through the menu. Hidden flags are retained and restored rather than forcing previously hidden objects visible. The city remains resident and its authoritative ticking is suspended throughout menu browsing without changing its saved pause/speed preference. Controller tracking and menu rays remain available.

Menu environment stays active through Settings, Lighting, setup and load review. Entry Settings omits the gameplay category bar. Back on the main page remains there; off-hand/dominant grips cannot reveal/manipulate the board, and changing hands preserves the screen. Suspend hides the panel and retains the sky/suspension; foreground reopens the main page with the existing fresh-release gate. End-play restores hidden states/suspension. Late-created scenery is masked too. The menu always uses a centered fixed viewport panel; View/Stable/Hand preferences continue to describe the gameplay drawer after return.

Continue prepares city presentation before removing the sky/masks and retains the current city/time choice. Load requires separate review: LOAD SAVED explicitly discards unsaved progress, restores a validated city and goes straight into the map paused; SAVE & BACK writes the current city to its own ID-based file and returns to the main menu; Cancel keeps city/environment. Settings Load and desktop F9 use review. Failed load keeps the city and sky, with status. The rebuilt model/terrain is prepared while hidden to avoid revealing stale geometry for a frame. Desktop clicks use the same menu targets; misses cannot issue map commands. Browsing/loading never writes the local save automatically.

New City opens a Normal draft with a generated city name and displayed fresh terrain seed. Relaxed/Normal/Challenging consume the shared authority evaluator; NEW TERRAIN rerolls the draft seed/name while preserving difficulty. REVIEW retains the selected draft and offers CREATE CITY, SAVE & BACK or Cancel. Create validates a fresh candidate and its serialized round trip, writes a separate atomic city file, then replaces authority and prepares/reveals the map paused. Cancel from review returns to setup; leaving setup or suspending discards it. Saving before switching is explicit. Current city rules/funds/terrain/files remain intact during browsing.

Load offers Previous/Next over up to32 valid city saves with name, preset and last-save time, including recoverable backup-only entries. IDs determine filenames independently of display names; names never become paths. Schema35 stores identity, rules version and eight incident/recovery values. Legacy saves receive Existing City / Normal; saving creates a named copy under SaveGames/Cities and retains CitySaveV1.json. Discovery currently rebuilds the library from individual documents, so no damaged index can hide valid cities. Editable names/rename, graphical terrain preview and an indexed library remain active D50 follow-ups.

Reuses the18-entry pool and existing unlit sky/UI materials. One static sphere and one material instance belong to the shared controller; no scene-specific actors are added per building/citizen and no new shader is authored. User accepts the original menu/load slice; sky-scene appearance and transitions remain active checks in DEVELOPMENT_TASKS. Formal comfort/performance remains D23.

## Paid station upgrades (D48)

With Inspect selected, trigger a police/fire station to open its normal tooltip. A bottom UPGRADE $cost button offers the next stage. Trigger the button to review exact price, at-most-half-day construction, staff, upkeep plus payroll, utility demand, road reach, protection and crews; it changes to CONFIRM UPGRADE $cost. Confirm on the button to pay once. Review names unavailable funds, operation or unpaid bills. Hover, initial inspection and review change no funds. Body clicks are captured without selecting the map underneath. The standalone City upgrade menu/tool has been removed.

Opening the drawer cancels purchase confirmation while preserving normal Inspect selection. Cancel, a different inspection selection, tool change, manipulation, lifecycle dismissal, demolition or load/reset invalidates confirmation. Hidden/stale panels have no hit target; nearer buildings occlude it. Under-construction, upgrading and fully upgraded stations show labeled non-purchasing states. Construction shows fractional game days and retains current-stage operation under its normal rules; overdue damage/repair names the recovery blocker. Inspection exposes completed stage, current staffing/required jobs and stage-specific reach/upkeep/protection. The City page has seven entries within the existing18-entry pool; one shared button/label is added to the existing inspector. Both XR and desktop use the same ray/panel action. No permanent HUD or marker Actor is introduced. The user accepts station functionality/stage appearance and the in-panel Upgrade/Confirm interaction; D48 is complete. Formal D39 balance and D23 performance/comfort remain separate.

## D46 staff education summary (2026-10-01)

The optional right-side city summary adds two rows: STAFF educated/uneducated totals, then Basic/Secondary/Higher counts. Educated includes Basic and above; uneducated means None. Counts describe current workers assigned to private or civic buildings, excluding unemployed residents and vacancies. School progress can raise a worker's tier while their entry job remains unchanged; qualification and skilled-role occupancy stay distinct.

The panel consumes the shared derived workforce snapshot. Existing text-bound layout moves utilization/demand bars below both added rows and expands the background; visibility and the top-right toggle retain their behavior. Device readability and exact counts are active D46 checks. Monthly/annual reporting of these same metrics is queued under D42.

## D39 industrial tax controls (2026-10-01)

City > TAXES has three columns: residential on the left, commercial in the center and industrial on the right. Each has minus/plus, a draggable rail/fill/thumb and its exact pending percentage. Green/blue/yellow follow zone conventions. All rates use0–25%,1% steps and default10%; minimum stays screen-left and maximum screen-right in View, Stable and Hand modes. Compact labels use text size1.2. Apply commits all three rates atomically; Cancel, grip dismissal, category changes, manipulation and lifecycle dismissal use the shared pending-edit discard path.

The fixed panel retains its footprint; only the hand Taxes panel widens from half-width17 to27 for three columns. The bounded entry pool is18: seven category entries plus eleven Taxes actions. Apply and Cancel occupy the bottom row. The footer shows pending monthly net and the0–25% range/demand tradeoff. Summary shows separate R/C/I tax forecasts. Pending preview changes no policy or treasury. Automated containment, label bounds, slider direction and Apply/Cancel checks pass; worn-headset readability and interaction remain D39 checks.

## D39 budget recovery refinement (2026-10-01)

When growth is blocked by a monthly expense shortfall, the chart summary labels it UNPAID BILLS and shows last-month income, bills and the unpaid amount. Current costs are split into roads, utilities, other buildings and public wages; current monthly net is labeled Forecast. The guidance names City > TAXES, Apply, reducing unused infrastructure and running until a month is fully paid. Once bills are paid, insufficient construction cash has its own amount/action. High demand does not promise affordable growth.

Taxes uses the three policy sliders and Apply/Cancel, with a two-line footer showing the pending policy's forecast net and the demand tradeoff. Forecasting uses the shared economy calculation and cannot change policy or spend cash; actual billing still uses completed purchases. The tutorial's upkeep recovery names the same navigation and timing. Expanded recovery text pushes the existing read-only bars below its rendered bounds; the chart background contains both. Headset readability and recovery checks remain pending in D39.

## Civic safety controls (D44, 2026-09-30)

Build > POLICE uses normal footprint/cost/construction controls. City > CRIME VIEW toggles the shared heat-map presentation; Overlays > CLEAR also clears it. Putting the new safety view under City kept the then-sixteen-entry pool and existing overlay navigation bounded; D39 expands it to18 for industrial policy. It displays exact risk plus grace/incident state, job shortage, homelessness and protected exposure units. Inspection adds building risk/protection and station capacity/upkeep; existing home/business metrics stay intact. No police staffing metric is fabricated: this first slice has fixed prevention capacity. Device readability/interaction checks stay active in D44.

Live objectives show police placement once crime is introduced, or station restoration if one exists but cannot operate. Infrastructure recovery still has priority. The accepted ten persisted teaching IDs are unchanged; completed police teaching history is subsequent D44 work. Fire controls will be added with D45 implementation.

## Objective card and debug visibility (2026-09-30)

The live D43 card is a separate upper-left unlit panel with one current action, reason, contextual hint, counts and progress. Header trigger collapses/reopens it; initial body trigger starts scenario tracking while retaining the city. Drawer/manipulation hides it temporarily. Its TextRender body uses single line breaks: empty lines use unscaled font height and can push text off the background. Tests check actual text bounds for every guide state. Schema 22 saves stable completed milestone IDs and the next teaching stage. Live recovery prioritizes a lost entrance, power, water or basic-service shortage without removing history. Completion follows authoritative conditions, including actual shop purchase arrivals. Already-satisfied steps count when starting or loading an existing city. No rewards or scenario thresholds change. Later civic-service stages remain future work.

The footer offers HISTORY once a milestone is completed; review shows one completed step at a time with NEXT to cycle and LIVE to return. Its two pooled trigger targets are separate from the body; hidden/collapsed targets pass rays through. The card background extends around the footer. Collapse and review selection are session-only; save/load preserves teaching progress. Pending device checks are tracked in D43.

Game Settings offers DEBUG INFO: OFF/ON, hidden by default on each launch. It controls engine on-screen debug text and the Quest input monitor; recorded diagnostics continue. Enabling uses the existing debug location; disabling removes the monitor. The engine display flag is restored when the controller ends play. To keep Settings compact, lighting choices are in the LIGHTING submenu. Debug and lighting mode remain session-only.

## D38 lighting controls (2026-09-29)

Game Settings > LIGHTING offers Day, Dusk, Night and AUTO LIGHT; Back returns to Game Settings. The active choice uses the selected-button treatment and each has hover help. Automatic is the default: a full cycle spans two game months (120/40/24 seconds at 1x/3x/5x), uses fractional calendar progress and smooth transitions, and freezes on pause. Fixed presets override the cycle immediately, including while paused. Returning to Auto eases to the current calendar phase when running. The selected mode is session-only; saved cities retain their existing calendar clock. Seven Settings entries or five Lighting entries plus the seven-button toolbar fit the sixteen-component pool. On 2026-09-29 the user confirmed all three fixed presets look good on the headset; automatic lighting is also user-accepted overall. The sky backdrop now follows the same modes, clock and pause behavior without extra controls; the skybox is user-accepted overall. Warm window emission now follows the same lighting controls and pause behavior, without additional UI. Weather controls remain future work.

## D41 review and proposed bottom drawer

**Accepted baseline and settings refinement (2026-09-28):** The user reports the bottom drawer works very well in-headset. Game Settings now has a direct gear-icon entry at the right end of the tool bar and in the hand palette. It owns menu position, icon-only Save/Load, and double-confirm Reset. City owns city policy, currently Taxes. Settings is the home for future implemented game-wide options; do not add nonfunctional placeholder controls. The gear uses the pinned Apache-2.0 Material Symbols `settings` asset, shared masked/unlit material, and hover help. The settings follow-up still needs a headset spot check; the initial positive feedback does not claim a formal three-mode comfort/lifecycle comparison.

The current code supports this refinement without changing simulation commands. `CityPaletteModel` supplies page entries/actions; `CityVRToolController` owns attachment, hard-coded two-column geometry, ray-plane picking, help, action dispatch, grip state, and board manipulation. Headset status and inspection already attach to the camera. The palette currently follows the off-hand grip position and turns toward the headset. It has nine pooled entries and fixed surface bounds, so changing only its attachment would produce a relocated tall panel rather than a bottom drawer.

Proposed arrangement, shown from the player's view:

```text
              ZONE:  Residential | Commercial | Industrial
              Shared help / exact cost or relevant action detail
    Inspect | Build | Zone | Overlays | City | Demolish
```

- Keep the category bar stationary within the open surface; replace only the drawer content when changing categories. Build initially retains Roads plus the existing construction options; Roads drills into Dirt/Local/Avenue/Highway with a visible Back action. Zone exposes all three types directly. City retains Taxes; the gear opens Game Settings for menu position, Save, Load, and guarded Reset. Existing time controls remain in their current HUD location.
- Open on off-hand grip at the root state; select a category to expand upward. Keep the selected tool visibly marked. Release closes; the next fresh dominant trigger acts on the city. Opening or hovering never executes a command. Start with the whole bar hidden while grip is released to limit permanent lower-view obstruction.
- Render at a finite stereo depth independent of board scale. Prototype starting values: approximately 80 cm ahead, lower-center, with depth, vertical offset, width, and target size tunable together. These are experiment values, not validated comfort dimensions. Ensure the expanded drawer does not overlap the lower-left inspector or cover the active board target unnecessarily.
- Compare **View-following** (camera-relative position and orientation) with **Stable-on-open** (capture a viewer-facing world pose on grip press; reopen recenters it). The first matches the requested sticky display behavior. The comparison tests whether head motion makes pointing or looking toward the lower controls harder. Avoid adding smoothing until the comparison identifies a need.
- For D41, opening the drawer captures construction input even when the ray misses a button or hits a gap. Today's router deliberately permits board confirmation when an open palette is missed; change this explicitly and update its test. Require a fresh trigger after dismissal so release/selection cannot place or demolish on the board. Preserve dominant-grip board manipulation priority, closing the drawer and discarding pending tax edits on that transition.
- Derive rendered rectangles and ray hit bounds from the same layout description, including the full bar/drawer background. Preserve generous targets and viewer-facing icon axes. Keep bounded components; do not duplicate simulation logic or rebuild the entire UI stack for the experiment.

The first prototype reuses current native components and action dispatch. A later Blueprint/UMG composition pass can use the same action/layout boundary if required. Quest acceptance must compare repeated selection tasks, occlusion, grip effort, and head motion with the existing hand panel. No anchor or visibility choice is accepted solely from a desktop mockup.

### Implemented prototype controls

- Default: **View**. Hold off-hand grip, select a category with the dominant trigger, choose a tool, then release grip to use it. B/Y navigates Back while open; A/X does not rotate a board preview while the drawer is open. Reopening returns to the root bar and retains the selected board tool.
- **Game Settings (gear) → MENU: VIEW/STABLE/HAND** cycles anchor modes. The label shows the current mode. Stable captures a new pose on each opening; Hand restores the original two-column hand layout. All modes use the new modal input capture for safety. Mode choice is session-local and defaults to View on relaunch.
- The drawer uses at most18 pooled entries (seven bar entries plus at most eleven drawer actions); the original D41 slice used16. The hand layout continues to use only its current page. Road Back returns to Build; Settings Back returns to the root; Taxes Back returns to City. Leaving Taxes discards uncommitted edits; Apply commits before leaving.
- Starting geometry: 80 cm ahead, bar 28 cm below eye position, 84 cm total background width at scale 1 (extended on the right for the gear; existing tool positions retained). Root bounds are local Z -9 to +8 cm; expanded bounds are -9 to +31 cm. These are adjustable prototype dimensions, not measured comfort results.
- Development console variables: `citysim.Ui.DrawerDistance` (50–150 cm), `citysim.Ui.DrawerHeight` (-60–0 cm), and `citysim.Ui.DrawerScale` (0.6–1.5). Scaling changes geometry and hit targets together. Stable mode samples these values when reopened. Hand mode keeps its established size.
- Existing time/needs/inspection HUD surfaces remain separate. The drawer blocks time-control clicks while open so a missed menu target cannot activate a different surface. Inspect and Demolish remain direct bar actions, and the selected tool is also reported by the existing summary HUD.

### Quest comparison checklist (pending)

Repeat the same actions in View, Stable, and Hand: residential rectangle; each road class and Back; power/water placement; overlay then Clear; tax edit then release/cancel; tax Apply. Test seated and standing, each dominant hand, a moved/scaled board, ordinary head turns, transition to both-grip manipulation, and suspend/resume. Record misses, accidental construction or time commands (required zero), readability/overlap with the inspector, board obstruction, selection effort/time, and grip fatigue. Confirm asymmetric icons and conventional tax slider direction in each mode. Choose the preferred anchor after this comparison; permanent visibility and grip-toggle behavior remain follow-ups.

### Review limitations and plan drift

The active queue and working source are newer than the most recent Git commit (`f826685` at review). D26 road classes and the initial D36 traffic files are present in the working tree; Git history alone understates progress. The review did not rerun builds or tests. This paragraph records the D17-era review, not the current queue. Later documentation consolidation removed the old first-playable plan and updated ARCHITECTURE.md to the permanent module/record-based simulation. Use DEVELOPMENT_TASKS.md and current road/population contracts for ongoing work; D41 completion is recorded in CHANGELOG.md.

## Panel roles

- **Clock controls:** the calendar and play/speed controls remain available at the top of the view.
- **Needs and summary dashboard:** a hidden-by-default upper-right panel toggled by a chart icon above the dashboard on the right, vertically aligned with the top-center play/speed controls. It combines city summary information, capacity/use/shortage, demand, and concise growth blockers. Hover help names Show/Hide city summary; only a dominant trigger toggles it. Closing tools never reopens this dashboard. A visible dashboard consumes ray input over its background; a hidden one has no hit target. Its visibility is session-local.
- **Hand palette:** an explicit grip-open surface for construction, zoning, simulation controls, persistence, and later diagnostic-overlay selection. It follows the off hand and supports either dominant hand.
- **Context inspector:** in Inspect mode, dominant trigger selects a building and opens one dark tooltip above it, facing the headset. A thin leader links it to the selected ray hit. Triggering the same building closes it; another building switches it; empty ground or B/Y dismisses it. Hover never opens or changes the tooltip. It retains the existing detailed building information, wraps long lines, and scales with headset distance rather than board scale. Tool changes, demolition, load/reset, and suspend clear stale selection. Opening tools or manipulating the board temporarily hides the tooltip. Building-linked service diagnostics follow the selected entity in VR; explicitly selected analytical overlays remain independent.

All drawer, dashboard, tooltip, and clock text uses the shared `M_CityTextUnlit` material. Panel backgrounds use dark `MI_CityUiPanel`; buttons, rails, tax fills, and icons use the shared unlit UI materials. World lighting must not shade these surfaces. The inspector uses rendered instance bounds for trigger picking and a stable authoritative entity ID for lookup; mesh bounds never author occupancy or commands. No per-building tooltip actors or per-frame citywide picking are introduced.

The September 28 contrast/context follow-up requires a Quest check of bright/dark lighting, chart-icon reach, open/closed dashboard input capture, tooltip text/bubble/leader alignment at near/far distances and board scales, same/different-building triggers, empty-ground dismissal, and save/load/deletion cleanup. Desktop material and interaction tests do not establish headset readability or comfort.

The current native `TextRender` components are an incremental implementation. These roles and their simulation-facing snapshot boundary should survive a later UMG composition pass.

## Metric display contract

Bars represent measured values with named bounds, not decorative or guessed demand.

| Metric | Visual rule | Source milestone |
| --- | --- | --- |
| Monthly net | Signed bar centered at zero; positive and negative sides use direction, label, and color | Existing monthly ledger / D20 |
| Housing | Occupied residents over housing capacity, with explicit shortage text | D19 snapshot |
| Jobs | Filled jobs over job capacity, with explicit job shortage or surplus text | D19 snapshot |
| Power | Usage over capacity; zero capacity is an explicit state | D19 snapshot |
| Service | Usage over capacity, with total demand and unserved count | D18 reach plus D19 snapshot |
| Residential/commercial demand | Separate unlit, zero-centered visual bar and explicit `+`/`-` value; residential green and commercial blue pressure oppose each other and are never inferred from zone color | D19 snapshot |

Every bar clamps its fill to its visual bounds while retaining the true labeled number. Direction, icon/shape, and text must repeat color meaning. Residential green, commercial blue, warning amber, and deficit red are semantic accents rather than the sole cue.

## Reusable progress and adjustable-control contract

A progress bar is a read-only visualization; a slider/stepper is an input control. They may share track/fill geometry, typography, and materials, but never input semantics or an ambiguous component type.

- Read-only progress always names the measure, current value, bound, and state. Construction progress can show `Building 45%`; utility use can show `Power 80 / 100`; neither accepts pointer input.
- Adjustable values show exact current/min/max values and a unit. Fiscal percentages are stored and commanded as integer basis points, then formatted for the player; fill width is derived presentation only.
- Every adjustable row exposes large minus and plus targets for reliable one-step changes. Its visual slider uses a neutral rail, semantic-color fill from minimum to the pending value, and a movable thumb; the larger invisible trigger target remains available for reliable coarse dragging, followed by stepping for precision.
- Opening the policy page starts a pending edit. Minus/plus steps and trigger-held track dragging change only the pending value; trigger release ends a drag, `APPLY` sends one authoritative command for both rates, and `CANCEL`, palette close, handedness swap, or suspend restores the last committed values. Hovering never changes state.
- Disabled, focused, pending, committed, and invalid states use shape/depth/text as well as color. Shared hover help states the increment and consequence.
- Projected effects and realized ledger values remain visibly distinct. For example, `Projected tax revenue` cannot be labeled as treasury or last-month income.

D28 first applies this control family to residential and commercial tax rates, then D27 reuses the read-only form for authoritative construction/upgrade progress.

The initial two-column D28 tax policy (superseded by the D39 three-column controls above) defaults to 10.00%, permits 0.00%-25.00%, and steps in 1.00% increments. Residential controls occupy the viewer-left column and commercial controls the viewer-right column, with shared Apply/Cancel actions below. Each column has a continuous-looking rail, colored fill, and thumb backed by discrete basis-point snapping; the exact basis-point value is authoritative. The conventional direction is invariant under the viewer-facing transform: minimum is screen-left and maximum is screen-right. Residential uses the established green accent and commercial uses blue. `TAXES` lives under `CITY`, while the compact icon-only Save/Load actions now live directly under `GAME SETTINGS` (D41) so the established component and root-page limits remain bounded.

The needs dashboard derives an overall signed demand balance from the existing bounded residential and commercial demand inputs without changing growth eligibility. Native unlit geometry replaces the earlier ASCII demand tracks: the center divider is zero, left is negative pressure, and right is positive pressure. Text always repeats the sign and magnitude so direction does not depend on color or bar shape alone. The reusable material family reserves yellow for industrial demand; that third bar stays hidden until D24 adds authoritative industrial demand rather than presenting a fabricated value.

## Control icon contract

Compact action controls are icon-only. They reserve screen space for city information and expose a short explanatory label in the shared hover/focus help area instead of repeating text on every button.

- Save: downward arrow entering a tray; hover help reads `Save city`.
- Load: upward arrow leaving a tray; hover help reads `Load city`.
- Start/resume: right-facing play triangle; hover help describes the current action.
- Reset: circular arrow with a distinct destructive treatment; hover help explains the required second confirmation.
- Back: a distinct back-chevron for submenu navigation. The grip-held palette has no Close button because releasing the off-hand grip already hides it; pending tax edits retain an explicit Cancel action.
- Game Settings: outlined gear; hover help names menu position, save, load, and reset. This is a direct root/bar action; changing anchor mode stays on the settings page.
- Hover, armed, and selected states change border/shape treatment as well as color. Icons must remain distinguishable without color.

D21 owns the shallow palette hierarchy and final control arrangement. It preserves the proven trigger, grip, handedness, and face-button routing.

## Reusable icon asset contract

Use the vendored Google Material Symbols outlined set for future UI actions unless a task records why the existing visual language cannot express the action. Material Symbols are Apache-2.0 licensed; do not copy an icon from an arbitrary website, operating system font, or editor installation. Keep all of the following together in a change:

- Exact upstream SVG under `ThirdParty/MaterialSymbols/svg`.
- Apache-2.0 license and pinned upstream revision under `ThirdParty/MaterialSymbols`.
- Transparent 256 px derivative under `ThirdParty/MaterialSymbols/Derived`.
- Unreal texture `T_Icon_<upstream_name>` and material instance `MI_Icon_<upstream_name>` under `/Game/CitySim/UI/Icons`.
- Action mapping, short shared-hover help, automated asset assertion, and headset direction/readability evidence.

Run `Tools/Prepare-CityUiIcons.ps1` after adding an SVG to regenerate transparent PNG derivatives, then add the name to `Tools/Create-CityUiIcons.py` and run `Tools/Create-CityUiIcons.ps1` to import it. The generated instance must use the shared two-sided, unlit, masked `M_CityUiIcon` master and its `IconTexture`/`IconTint` parameters; do not create a unique shader per icon. Text glyphs remain fallback-only.

The engine Plane mesh image axes require the established viewer-facing transform in `CityVRToolController`: image-right and image-up map to viewer-right and viewer-up while the plane normal remains palette-forward. Do not infer screen direction from palette-local +Y/+Z. Directional icon tests must assert all three transformed axes, and the device check must include at least one asymmetric horizontal icon (Back), one vertical icon (Download or Upload), and Play. This prevents an icon from being flat but rotated or mirrored.

Icon-only controls always provide shared hover/focus help. White is neutral, yellow is hover/focus, cyan is selected, and red is destructive; button depth repeats state so color is not the only cue. Preserve generous hit targets independently of the visible icon size.

## Graphical building inspection (2026-09-29)

D116 adds a projected Desktop condition layer in the existing HUD Blueprint and compact world-facing VR icons. Both consume the same read-only condition model. One primary symbol and a secondary count replace persistent prose above lots; compatible local overlaps cluster, with a lot count. The 16-marker presentation limit preserves the full affected-lot list. Desktop's Next issue control and Settings > Next issue on both platforms reach the remaining/offscreen lots. Ordinary building/zoning keeps alerts visible; direct marker picking requires idle Inspect. Entry/modal/manipulation and camera/edit gestures gate selection; marker selection never repairs, spends or places anything. A press cannot retarget to a different entity/session after the layout changes.

Selected inspection shows current operation failures, settled purchase concerns and recorded sustained/search state separately. Cash/stock/free retail slots/current reserved-shop walk routes use existing eligibility and traffic rules; current observations never claim to prove last month's cause. A related-lot button can inspect the actual reserved shop or nearest connected water source (labeled as such, not an invented allocation). Source/route/workforce detail remains in the existing sections. Recorded recovery/relocation/departure cues reuse the existing housing history for the current settled month and retain source identity after citizens leave. Unemployment alone creates no warning or happiness penalty.

The additional 13 outlined symbols are user-approved D116.A1 v1; exact upstream/source and derivative hashes are in `ThirdParty/MaterialSymbols/D116-Sources-v1.json` and `D116-Approval-v1.json`. `Tools/Import-D116ConditionIcons.py` verifies the approved bytes and reuses the existing importer/material contract. Desktop condition glyphs use these same textures; ordinary toolbar icons retain their native vector helpers. Physical mouse, PCVR/Quest direction, readability and comfort remain device observations, never inferred from offscreen captures.

Keep selection on trigger. A building-anchored unlit card presents its name/density, operational blocker, cash and stock/transit summary. Up to three labeled bar rows show exact numerator/denominator values. Business rows are staffing now, factory output next month (or sales last month), and payroll last month. Homes show residents, purchases last month and desirability. Zero capacity draws an empty rail; color supplements values rather than carrying meaning alone. Factories also report actual goods made last month and cumulative exports. Construction status takes priority over an operational label.

The card remains viewer-facing and angularly scaled with the existing leader to the selected hit. Tools, manipulation and lifecycle visibility gates hide all metric components along with the text. Full DescribeCell diagnostics remain available separately. Test headset readability/occlusion before treating this first graphical pass as accepted.

Future D42 Reports is an optional screen for summary/time-series exploration with exact values, selectable periods and bounded saved monthly history. It must distinguish projections, monthly flows and end-of-month balances. No historical series is collected by the current card implementation.

## Citizen routine inspection (2026-09-29)

Home cards now show total residents, travelling residents and an At home now bar. Residents at work or shops are absent from the home bar; a travel intent counts as travelling even when its route is blocked. Assigned staffing is explicitly labeled on business cards because payroll/production still use job assignments, not physical attendance. Routine changes are arrival-driven and saved; check presence counts, departures/returns, pause and mid-trip load on device.

## Arrival purchase inspection (2026-09-29)

Home savings include funds held for outstanding purchases; the card shows the held amount explicitly. Shop stock likewise includes held units. Runtime purchase/sales bars now count this month's completed arrivals rather than monthly reservations. Last-month payroll/production remain labeled separately. A held order becoming a completed sale should be visible as the household wallet decreases, retailer cash increases, stock decreases and the current-month bars advance. Older prepaid shopping visits do not charge again.

### Local construction grid and wind indicator

Terrain is normally free of grid lines. Building placement reveals a small patch around the current targeted cell; road and zoning tools reveal wider patches. The patch fades at its edge and hides on inspection, manipulation, UI focus or loss of target. Point at the wind vane and press the dominant trigger to toggle wind arrows; hovering does not reveal them. Arrows remain visible after pointing away until toggled off (diagnostic views temporarily suppress them).

### Guided objective follow-ups (D43)

The accepted objective card is separate from stats and includes ten persisted teaching milestones, completed-step review and service/goods recovery guidance. Recognize already-completed actions in existing cities and support out-of-order play. Planned flow: roads and power/water, occupied homes and jobs/goods, police/fire protection, then schools and larger parks. Display a stage only once its system exists. Milestone history is implemented; live police guidance now introduces available protection or restoration. Persistent completed police/fire teaching steps remain D44/D45 work.

### Citywide citizen state (D44, schema 24)

The optional city summary now labels city homeless/unemployed counts, and Crime View labels its pressure values citywide. Building inspection retains only local crime risk/protection, so a residence never appears to contain homeless residents. Homeless citizens have no assigned home/household in authority. Education is saved per citizen for future skilled jobs/schools; current summary does not add another education text block. Reports may present distributions later. Initial police placement/Crime View access are user-confirmed; new mesh/readability/count checks remain D44/D39.

### Safety testing controls

Game settings > SAFETY TESTS groups START FIRE / START CRIME / START ARSON with a Back target. Each selects a board tool; the user closes the drawer, points at a completed building and pulls trigger. The page/tool labels state that fire/arson causes real damage; targeting presents the authoritative result in the inspector. Paused tests explain that time must resume for dispatch. Existing Inspect mode remains the way to monitor staff, supplies, crew travel/scene/return, damage and repair.

## Parks selection, inspection and coverage (D47)

Build > PARKS offers Pocket Park, NEIGHBORHOOD, CITY PARK, PARK VIEW and Back. City > PARK VIEW opens the same optional overlay. Placement retains authored footprint/cost and previews road/utility operation blockers. Neighborhood and city placement reveal local grids sized for their5x5/7x7 lots. Appended tool values are outside the explicitly bounded safety-test range. Fixed and hand drawers share the existing pooled entries; the longer neighborhood label uses a smaller hand font.

Park inspection names footprint, reach, monthly upkeep and operation. Its bar shows the active appeal against30; construction, damage, disconnected roads, missing utilities and unpaid bills remove benefit. Park View shows exact cell appeal from0 to30, operating/total counts, upkeep and the strongest-benefit rule. A zero cell is a valid uncovered value. Homes retain their shared desirability summary; no park-specific permanent HUD is added. The saved larger-park objective follows education and points to Build > PARKS/PARK VIEW. Headset menu, overlay and tooltip readability remain manual checks.

### Sports park selection and activity

PARKS now has seven park choices: Pocket, Neighborhood, City, Basketball, Skate Park, Baseball and Soccer, plus Park View and Back. The fixed drawer uses16 of the existing18 entries; hand mode uses9. Basketball/Neighborhood use compact labels, shared hover help names cost/upkeep and the size tier, and each action selects its saved authored definition. Sports retain their tier's placement grid size and the existing park inspector/coverage. Daytime activity uses bounded shared visitors and props; it hides with park operation loss, night or diagnostic views, and freezes when paused. No additional permanent HUD or activity reward is introduced.

### Building condition and crime badges (D52)

Settings > MAP BADGES toggles the session's building markers, enabled by default. Each affected lot has one compact badge: wrench/REPAIR with the authoritative cost, crossed tools/REPAIRING with progress and exact game days remaining, or police shield/CRIME for a current incident. Repair and crime on one lot share the badge and include a second shield; risk alone and historical incidents do not create markers. Active fires retain particles and do not offer repair until extinguished. Repair progress uses the saved completion deadline and configured repair duration; no new saved start time or history is created.

Sixteen fixed component groups use existing unlit text, panel and icon materials, no collision or ticking marker Actors. They face the camera at approximately constant angular size; nearby badges shift upward in the viewer's projection, with thin leaders retaining lot association. They hide during construction/road/zoning/demolition tools, overlays, drawer use, manipulation, input suspension, and while their building's inspection is open. Inspect and repair/safety-test modes retain them. This prevents a marker from blocking placement or hiding its own tooltip. Hidden/completed/demolished markers have no hit target; load transitions wait for the board's current city model.

With Inspect selected, trigger a badge to open the existing building tooltip; underlying buildings remain selectable and nearer buildings/terrain occlude badge picking. Crime inspection describes the actual event and police action, while repairs retain the explicit City > REPAIR command and progress metric. Desktop Inspect also follows the mouse ray. UI never starts repair or resolves crime just by showing/selecting a badge. D52 is user-validated and complete on2026-10-02; formal performance measurement remains D23. Future health warnings and hospital coverage/operation will reuse this pattern after health simulation, hospital definitions/staffing/treatment and progression have their own dedicated card.

The desktop sandbox pawn now exposes a camera component at its existing250cm eye height and uses the same control rotation. Shared inspection and badges can therefore be checked in actual PIE. Runtime initialization hides the marker pool after component registration before displaying current conditions.

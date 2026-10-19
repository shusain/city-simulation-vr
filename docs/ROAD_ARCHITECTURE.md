# Road architecture direction

Updated 2026-10-02 as D40 begins after D49/D50 acceptance. D26 established the logical road hierarchy and graph; D40 owns measured terrain-conforming presentation plus bridge/elevated profiles without replacing graph authority.

## D40 layered underpasses (2026-10-02)

`FCityCellCoord.RoadLevel` extends road identity:0 is ground;1 is an explicit span interior, including ramps. Dry span endpoints remain level0 portals. Terrain, building anchors, zones, pollution and map overlays retain their physical `(X,Y)` domain. The grid stores independent occupancy per road level; placement blocks buildings underneath the raised corridor. The current contract permits only one raised span per physical column; multiple raised heights and elevated intersections remain D54.

Connectivity enumerates adjacent road addresses on both levels. Raised addresses connect along their owning span and only transition to ground at that span's endpoints. A crossing's two addresses share map coordinates but have independent neighbors, graph edges, lane/connector queues and traffic positions. Service reach uses the same adjacency, so it cannot turn vertically at a crossing. Motor congestion heat maps show the highest lane load over the physical map cell; individual lanes remain distinct.

Ground crossings must be straight and perpendicular, beneath a whole plateau tile with no pier. Minimum deck-to-ground clearance is450 cm, covering the road surface67 cm, maximum vehicle envelope320 cm, safety gap40 cm and deck allowance23 cm. Alternate pier tiles and ramp tiles are rejected. These rules apply whether ground or raised is built first; validation precedes any occupancy change or charge. Demolition may leave perpendicular ground stubs, which remain valid for save/load. Ground demolition touches its address only; span demolition removes all raised interiors/ramps and retains its ground endpoint roads and any underpass.

Road rendering passes the level into its shared height evaluator for paving, sidewalks, curbs, markings, previews, pooled traffic and emergency spray. Sampled vehicle grid Z carries the route level, including interpolation through ramp portals; actual height comes from the authoritative terrain/span profile. Drawing uses the terrain ray to place ground roads under a span. Inspect/Demolish tests the nearest visible road surface at each level; ground can be selected by aiming below the deck. This headset selection UX remains pending verification.

Schema38 writes a required level on road records. Schema36/37 explicit interior addresses migrate to level1. Physical sort order is preserved, retaining graph edge/lane IDs and vehicle progress. Migration checks the previous network fingerprint before upgrading it to include road levels; malformed fingerprints cannot be cleared by migration. Older automatic bridges retain level0 and their prior presentation. Restore rejects orphan raised addresses, overlapping spans, wrong classes, illegal ground crossings and clearance/profile disagreement with generated terrain. Older schemas cannot serialize a layered crossing.

Native checks cover both axes/directions, all four road classes, both build orders, independent routes and queues, emergency yielding isolation, service reach, exact pooled car heights, demolition, save/resume and old moving-traffic migration. Temporary PIE shows both road layers with alternate piers. Current fixtures and checks do not establish Quest rendering, performance or comfort.

## D40 unified road drawing (2026-10-02)

User confirms joystick height adjustment works. Ordinary drawing now derives treatment from the authoritative terrain path: any water selects Bridge, dry terrain selects Ground unless raised by the stick. The first up flick on a dry drawing selects the computed safe elevated height; subsequent flicks add the existing2 m steps. Down from the minimum elevated height returns a dry drawing to Ground. An automatic water bridge is already raised, so its first up flick adds2 m; lowering cannot turn water into an unsupported ground road.

Resolve treatment whenever endpoints or stick intent change, before the normal command validation and commit. Automatic selection retains straight-span, dry-approach, safe grade/clearance and cost rules. Bent or short water crossings show the existing validation explanation rather than creating legacy automatic road cells. Moving back to dry terrain restores the current ground/raised intent. No new save schema or stored automatic mode is needed: committed spans retain explicit Bridge/Elevated profiles.

The Roads drawer offers classes and a Ground reset; separate Bridge/Elevated entries are removed. Class selection, successful commit and gesture cancel return to Ground at zero extra height. Stick Y is consumed for all road drawing while the drawer is closed, with the accepted neutral/lifecycle gating. Outside road drawing the temporary context releases it. Preview endpoints remain unchanged by flicks. The user accepts this drawing flow. Targeted handedness/lifecycle checks remain pending; underpasses now use the contract above.

## D40 height UX and underpass follow-up (2026-10-02)

Basic bridge/elevated placement is user-accepted. The current UX adds dominant-hand stick up/down flicks while the Bridge/Elevated tool is active and the drawer is closed. Each flick changes the extra deck height by200 cm, bounded to0–4 steps above the computed safe automatic height. Return inside the neutral deadzone before another flick; held input is ignored on activation, hand swap and lifecycle resume. A temporary consuming input context captures dominant stick Y/2D during this tool, then releases it for other tools, menus and manipulation. The user accepts the height control overall; targeted both-hand/lifecycle testing remains required. Unified drawing now also captures Ground mode stick input as described above.

Adjusting height retains the current start/end preview, recomputes ramps/clearance/cost and cannot spend funds. The status shows `Auto +N m` so it does not imply a shared absolute height between different spans. Each extra step adds$20 per interior cell; insufficient approach length rejects the taller span with an explanation. Schema37 persists the chosen step and verifies the derived deck/ramp/cost on restore. Schema36 spans migrate at step0 with their existing dimensions/cost.

Explicit raised-road supports now occupy alternate interior tiles (odd distance from the saved start cell), consistently across axes/directions and rebuild/save. Existing automatic bridges/supported ground retain their previous presentation. Layered underpasses now permit ground crossings in eligible gaps; piers and ramps remain blocked.

Layered identity and migration are implemented as described above. D54 later connects compatible elevated branches; different-height crossings remain outside this slice.

## D40 explicit construction profiles (2026-10-02)

The initial explicit-mode implementation selected road class independently from Ground, Bridge or Elevated mode. Unified drawing now derives those profiles as described above. Ground requires dry land and retains terrain-following supported-grade behavior. Pre-schema36 roads keep their legacy automatic water/support classification. Existing roads are preserved; the new modes apply to new commands.

Bridge/Elevated create one straight span between dry approaches. The initial version reserved the full corridor. The current implementation allows a raised span over a compatible ground crossing in a clear deck cell, in either build order, as described above. Buildings remain blocked. Ordinary endpoint roads may be reused; raised spans cannot overlap or overwrite one another. Bridge must cross actual water; Elevated is for dry corridors. Raised cells cannot provide building frontage or connect to adjacent ground roads. Connectivity, road-neighbor masks and graph routes share that rule; the two ground endpoint cells can connect to normal roads.

The deck is 480 cm above the highest terrain/water datum in the raised corridor. Integer ramp lengths limit centerline rise to 200 cm per 800 cm cell; sampled approach grades must obey the same limit. The span must have room for both ramps and at least one deck cell. Twenty-five samples per cell check terrain penetration and at least 400 cm water clearance. Transverse terrain slope fades out through the first/last ramp cell; endpoint cells remain on terrain. This uses current fixed terrain; explicit raised interiors have their own road level above ground.

Cost is the selected class base plus $60 per interior bridge cell or $40 per interior elevated cell. Newly created ground endpoints pay their normal terrain/class cost; reused endpoints are free. Preview validation supplies the exact commit cost, failure explanation and candidate height. A fixed, emissive-backed construction status appears below time controls while placing roads. Command revision validation and occupancy checks precede the atomic charge/commit.

Schema36 persists span endpoints, profile, integer deck/approach heights, ramp lengths and recorded interior cost. Restore rejects incomplete/overlapping corridors and inconsistent interior road classes; after terrain generation it recomputes and compares the entire profile/cost before accepting the candidate. Older saves have no explicit spans and retain automatic presentation. Demolishing any span cell removes every interior ramp/deck/support cell together and refunds half the recorded interior cost; ordinary endpoint roads remain for subsequent editing. Whole-span class reshaping is deferred; use demolition/rebuild.

The existing five-section RoadSurfaceProfile component renders raised paving using sampled ribbons; sidewalks, curbs, road markings, preview centers, visible traffic and emergency spray use the same height evaluator. Narrow instanced columns sit inside reserved road cells. No per-span Actors, collision authority or new rendering component is introduced for roads. Formal Quest spline/modular CPU/GPU/memory/draw comparison remains pending under D40/D23.

## D40 sampled ground paving and junction candidate (2026-10-02)

Straight ground paving is user-accepted. Ordinary ground cells now use the existing RoadSurfaceProfile component: section0 underlay, sections1–4 batched road-class paving. Straight cells contribute25 vertices/32 triangles. Corner/T/cross cells are a sampled center square plus disjoint trapezoidal arms for their connected neighbor bits, with up to125 vertices/160 triangles. Every patch shares the board height sampler, world-grid UVs and slope normals. Rigid ground bodies/paving are omitted so they cannot protrude through the new surface. Legacy supported/automatic bridge cells retain existing presentation; new explicit spans use the same sampled ribbons.

At a boundary between ordinary road cells, the shared paved width is the average of both class widths. Straight strips taper from that boundary width to their own center width, and junction arms taper from the center square to that same portal. This gives identical positions at mixed-class straight/junction boundaries. Straight sidewalks taper using the same paved width plus shared walking-strip width; visible pedestrian offsets follow that cross-section. Curbs/edge paint follow the sampled boundaries and closed junction edges are clipped to actual center/connected-arm extents. No lane legality, traffic authority, command cost or save schema changes.

This remains the initial bounded rendering candidate. Explicit construction profiles and migration are implemented; headset verification and spline/modular comparison remain active. Formal Quest CPU/GPU/memory/draw measurements are pending. Profile regeneration uses existing road/board rebuilds; no per-cell components or ticking Actors.

## D45 emergency yielding foundation (2026-10-01)

Traffic remains fixed-point C++ state. FireResponse/PoliceResponse journeys carry `bEmergencyResponding`; EmergencyReturn journeys keep ordinary priority. Only active responses scan their forward road route, at least6 cells or a speed-adjusted clearance horizon, and stop ordinary motor traffic on both sides. Previous curbside state is required before a responder ignores that vehicle as a longitudinal leader or passes it head-on. Opposing responders use stable ID priority: the other crew stops at the curb until the first has passed. The scan includes vehicle clearance beyond the destination so crews converging on one entrance cannot stop each other just short of arrival. Approaching responses hold normal junction queues and reserve connectors first, with ID ties. Walkers keep their separate sidewalk flow.

`bYielding` is a stopped curbside state. A vehicle waits for the normal safe gap on both sides before merging, remains stopped through the interpolated merge step, then accelerates normally. Existing queues must not need an extra gap that their own followers cannot create. Lane loads still include pulled-over traffic. The native instance pools interpolate the lateral offset (dirt160/local195/avenue270/highway300 cm); responses use the cleared center except while yielding to an opposing crew, and interpolate back onto their travel side on arrival. Spray uses the same offset helper. Current trip routes use lane0; richer lane selection and authored shoulders remain later road work.

Schema29 persists yielding and response priority. Older saves derive priority from the associated emergency trip and begin with ordinary vehicles in their normal lane. Full-city restore verifies the priority matches the source trip. Safe-gap validation permits longitudinal overlap only between a responder and stopped curbside traffic. No per-vehicle Actors or new ticking components. Automated overtaking/oncoming/queue/connector/save/pooled-transform checks pass; headset clarity at1x/3x/5x and road-clearance/comfort remain manual checks in D45/D23.

## Research findings

Epic separates the editable path from the mesh deformed along it. `USplineComponent` owns path data, while each `USplineMeshComponent` deforms one mesh between a start/end position and tangent. The spline-mesh API also exposes an explicit forward axis, which is important given the building FBX-axis issue already observed in this project:

- [Blueprint Spline Components Overview](https://dev.epicgames.com/documentation/unreal-engine/blueprint-spline-components-overview-in-unreal-engine)
- [`USplineComponent` API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/USplineComponent)
- [`USplineMeshComponent` API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/USplineMeshComponent)

Epic's Landscape Splines are designed to deform/paint Landscape terrain and distribute meshes along editable segments. That is valuable reference behavior for future terrain cities, but it is not a suitable authoritative system for the transformable tabletop board or runtime player commands:

- [Landscape Splines](https://dev.epicgames.com/documentation/unreal-engine/landscape-splines-in-unreal-engine)

PCG can draw roads along splines and spawn spline meshes, but current PCG Editor Mode documentation labels the mode experimental. It may become an offline authoring helper; it should not become a runtime dependency without a measured content-production need:

- [PCG Editor Mode](https://dev.epicgames.com/documentation/unreal-engine/pcg-editor-mode-in-unreal-engine)
- [PCG API, including spline creation/sampling/spawning](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/PCG)

ZoneGraph already models lane profiles, directional lanes, tags, connectors, linked lanes, and A* routing. It is a useful reference or later implementation candidate for vehicle lanes, but enabling it is a separate plugin/benchmark decision rather than a prerequisite for wider road art:

- [ZoneGraph API](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ZoneGraph)
- [`FZoneLaneDesc` width/direction/tags](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ZoneGraph/FZoneLaneDesc)

## Proposed separation

Authoritative simulation remains a compact deterministic graph:

- Nodes represent endpoints/intersections and own legal lane connectors.
- Edges carry road definition ID, class, width, directional lane counts, speed, capacity, access tags, upgrade history, and logical polyline/control points.
- Building definitions expose explicit entrance-side masks; placed entrance portals connect to compatible curb/access edges.
- Service distance and future traffic routing query graph data, never rendered spline components.

Presentation is derived and replaceable:

- Collapse collinear degree-two cells into maximal segments.
- Maintain a continuous distance origin per segment so dashed center/edge lines do not restart at cell boundaries.
- Use dedicated node/junction geometry for corners, T nodes, crossings, stop lines, turn pockets, and highway ramps.
- Evaluate spline meshes for visible curves, but pool/bake or use bounded modular instancing where per-segment component cost is worse on Quest.
- Generate dirt shoulders, curbs, sidewalks, medians, barriers, and lane markings from the road definition rather than multiplying simulation types.

This retains the current grid city's determinism while leaving room for curved presentation and later non-grid control points. Visual splines may be rebuilt from dirty graph regions after edits; they never decide connectivity, costs, lane legality, or save state.

## Road-definition minimum

Each future road definition should declare:

- stable content ID and upgrade compatibility;
- surface/shoulder/curb presentation set;
- carriageway width and directional lane descriptors;
- speed, capacity, allowed access/vehicle tags, placement/upkeep cost;
- marking rules including dash length, gap, edge lines, median, and junction termination;
- supported junction/transition meshes and collision/LOD policy.

D26 retained bounded modular instancing for the orthogonal-grid foundation. D40 must compare subdivided modular ribbons with spline meshes on one long straight plus corner/T/cross, slope, width-transition, water-crossing, and elevated fixtures across dirt, local road, avenue, and highway. Record component/draw counts, app CPU/GPU, memory, seam continuity, terrain conformity, and lane-profile alignment before choosing production presentation.

# Unreal architecture

## Experience and scope

Standalone Quest 3 is the working target, with PC VR/Link as a development convenience. The player plans a stylized miniature city from a seated or standing tabletop: roads, zoning, services, growth and budget recovery. Favor consequential planning and inspectable causes over individual citizen micromanagement. Development Tasks owns the current scope and feature queue; completed milestone evidence lives in Changelog.

Controller pointing/confirmation, optional handedness swaps and deliberate board move/rotate/scale gestures keep construction and manipulation distinct. Previews show footprint, connection validity and exact cost; cancellation never spends money. Keep the horizon stable, critical status readable without a continuously raised arm, menu focus exclusive, and headset-removal pause/lifecycle recovery explicit. Full VR and small scenario-plus-sandbox remain defaults; mixed reality, multiplayer, modding, hand-only control, street-level exploration and player terrain sculpting require a concrete future task.

## Separation of responsibilities

Begin with one runtime C++ module and clear folders; split modules only when useful. Use Blueprints for the VR pawn, UI composition, audio, and content tuning. Keep authoritative simulation and validation in C++ with testable data structures.

| Area | Responsibility |
| --- | --- |
| Simulation | Fixed-step city state, stable IDs, economy, eligibility, demand, and seeded growth |
| Commands | Validate and apply placement, demolition, zoning, and budget changes as atomic transactions |
| Presentation | Convert changed city records into instanced geometry and selected-object feedback |
| Interaction | Enhanced Input actions, XR pointing, board transforms, tool state, and menu focus |
| Content | Data assets describing footprints, costs, capacities, mesh variants, and material references |
| Persistence | Versioned saves, migration, validation, and atomic save replacement |

A world subsystem owns the simulation lifetime. UI and controllers issue the same commands as a desktop debug harness. Rendering reads snapshots/change sets, never determines whether a building is powered or a transaction is valid.

## State and timing

Store cells, road graph nodes/edges, building records, and service/economy state as compact data. Avoid one ticking Actor per building, citizen, or vehicle. Start with a 5 Hz fixed simulation tick and lower-frequency economy/growth passes. Maintain integer currency and stable content IDs. Fix iteration order and random seeds for reproducible tests; cross-platform bit-identical determinism is not an initial promise.

Use dirty regions after edits. Recompute affected road connectivity and service caches under a bounded work queue. Begin single-threaded and profile; if needed, workers operate on immutable snapshots and return results for game-thread application. Do not access Unreal objects unsafely from background work.

Cap catch-up work under overload so simulation time slows rather than freezing head tracking. Profile save hitches and avoid a blocking main-thread write during interaction; representative frame, memory and thermal targets are maintained in D23.

Citizens, households, businesses, freight and trips now have authoritative records; visible walkers/vehicles consume bounded pools. Density, occupancy, assignments, payroll, inventory and education determine capacity and economy rather than the first prototype's fixed ten-resident/ten-job aggregates. Use `CITIZEN_AND_ECONOMY_ARCHITECTURE.md` and `ROAD_ARCHITECTURE.md` for the current detailed contracts.

Needs snapshots expose capacity, demand, allocated use, shortage and road-reach gaps separately. Growth uses those shared evaluations plus eligible zoned land, treasury affordability and the last billing shortfall. Presentation explains the actual blocker; it never supplies identities, connectivity or financial outcomes.

## Board space

Store logical city coordinates independently of Unreal world coordinates. A board root transform maps logical meters to the displayed miniature. Convert controller ray hits through its inverse before grid snapping. Save city coordinates, not transformed mesh locations. Keep UI sizing and interaction tolerances sensible as the board changes scale; clamp bounds and test rotated/scaled picking.

Terrain extends this mapping rather than replacing it. A deterministic, quantized height/surface model maps logical grid vertices and cells to elevation, slope, and water classification. Occupancy, zoning, entrances, and road connectivity remain integer-grid state; presentation samples the shared model for Z. Buildings stay upright on explicit foundation envelopes, while road presentation follows bounded surface profiles. Save generation seed/parameters and authoritative edits, never transient mesh vertices or collision results.

Ray picking may intersect the rendered terrain for a candidate point, but it must convert that point through board space and validate the resolved cell against the authoritative terrain model. Grid lines, selection previews, zoning, service overlays, roads, foundations, and water masks all consume the same surface query so they cannot drift apart vertically.

## Rendering baseline

Prototype mobile forward rendering, mobile multiview, MSAA, Vulkan, and static project-level foveation. Verify every setting in the selected plugin/engine build. Start without Lumen, Nanite, hardware ray tracing, expensive transparency, or pervasive dynamic shadows. Prefer simple opaque materials and controlled lighting suitable for dynamically placed buildings.

Group repeated meshes using ISM/HISM components by mesh and material, partitioned into spatial chunks. Benchmark both instancing choices and chunk sizes. Track stable building IDs separately from mutable instance indices, updating the lookup when instances are removed. Use explicit selection colliders or grid queries instead of thousands of complex physics bodies.

Placeable definitions own an explicit bitmask of gameplay-local entrance sides and a separate quarter-turn presentation yaw offset. The entrance mask is authoritative for placement and later routing portals; the presentation offset only aligns imported mesh axes and does not alter saved logical rotation. Assets with multiple valid entrances declare every side rather than relying on bounds or nearest-road iteration order.

Road classes and traffic use the deterministic node/edge/lane graph documented in `ROAD_ARCHITECTURE.md`. Presentation derives surfaces, junctions, markings and bounded vehicles from it; D40 owns further terrain-conforming geometry and bridges/elevated roads. Bound highlight overlays and animated vehicles. LODs and screen-size culling must remain readable when players zoom the board.

## Save contract

Save schema version, content IDs, seed/RNG state, logical grid, roads, buildings, balances, simulation clock, and scenario progress. Reconstruct derived connectivity and visual instances after load. Validate dimensions and content references; report incompatible saves clearly. Write to a temporary file and replace the previous save only after successful serialization. Retain a last-known-good save.

## Project layout

```text
CitySimVR/
  CitySimVR.uproject
  Config/
  Source/CitySimVR/{Public,Private}/  # Simulation, Commands, Interaction, Presentation, Persistence, tests
  Content/CitySim/{Maps,Blueprints,UI,Data,Art}/
  Plugins/                    # selected project-local XR plugin, if used
ArtSource/{Blender,ComfyUI,Exports}/
Tools/                       # import validation, material remap, packaging and diagnostics
docs/
```

The permanent runtime module, city-specific content and source art exist. Never extend generated `Intermediate/Source`. Local Git/LFS tracks binary assets and excludes generated Unreal caches, Intermediate, Saved, Binaries, local SDK paths, packaged builds and vendor plugin binaries. Keep source art, exports and runtime content distinct. Current cards and acceptance checks are in `DEVELOPMENT_TASKS.md`; build/deploy instructions are in `ENVIRONMENT.md`.

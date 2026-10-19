# Shared development contracts

Cross-feature defaults moved from the task queue on 2026-10-18. Feature-specific proposals live on their Gitea issue, including [progression #22](https://git.shaun-husain.com/CitySimVR/citysim/issues/22).

- Authoritative rules remain in C++; UI and overlays consume snapshots/shared evaluators.

- Logical `(X,Y)` cells remain authoritative for terrain/buildings. Road addresses also carry a ground/raised level; graph, traffic and presentation preserve that level. Rendered meshes never define occupancy or connectivity.

- Citizens, households, businesses, trips, and vehicles are data records. Visible agents use bounded pooled proxies; no per-agent ticking Actors.

- Use coherent current definition IDs and current-version saves. Backwards compatibility/migrations are optional during early development; prefer simple current behavior and clearly document breaks instead of maintaining every historical format.

- Building entrances use authored entrance-side masks. Only connected roads with `BuildingFrontage` access satisfy development; highways require a compatible local/avenue connection.

- Low-density buildings retain 2×2 logical plots (16×16 m) for consistent zoning and road spacing. Prefer measured model/entrance clearance adjustments within that footprint; discuss a necessary plot enlargement explicitly. Specialized or higher-end utility buildings may use reviewed larger footprints. Foundations support building structures; pedestrian paths and vehicle approaches are separate terrain-following presentation.

- Critical state uses text/icon/shape as well as color. Green residential, blue commercial, and yellow industrial remain semantic conventions.

- Read-only progress and adjustable policy controls stay distinct. Exact authoritative values accompany bars/sliders.

- Environmental and financial simulation use deterministic fixed-step integer state. Visual weather, traffic, and construction never author outcomes.



## Product and presentation defaults

- Preserve the accepted tabletop loop, deterministic fixed-step authority, current saves, bounded pooled presentation and Quest controls.
- Build guided progression and civic services on existing roads, freight, occupancy and traffic. Prefer fixed glance panels, contextual inspection and optional continuous heat maps over permanent HUD prose.
- Improve art in coherent batches after data contracts exist. Presentation never authors simulation identity, connectivity, weather, terrain or finance.
- Extend the existing sandbox/pipeline; no engine/plugin/framework changes without a concrete task need. Preserve the VR-template regression scene.
- Use established Blender helpers and checked FBX contracts in [BLENDER_ASSET_PIPELINE.md](BLENDER_ASSET_PIPELINE.md), [ASSET_PIPELINE.md](ASSET_PIPELINE.md) and [AUDIO_REVIEW.md](AUDIO_REVIEW.md). Preview sheets use `Tools/Render-CityBlendPreviewSheet.ps1 -BlendFile <file.blend>`, matching A1 references and PreviewSheets.json. No automatic authoring-refactor or performance round follows asset review.

- Police/fire stations require a ground road supporting building access across the entire three-tile front in every rotation (user2026-10-18). Reject partial-frontage placement and paid upgrades; missing or disconnected frontage disables operation until restored. Reserve the complete garage face for the existing multi-bay upgrades, and use direct bay approaches. Existing partial-frontage cities remain loadable and repairable by extending the front road. Review steep driveway grades separately in#197.

- Wide utility service entrances (CoalPower, OilPower, Power, Service and NuclearPower) use complete ground-road frontage across their authored front in every rotation. Reject partial-frontage placement/paid expansion; road removal disables access until restored. Draw direct personnel/bay approaches, with no single-road-tile detour. Tower/wind/solar retain their narrow personnel access. Current cities with removed frontage remain loadable and repairable. User2026-10-18; implementation/acceptance tracked in#125.

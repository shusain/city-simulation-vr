# Blender, ComfyUI, and Unreal asset pipeline

## Production flow

Concept/reference -> Blender source mesh -> cleanup and UVs -> baked texture maps -> FBX export -> Unreal static mesh -> explicit material instances -> collision/LOD review -> Quest benchmark.

ComfyUI is optional upstream tooling for visual exploration and texture sources. Generated images are not automatically tileable PBR materials or game-ready geometry. If a workflow generates meshes, route them through the same Blender cleanup, scale, UV, collision, and LOD steps. Record workflow JSON, model/version, seed, source references, and usage rights with the source asset. ComfyUI installation and available workflows have not yet been inspected.

## Blender authoring contract

- Use metric dimensions, with a one-meter reference cube; confirm it imports into Unreal as 100 cm before bulk export.
- Apply rotation and scale. Choose ground-center pivots for buildings and a documented grid-aligned pivot for road pieces.
- Buildings occupy multiples of an 8-meter logical cell. Keep sidewalks and road widths consistent across the kit.
- Name meshes `SM_Building_Residential_A`, textures `T_Residential_A_BaseColor`, and material slots semantically, such as `Facade`, `Roof`, and `Trim`.
- Provide clean UV0; add a separate non-overlapping lightmap UV only if the selected lighting strategy needs it.
- Create simple collision proxies where needed and explicit LODs; avoid complex collision for decoration.
- Preserve `.blend` originals and non-destructive high-resolution sources outside runtime Content.

Start with FBX for static meshes, textures exported separately. Validate the Blender 5 exporter against the chosen Unreal importer using the calibration asset. Freeze that importer route and settings for consistent reimport. Do not expect Blender node graphs or procedural shaders to transfer faithfully: bake them into maps and reconstruct the runtime material.

## Material remapping design

Create a small set of Unreal master materials: opaque building surfaces, road/terrain surfaces, and optional masked foliage. Prefer atlas/trim-sheet reuse; generate variation with instance parameters or per-instance data. Avoid creating a unique shader/material for every generated building.

For each asset, maintain a manifest containing asset ID, source/export paths, dimensions, footprint, mesh destination, material-slot mapping, texture paths, LOD policy, collision policy, and generation provenance. Map semantic slot names to explicit Unreal material-instance paths. Unknown or duplicate slots fail validation rather than silently choosing a material.

Example future manifest shape (paths are illustrative and do not exist yet):

```json
{
  "schema_version": 1,
  "asset_id": "building.residential.a",
  "source": "ArtSource/Blender/Residential_A.blend",
  "export": "ArtSource/Exports/SM_Residential_A.fbx",
  "footprint_cells": [2, 2],
  "mesh_destination": "/Game/CitySim/Meshes/SM_Residential_A",
  "material_slots": {
    "Facade": "/Game/CitySim/Materials/MI_Residential_Facade",
    "Roof": "/Game/CitySim/Materials/MI_Roof"
  }
}
```

Implement the importer later as Unreal Editor Python or an Editor Utility, with dry-run validation, explicit assignment by slot name, and a report of changed assets. Reimport must preserve deterministic naming and restore assignments from the manifest. Validate references before saving packages.

Use BaseColor as sRGB. Import roughness, metallic, AO, and packed masks as linear data. If using ORM packing, standardize R=AO, G=roughness, B=metallic. Import normals with normal-map compression and verify tangent/green-channel orientation under a moving light. Bake procedural detail rather than reproducing expensive Blender shaders.

## Initial content budgets

These are starting production constraints to test, not Quest hardware limits:

| Asset | Initial target |
| --- | --- |
| Common building | 500-2,000 triangles at LOD0; aggressive lower LODs |
| Landmark | Up to 5,000 triangles, small on-screen count |
| Material slots | 1 preferred, 2 maximum for ordinary buildings |
| Texture sets | Shared 1K atlas initially; 2K only where on-device inspection justifies it |
| Vehicle | Small shared mesh/material family with capped visible instances |

First kit: one residential building, one shop, power/service building, road straight/corner/T/cross, ground tile, tree, and vehicle. Prove reimport and scale with the first building before expanding the kit.

## Acceptance checklist

- Dimensions, pivot, and footprint align with adjacent pieces.
- Mesh normals, UV seams, texture color space, and normal-map direction are correct.
- Material assignments survive a geometry change and reimport.
- LOD transitions work at tabletop and zoomed scales in both eyes.
- Collision and selection are simple and accurate.
- A repeated block fits measured on-device rendering budgets.

Reference: [Epic FBX material pipeline](https://dev.epicgames.com/documentation/en-us/unreal-engine/fbx-material-pipeline-in-unreal-engine). Treat automatic material import as a convenience; the project manifest is the source of truth for final assignment.

## UI icon pipeline

UI icons are licensed source assets rather than Blender geometry. Follow the reusable icon contract in `UI_INFORMATION_ARCHITECTURE.md`. In short: vendor the exact Material Symbols SVG and license/revision evidence, regenerate the tracked transparent PNG with `Prepare-CityUiIcons.ps1`, import through `Create-CityUiIcons.ps1`, reuse `M_CityUiIcon`, and validate viewer-facing direction on Quest. Never rotate a directional bitmap ad hoc in its material; preserve the upstream image and fix component orientation so the whole icon family shares one convention.

## D22 miniature-city visual contract

Category recognition must survive desaturation and normal tabletop viewing distance. Residential uses a pitched roof and chimney; commercial uses a storefront awning/sign; power uses paired generators and tall stacks; Water Treatment uses clarifiers and a raised tank. Semantic colors reinforce those silhouettes: green residential, blue commercial, orange-red power, and teal water infrastructure. Roof and trim are shared secondary slots, keeping ordinary buildings at the two-slot budget.

Every building records one or more gameplay-local entrance sides (`-Y`, `+X`, `+Y`, `-X`) and a zero-to-three-quarter-turn presentation yaw offset. The entrance mask describes the logical footprint after the presentation offset has aligned the imported mesh; it is not inferred from bounds. The current Blender-to-FBX-to-Unreal conversion reverses the authored Y frontage, so the starter buildings keep a logical `-Y` entrance and declare a two-quarter-turn presentation offset. A contrasting door and ground-level walkway must agree with the metadata, remain readable from tabletop distance, and reach the neighboring road surface. New manual previews and zone-grown buildings score every declared entrance edge and default toward the edge with the most entrance-connected road cells; a player's explicit quarter-turn remains authoritative for that preview, including at corners. This lets corner, drive-through, and multi-entrance assets add sides without changing placement code and gives later routing an authoritative portal contract.

Road tiles contain only the shared dark asphalt surface. Continuous runtime graph-edge instances supply lane markings with one cadence across cells; dirt roads receive no lane paint. Straight, corner, T, and cross pieces retain their grid-aligned pivot and intentional asphalt overlap. All category materials and their 25%-opacity overlay variants must be authored through the existing scripts and support instanced static meshes in cooked builds; a checkerboard is a failed import/cook, never an acceptable fallback.

Add future variants through `city_starter_kit.json`, not by special-casing presentation code. Each variant needs a bounded catalog entry, semantic slots, truthful dimensions, ground pivot, explicit single-mesh/LOD and collision policy, and an automated assertion before Quest review.

### Residential progression prototype

The starter manifest now names a `residential` progression family with `construction`, `low`, `medium`, and `high` stages. Low/medium/high share the same 2 x 2 logical footprint, local `-Y` entrance contract, two-quarter-turn FBX presentation correction, two-slot material budget, and ground-center pivot even though their silhouettes and heights differ. The construction site shares that footprint/frontage and uses separate crane jib and hook attachments. Animated attachments declare their semantic role and an explicit hinge/cable pivot rather than pretending to have a ground pivot.

The old D22 showcase remains presentation-only and hidden by default. D27 now consumes authoritative construction state for real lots: the static site mesh is selected from catalog data, while all active crane jibs and hooks share two bounded instanced component pools updated by the board tick. This preserves the cooked animation proof without per-building actors or components. Animation never determines construction completion, capacity, or save state.

Commercial and industrial now join residential as three-stage progression families. Shop low/medium/high use increasingly tall storefront, mixed-use, and tower silhouettes; Factory low/medium/high use increasingly large halls, processors, tanks, and stack groups. Every stage keeps the same 2 x 2 footprint, ground-center pivot, local `-Y` entrance, two-quarter-turn FBX correction, two-slot category/trim budget, and road-reaching door/loading path. Their catalog levels—not mesh height—author jobs/output, power/water demand, upkeep, pollution, upgrade cost/time, and pressure gates. New variants must preserve these semantic frontage and simulation contracts.

The pocket park is a reproducible 3 x 3 civic asset with a lawn, cross paths, fountain, and four simplified trees. It uses only the shared `Park` and `Trim` semantic slots, a ground-center pivot, four logical entrances, and the same two-quarter-turn imported-mesh correction. Park upkeep and desirability radius/bonus are simulation tuning; the mesh silhouette never authors those values.

## D38 architectural refinement (2026-09-29)

`Create-CityArtKit.ps1` regenerates the original primitive kit with low/medium/high facade details. The nine residential/commercial/industrial meshes remain two material slots and measure 90–370 triangles. Opaque window quads avoid glass transparency; broad details target tabletop readability. Shared roof material now also supplies commercial/industrial detail contrast via the manifest. No new texture sets are introduced in this pass.

Run `Import-CityArtKit.ps1 -ArchitectureOnly -DryRun`, then `Import-CityArtKit.ps1 -ArchitectureOnly`. Full-manifest validation still runs; only the nine finished density meshes are imported. The import report includes actual Unreal bounds, triangles and material assignments, and rejects mismatched dimensions/ground pivot, more than 500 triangles or more than two sections. Repeat import checks material assignment persistence. `Artifacts/AssetImport/city_art_geometry.json` records Blender counts for all generated meshes. `Render-CityArtPreview.py` renders the nine source preview tiles, using the manifest's detail-material mapping. These previews do not replace Quest validation.

### Night windows and pavement follow-up

Building detail slots now use `MI_CityWindowDetail`, still within two slots per building. The Blender generator writes alpha 0 only on window quads and alpha 1 on all solid geometry before joining. FBX import explicitly replaces vertex colors; the material uses inverse alpha as its emissive mask, driven by the shared world lighting collection. Preserve this mask when editing/exporting models. Store awnings overhang both side walls to avoid coplanar faces.

Generate surface assets with `Create-CitySurfaceMaterials.ps1` before architecture import. `Generate-CityPavementTexture.py` deterministically bakes the 64x64 source mask: red grain, green slab joints. Texture is linear, mask-compressed and mipmapped; no texture alpha transparency. `M_CityPavement` exposes CityColor, GrainStrength and SeamStrength. Sidewalks enable seams; roads leave them off. All materials use standard nodes and instancing-compatible shader permutations.

Pavement is explicitly referenced by the native board default object so it and its texture are included in Android cooks. Sidewalk and curb-top UVs use logical board coordinates; this aligns grain/joint samples on overlapping corner strips and adjacent cells. Curbs use clockwise outward winding as required by Unreal procedural meshes and are suppressed with other road decoration in heat maps.

### Road marking ownership (D38)

The four road topology meshes contain one `Road` slot and an 18 cm asphalt slab only. Lane paint is generated by the runtime graph-edge marking pool. Do not bake dash geometry into these meshes: recoloring raised markings to asphalt leaves dark blocks protruding through the class surface and yellow paint. Regenerate with `Tools/Create-CityArtKit.ps1`, then use `Tools/Import-CityArtKit.ps1 -RoadsOnly` for a focused four-mesh import with bounds, pivot and material validation. ImportedKit verifies the reduced height and single material slot.

### Terrain detail mask (D38)

`Tools/Generate-CityTerrainTexture.py` bakes an original deterministic periodic 128x128 mask into `ArtSource/Textures/CityTerrain.png`. `Tools/Create-CityTerrainMaterial.ps1` imports it as linear, mipmapped `T_CityTerrain` and binds it to the existing terrain material. One sample repeats over one eighth of a logical cell (1 m) at strength .4; mesh UVs are board coordinates so chunk boundaries share texture phase. Vertex colors supply continuous slope rock and height-based dry/wet shore blending. The mask modulates the surface below the placement-grid layer. No external imagery or license dependency.

The terrain grid is hidden by default (`GridReveal=0`). A shared runtime MID drives GridCenter in logical cell coordinates and GridRadius (3 building / 4 road / 6 zoning cells); a radial mask feathers its outer quarter. Board-wide UVs preserve alignment as the region moves.

### PIE debugging screenshots

`Tools/Capture-CityPreview.py` is a verified desktop PIE capture helper. Launch a fresh `UnrealEditor.exe CitySimVR.uproject -NoHMD -unattended -ExecCmds="py <absolute path to Tools/Capture-CityPreview.py>"`. Use the console startup command; `-ExecutePythonScript` exits before asynchronous PIE capture finishes. It captures a fresh sandbox at `Artifacts/Preview/City-PIE.png`, ends PIE and closes that editor without saving the map. It records errors beside the image and times out after 120 seconds. It uses the existing desktop pawn, accounting for BaseEyeHeight, so no map camera or actor assets are added. Use for framing/geometry/material checks; Quest ASTC/mobile shading, stereo and comfort still require device review.

### Construction detail iteration (D38)

`Import-CityArtKit.ps1 -ConstructionOnly` imports the site, jib and hook after full manifest validation. Site/jib keep existing bounds and pivots; hook retains its top attachment origin. Meshes are capped at 500 triangles and two sections each. The construction entrance is an open gate with a path, not a finished-building door panel. Perimeter barriers, stacks and braces are decorative. Runtime cable cubes use one shared pool and follow the existing hook/jib transforms; they clear with delivery/completion lifecycle and diagnostic overlays. `Render-CityArtPreview.py` also writes a construction assembly view with a proxy cable.

## D44/D45 civic source kit

`Tools/Create-CityCivicArt.py` creates original police/fire stations and police car/fire engine in `ArtSource/Blender/CityCivicKit.blend`, exporting four FBXs without regenerating the starter kit. The shared manifest supplies exact dimensions, two semantic slots, ground pivots, frontage and the existing two-turn FBX building correction. Stations occupy 3x2 cells; garage aprons reach the neighboring road. Current meshes measure 544/644/260/392 triangles. Shared body instances reuse M_CityColor; roof details use MI_CityPoliceDetail / MI_CityFireDetail and M_CityCivicDetail.

Run Blender with `--background --factory-startup --python Tools/Create-CityCivicArt.py`, then `Import-CityArtKit.ps1 -CivicOnly -DryRun` and `Import-CityArtKit.ps1 -CivicOnly`. Repeat import records measured bounds, triangles and assignments. `Render-CityCivicPreview.py` renders the four saved source models under `Artifacts/ArtPreview/D44`; source previews do not verify headset appearance. Police is linked into the existing catalog using Update-CityPoliceCatalog.py; fire/vehicles are prepared assets for future placement/dispatch. Selection and service authority stay in C++ logical records.

### Starter civic lots and parked vehicles

D44 now reserves the original 3x2 (24x16 m) civic lot while using a 14x10 m starter building body, one garage bay and matching car/engine parking on the side. The generator joins one decorative vehicle into each station mesh to retain a single building instance/two material sections. Parked vehicles never grant prevention/response capacity or represent actual dispatch counts. Future D48 paid wings/bays expand within the same saved lot.

Police/fire lots measure 544/644 triangles including the parked vehicle. The civic station import/test limit is 800 triangles; standalone vehicles keep the 500 limit and all other current focused imports keep their existing 500 limit. This is a bounded presentation budget change to accommodate the added vehicle, within the original common-building budget. Overall mesh bounds include the reserved ground pad/frontage driveway (24x18.4 m); manifest `building_body_dimensions_m` and `reserved_lot_dimensions_m` describe the smaller structure separately. Save footprint/entrance metadata remain 3x2/local -Y with two-turn FBX correction. Source previews and reimport checks do not establish headset readability.


### D48 station extension assets

Create-CityStationUpgradeArt.py creates four original garage/department police/fire meshes in CityStationUpgradeKit.blend and appends only its manifest entries. Reserved3x2 pad, original ground pivot, frontage and two-turn import correction are retained; garage/upper wing expands within that lot. Joined parked vehicles stay cosmetic. Police garage/department measure936/1256 triangles; fire1012/1380. Two existing shared civic sections are retained, with a1500-triangle limit for these extension meshes. No new material/shader or building Actor is introduced.

Create-CityStationDamageArt.py focuses the existing damage recipe on the four extensions, saves CityStationDamageKit.blend and a separate D48 geometry report, and checks that the shared damage source bytes remain unchanged. Matching damaged meshes retain source topology, zero boundary edges and non-emitting charred sections. Import-CityArtKit.ps1 -StationUpgradesOnly -DryRun, then -StationUpgradesOnly, validates/imports eight assets without regenerating existing meshes. Update-CityStationUpgradeCatalog.py assigns all three stages and policy while preserving definition IDs/entrances/lots.

Capture-CityStationPreview.py captures six imported starter/extension meshes and four damaged extensions from opposite overhead directions in temporary PIE. It isolates the gallery and existing HUD, ends play and closes without saving the map. Images/report are in Artifacts/Preview/D48-Stations*. Desktop shape/material checks do not establish Quest shading, normal-scale readability or comfort.

Capture-CityStationInspection.py uses the local pre-upgrade device-city backup at Artifacts/Diagnostics/D48-stations-device-before.json, loads it through native persistence in temporary PIE, restores the local default save byte-for-byte, then selects the real police/fire inspector using InspectBuilding. Day/night captures verify the actual priced action and shared unlit text/button materials. It ends play without saving the map and records restoration plus visible text in Artifacts/Preview/D48-StationInspection.json. The half-day refinement changes the catalog timing and C++ UI/state only; accepted station meshes/materials are preserved.

Authoring recovery on2026-10-02: the first focused damage run accidentally overwrote CityDamageKit.blend. Its original15-mesh gallery and geometry report were reconstructed from unchanged original FBX exports; the source file is not byte-identical to its previous version. Existing FBX and Unreal assets were preserved and not reimported. Restoration evidence is Artifacts/Diagnostics/D48-damage-restoration.json. The corrected focused path and byte-preservation guard pass before importing the eight new assets.

### D44/D45 red/blue police roof caps

Police/fire lots and vehicles keep two sections. Roof-slot geometry uses MI_CityPoliceDetail / MI_CityFireDetail with standard-node M_CityCivicDetail: ordinary geometry alpha1 uses CityColor; cap alpha0 uses decoded vertex RGB for emission. Parked decorations have zero emission and dim lens color (decoded RGB times.18); moving pools override only Roof with MI_CityPoliceResponseDetail / MI_CityFireResponseDetail (BeaconPulse=1). A single two-second smooth curve drives red at0–4; blue/warm-white uses its exact inverse. Their combined scalar brightness stays4 through crossovers, independent of simulation speed. Police is red/blue; fire is red/warm-white. Preserve RGB and alpha on WindowMask without adding sections. Current counts: police lot544/car260, fire lot644/engine392. Vehicle heights are1.87/2.78 m, with original ground pivots, footprints and entrances. Fire-engine caps measure .75x.72x.56 m and sit above their housing for better tabletop visibility. Create-CityCivicArt.py synchronizes measured civic dimensions into the manifest. Generate the detail parent and response instances with Create-CityCivicMaterials.py before focused import. VertexColor's main RGB output is unnamed: use the empty output string and check every connection result. The generator repairs existing rooted graph wires in place; it must never silently retain missing color inputs. Static-mesh sRGB vertex bytes pass through a standard Power2.2 node before lens tint/emission to preserve saturated beacon colors. Tagged pulse nodes are reused on repeated authoring. ImportedKit checks light vertices, emissive fire roof assignment and budgets/bounds/sections; CivicLightEmission verifies actual graph wires/masks/color decode/dim lenses, the shared curve and its inverse, imported color-side selection, full-cycle brightness, moving-pool Roof assignments and zero parked emission. Station vehicles remain joined decoration; moving vehicles consume authoritative trips.

## D44/D45 safety effects and damaged variants

`Create-CityDamageArt.py` derives all15 current building/stage meshes from the authored FBX exports, retains every source roof face, recalculates face normals, sags upper vertices and replaces sections with non-emitting Charred/CharredTrim. It saves CityDamageKit.blend, suffixed `_Damaged` exports and exact manifest measurements. Horizontal bounds and ground pivots remain original. Import/reimport using `Import-CityArtKit.ps1 -DamageOnly`; source previews via `Render-CityDamagePreview.py`. The generator rejects degenerate faces or additional boundary edges; face counts and boundaries are recorded alongside triangle counts. `Capture-CityDamagePreview.py` inserts a temporary gallery, captures all15 imported meshes from two overhead directions in PIE, then discards the gallery without saving the map. Never regenerate original meshes in this pass. Promptly extinguished superficial damage (default up to30%) clears without a repair bill, returning the original mesh. The board keeps logical IDs/density and chooses the suffixed mesh while fire damage exists, including during paid repair; explicitly cook Art/Meshes for these dynamically selected variants.

Run `Create-CitySafetyMaterials.py` in Unreal Python, then `UnrealEditor-Cmd <project> -run=CitySafetyAssets -unattended -nop4`. The commandlet authors baked Unreal CPU sprite particle systems PS_CityFire (flame/smoke) and PS_CityWaterSpray. Standard unlit particle-color/soft radial opacity nodes; additive flames and translucent smoke/water. Rates36/12/80 per second and lifetimes2/4/.9 seconds bound approximately192 live particles per fire/spray pair (576 across three pairs) before frame granularity. Flames and spray have larger sprites, sustained alpha and emission multipliers3/1.6; smoke remains multiplier1. The jet distance normalization is650 cm for the new lifetime. Initial allocations are96/64/96. Three pooled pairs are the gameplay cap; no physics/fluid solver or particle-driven simulation. Author distributions before cooking, retain particle sprite material usage and fixed bounds. Material generation updates existing emission outputs in place: CDO particle references root these graphs, so deleting their expressions asserts in Unreal. Brightness nodes have stable description tags to make repeated authoring idempotent. Water spray requires an arrived authoritative engine whose displayed interpolated pose has also reached the endpoint. Fire/Crime views retain emergency vehicles; other overlays suppress both engines and their spray. Pause freezes updates. Quest overdraw/comfort must still be measured in D23.

### Emergency-light PIE visual verification

`Tools/Capture-CityEmergencyPreview.py` runs in a fresh UnrealEditor with `-NoHMD -unattended -ExecCmds="py <absolute script path>"`, following Capture-CityPreview.py. It begins PIE, temporarily isolates/enlarges the actual FireResponseInstances and PoliceResponseInstances, verifies their moving Roof materials, captures response peaks and both crossovers plus the original parked Roof material at Artifacts/Preview/D45-BeaconPair-{Red,Other,CrossA,CrossB,Parked}.png, records sampled game times/modes in D45-BeaconPair-Preview.json, then ends PIE/closes without saving. Capture errors are written beside the report. This validates desktop color/alternation, not mobile bloom, headset comfort or representative-board readability.

## D46 school assets

`Tools/Create-CitySchoolArt.py` authors a compact3x2 school in CitySchoolKit.blend with an entry/book emblem, windows, benches and closed roof. Normal mesh228 triangles,24x16x6.34m, ground-center pivot, two shared Roof/Trim slots. School catalog entry is appended by Update-CitySchoolCatalog.py; existing definition order is retained. The common construction mesh is used until delivery/construction completes. School entrance is logical-Y with presentation yaw2, matching the established Blender/import convention.

`Create-CityDamageArt.py -- --SchoolOnly` creates only the school derivative in CitySchoolDamageKit.blend, preserving prior damaged assets. It retains all114 source faces/zero boundary edges and228 triangles, with recalculated normals/charred sagging presentation. `Import-CityArtKit.ps1 -SchoolOnly` validates/imports only normal and damaged school variants; shared slots, dimensions, ground pivot and budgets are checked. Focused import reports zero errors. Blender and temporary Unreal PIE previews inspect the actual meshes; the capture helper closes without saving its gallery/map. Headset appearance remains pending. No new material or per-building Actor is introduced.

## D47 larger park assets

Tools/Create-CityParkArt.py authors NeighborhoodPark and CityPark in ArtSource/Blender/CityParkKit.blend:40x40m and56x56m ground footprints, four edge-reaching cross paths, low-poly trees/benches, a neighborhood fountain and city pavilion/court. Meshes use the existing Park/Trim slots and measure448/648 triangles, below the800-triangle civic budget. The catalog retains existing IDs/order and appends both definitions through Update-CityParkCatalog.py. Shared construction and ground-centered imported pivots remain.

Create-CityDamageArt.py -- --ParksOnly exports only these two damaged derivatives into CityParkDamageKit.blend and a separate D47-park-damage-geometry.json report. Focused SchoolOnly/ParksOnly runs record their actual separate source path and preserve other manifest entries. Import-CityArtKit.ps1 -ParksOnly -DryRun, then -ParksOnly, validates/imports all four normal/damaged meshes with dimensions, pivots and two shared slots checked. This adds no material or per-building Actor.

Render-CityParkPreview.py captures Blender previews. Capture-CityParkPreview.py temporarily isolates all four imported variants in Unreal PIE, captures front/back views under Artifacts/Preview/D47-Parks-*.png, records asset paths in D47-Parks.json, destroys the gallery and closes without saving the map. Desktop previews validate shape/material assignment; normal board scale, day/night readability and Quest cost remain manual/D23 checks.

### D47 sports park assets and activity previews

Create-CitySportsParkArt.py authors four original alternatives in CitySportsParkKit.blend, preserving CityParkKit.blend: Basketball612, Skate288, Baseball564 and Soccer592 triangles. All use two shared Park/Traffic slots (green turf and pale markings/concrete), with closed authored ramps/rings and explicit four-side promenade access. Medium footprints40x40m, large56x56m; matched damaged meshes retain all faces/boundary topology. The same kit includes a60-triangle skateboard and20-triangle ball for shared activity. Animated props use explicit zero logical footprints and approved park_skateboard/park_ball roles, allowing centered prop pivots. Park-only dry-run/import includes all six larger normal/damaged meshes plus both props (14 entries); existing starter assets and map are preserved.

Render-CitySportsParkPreview.py inspects Blender sources; Capture-CitySportsParkPreview.py captures eight imported sports normal/damaged meshes in a temporary PIE gallery. Capture-CityParkActivityPreview.py loads the automation-generated seven-style fixture in ephemeral PIE, immediately restores the local default save byte-for-byte, and captures close views of actual moving pools twice by day, field activity and their absence at night on flat preview terrain. It closes without saving the map and writes restoration/motion evidence beside the images. This is desktop verification; normal-scale sports readability, activity pacing and Quest cost remain device/D23 checks.

Activity coordinates are authored in Blender space, then converted through ToImportedMeshSpace before the building transform: imported FBX reflects authored Y, so positions become(X,-Y,Z), rotation becomes(Pitch,-Yaw,-Roll). The imported baseball mound-top centre anchors a regression for pitcher alignment at all four building rotations. Preserve this conversion when adding new activity paths.

Park placement uses the highest terrain render sample across the full rotated footprint, including sub-cell samples and boundary samples, with the existing40cm mesh-origin clearance. This applies to pocket/larger/sports parks and their construction/damaged presentation. The preview, foundation, entrance profile and activity share that base; road entrance height cannot lower a park into terrain. Terrain and authoritative placement/save data remain unchanged. Capture-CityParkActivityPreview.py accepts -CityParkPreviewTerrain to retain the fixture's generated terrain and write separate D47-ParkTerrainActivity captures; the default flat-source inspection is retained.

### D52 condition icons

The existing pinned Apache-licensed Material Symbols subset adds build (wrench), construction (crossed tools) and local_police (shield). Sources and attribution remain under ThirdParty/MaterialSymbols; the same revision and license apply. Prepare-CityUiIcons.ps1 -ConditionOnly rasterizes only these three icons. Create-CityUiIcons.py with -CityConditionIconsOnly imports their textures/material instances while reusing the existing unlit masked M_CityUiIcon master. Full preparation/import includes all eleven symbols. No custom shader or new material master is needed. Condition badges use shared cube/plane meshes and the existing unlit panel/bar/text materials.


## Wind turbine rotor attachments (D56)

`Tools/Create-CityWindAnimationArt.py` produces original closed body/rotor meshes, previewing three blade angles before `--export`. `ArtSource/Blender/CityWindAnimation.blend` retains ground-centered body and axle-centered rotor pivots. The focused `-WindAnimationOnly` Unreal importer checks the axle-centered rotor separately from ground-centered buildings. Imported body68 triangles and rotor48 triangles preserve the original116 total with two existing Power/Trim material sections per mesh.

The catalog's full turbine remains the placement/construction preview and source of the existing damaged variant. Completed intact presentation replaces its welded blades with the body mesh and a pooled rotor attached at authored `(0,-1.7,17.8)` meters, reflected to Unreal `(0,170,1780)` centimeters. Rotation is around local Y before applying the same terrain/wind-facing building transform. `CityWindRotorMotion.h` analytically integrates a 2.5-second exponential speed ramp toward0/15/30 RPM, independent of game-speed multiplier. Board presentation caches turbine IDs by presentation generation, preserves motion across unrelated rebuilds, pauses with the simulation and discards attachments/state on damage/removal. Animation never changes generation capacity or saved simulation state.

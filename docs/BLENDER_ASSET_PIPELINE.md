# Blender asset-generation helpers

Reviewed 2026-10-13 for D93/D95/D96/D97/D98/D99. Run these authoring tools with the installed Blender described in [ENVIRONMENT.md](ENVIRONMENT.md). Python outside Blender can parse them but cannot execute `bpy` helpers.

## Paired Blender / Unreal asset sheets (D104)

Reflection diagnosis and future isolated-sample guidance: [UNREAL_REFLECTIONS.md](UNREAL_REFLECTIONS.md). D104 PC appearance/materials are user-accepted2026-10-14; D115 stays deferred.

**Industrial steel follow-up2026-10-14:** `CityIndustrialMetalMaterials.py` supplies exposed steel door0.22/hardware0.18 roughness and exact entrance-recipe face selection. `Update-CityIndustrialMetalMaterials.py` adds IndustrialDoorMetal to eight paired production exports without changing geometry/UV/RGBA/custom normals; damaged faces retain MI_CharredMechanical, with a distinct Blender semantic material to prevent FBX merging slots. The main production generator/importer use the same helper. Dedicated source/native before-after/package receipt is `Artifacts/D104/IndustrialMetal`; new material assignments apply only to industrial low-density assets. Glass/painted walls/paths and source studies remain. User accepts PC dynamic reflections; further Chaos work is deferred pending detailed interiors/support structure, and current stand-ins/scripted damage remain the current pipeline. D115's prototype stays retained evidence, with no automatic simulation/adoption.

**Latest hollow originals / dynamic delivery:** `CityIndustrialCylinderNormals.hollow_original_chimneys()` replaces Factory A's solid caps with annular rims/inner liners/deep closed floors; the production generator applies it before matched damage. Outside geometry/bounds/materials stay fixed, +96 triangles per original-factory mesh. Update-CityOriginalFactoryChimneys.py and Import-CityOriginalFactoryChimneys.py provide a focused paired export/import. Latest receipt/gallery is `Artifacts/D104/DynamicReflections`; smooth normals are mesh data, while Windows live specular capture is runtime environment behavior. Earlier cylinder receipts are retained history.

**D115 simulated damage source study:** `Create-CityDestructionStudy.py` authors84 closed masonry/roof pieces (.24m/.14m), attaches existing details and exports dedicated structural UCX hulls with checked units/material semantics. Shared Builder.make reacquires UV/color handles after layer allocation. `Run-CityDestructionStudy.py` uses stock Unreal Chaos rigid-body simulation and records actual native transforms/seed104106; `Bake-CityDestructionStudy.py` converts coordinates/handedness and merges three poses into ordinary static FBXs/editable CityDestructionBaked.blend. Three1560-triangle candidates, current668-triangle stand-in/intact and actual physics views are compared in `Artifacts/D104/DestructionStudy/Review.html`. Final bodies sleep; source/hash/frame receipts retained. Capture-CityDestructionBakes.py/Compose-CityDestructionReview.py can review/composite without regenerating authoring. Study packages are outside production catalog/cook; current stand-ins retained. Art review precedes any adoption; Geometry Collection clustering/strain/cache and live animation remain optional separate work. See ENVIRONMENT for pipeline/guard limitations.

**Industrial cylinder refinement:** `CityIndustrialCylinderNormals.py` identifies circular solid/tube groups in the mechanical slot and supplies averaged curved-wall corner normals across disconnected face vertices, retaining hard caps/rims. The production authoring tool applies it to healthy and damaged factories. `Update-CityIndustrialCylinderNormals.py` is the focused production-copy update, with unchanged geometry/UV/RGBA snapshots and eight FBX normal readbacks. Native import reads packed render-buffer normals and compares normalized directions to source; smooth shading is a mesh-normal change, not a material property. Current review/receipt: `Artifacts/D104/CylinderShading/Review.html` / Implementation.json. Earlier study/integration sources and galleries remain retained.

**Production integration2026-10-14:** Approved D103/D104 study sources remain reference inputs. `ArtSource/Blender/CityLowDensityBuildings.blend` and `ArtSource/Manifests/city_low_density_buildings.json` own twelve coordinated appearances with24 healthy/damaged exports under `ArtSource/Exports/LowDensity`. `Prepare-CityLowDensityIntegration.py` builds this separate production copy, preserves reviewed scale/entrances and semantic bindings, fixes six veranda face windings, and performs checked FBX roundtrips. Reauthor only for a concrete asset change; capture/composition alone does not require regeneration.

`Tools/Test-CityLowDensityIntegration.ps1 -Mode Import -RunName <fresh>` imports52 native meshes including32 independently named shop combinations plus PBR/sign materials and catalog rows. Bare steel uses mapped roughness~0.144/HQ forward reflections, while painted panels remain nonmetallic. `-Mode Bake` persists the explicit room reflection after concrete HDR changes; no Lightmass. `-Mode Capture` records perspective material views and `-BottomSigns` checks the flipped-V lower sign atlas; the original orthographic geometry-review sheets stay retained. `Test-CityLowDensityDesktop.ps1` captures the actual maintained game in an isolated profile. `Compose-CityLowDensityIntegration.py` creates `Artifacts/D104/Integration/Review.html` with26 native and17 game views/full references; the final verifier owns source/package/preservation evidence. Exact commands and delivery are in ENVIRONMENT/CHANGELOG. Physical headset rendering is unconfirmed.

`Tools/Render-CityUnrealPreviewSheet.ps1 -BatchFile Artifacts/D104/Batch.json` is the native counterpart to the Blender sheets. It uses an isolated content-only Unreal project under `Artifacts`, existing D3D11 forward rendering, explicit static mesh/material imports, fixed daylight/manual exposure and square offscreen render targets. It does not build/load gameplay or edit production content. `-Mode Import`, `-Mode Capture` and `-Mode Compose` let later comparisons reuse already imported native assets. When the matching Blender manifests exist, composition also writes the paired `Review.html` gallery.

The version-1 batch describes its source/hash, isolated project/output, mesh bridge files/hashes/semantic slots, existing material mappings, optional PBR materials/textures, and ordered `renders`. Each render has a unique key, object/source object, group/view/category, native mesh, semantic material overrides, dimensions/triangles, explicit camera position/target/orthographic width in Blender metres, ground height and citizen ruler placement. `CityUnrealPreview.unreal_cm()` maps Blender `(x,y,z)` to native centimetres `(100x,-100y,100z)` for the established FBX handedness. The importer verifies actual native triangle/section counts, bounds and REPLACE vertex-color policy. Capture resets every material binding between options, checks the upright 1.43175m ruler and writes hashed `PreviewManifest.json` files.

Native orthographic cameras may recede along the same view axis to keep the bottom frustum above the floor; direction/target/width and projected geometry remain fixed. An800m native floor prevents low-angle far-edge borders. A specified-cubemap sphere reflection probe and original neutral full-sphere studio HDR supply the reflection environment for bare steel under forward shading. As in the existing D79 dynamic-room/rooftop fix, this isolated process sets `r.ReflectionEnvironmentLightmapMixing=0`; black/unbaked indirect light must not suppress rough-metal reflections. The final workflow does not build Lightmass or save a map. Actual/reference camera positions, recede factor, reflection cube and mixing state are recorded per capture. Rejected black-border/dark-metal examples are under `Artifacts/D104/FirstNative`; focused projection/finite-depth/cube/bake/mixing diagnoses are retained beside it. API references: [SceneCaptureComponent2D](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/SceneCaptureComponent2D?application_version=5.7), [sphere reflection capture](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/SphereReflectionCaptureComponent?application_version=5.7), [reflection environment and lightmap mixing](https://dev.epicgames.com/documentation/unreal-engine/reflections-captures-in-unreal-engine).

`Render-CityPreviewSheet.py` accepts either engine's manifests. Existing Blender behavior/default labels are preserved; native manifests supply renderer labels. Each group/view receives A1 badges, full-resolution `ReferencedRenders`, and `PreviewSheets.json`, with matching tile order across engines. `Compose-CityMaterialComparison.py` verifies each render and camera contract before building the side-by-side gallery. This compares geometry/framing/material behavior; Cycles/AgX studio lighting and Unreal's daylight/tonemapper are explicitly different. It does not establish packaged/mobile/headset appearance.

D104 authoring starts from the approved D103 Revision3 blend. `Create-CityMaterialTextures.py` generates original seamless 512px timber, horizontal clapboard and corrugated-metal base-color/DirectX-normal/ORM maps; `Create-CityMaterialStudy.py` makes separate material copies and temporary mesh bridges. The saved study and batch manifest are `ArtSource/Blender/Studies/CityBuildingMaterials-D104.blend` and `ArtSource/Manifests/city_building_material_study.json`. `Validate-CityMaterialStudy.py` reopens the source and verifies original geometry/UV/RGBA, only main-wall binding changes, other materials and packed images. `Render-CityMaterialBlenderPreviews.py` renders new options and copies approved unchanged references byte-for-byte. Re-authoring replaces only the D104 source/maps/generated outputs; use a new output/source location before retaining a later approved D104 iteration. Exact commands/evidence are in ENVIRONMENT.

**Veranda bridge winding:** The approved Home_D_Veranda source transposes its main gable axes without reversing face winding, and its single veranda roof plane also faces inward. Blender's two-sided display hides this; Unreal's opaque roof culls those surfaces. `native_winding()` reverses exactly six faces only on a temporary export copy, retaining positions, triangles, semantic slots and per-geometric-corner UV/RGBA. Source blend stays unchanged; the native bridge correction is recorded in the batch manifest and `Artifacts/D104/VerandaRepair/Source.json`. Carry this correction into later authorized production exports; do not silently overwrite the approved source. The repaired native import and its three focused replacement views are under VerandaRepair.

## Shared starter and civic code

| Module | Responsibility |
| --- | --- |
| `Tools/CityArtKit.py` | Palette initialization, boxes/beams/cylinders/wheels, entrance/facade details, joining with `WindowMask`, static FBX export, and starter building/road/vehicle recipes |
| `Tools/CityCivicMeshes.py` | Police/fire palette additions, stations and emergency vehicles, including an explicit reduced-detail ladder option |
| `Tools/Create-CityArtKit.py` | Starter scene reset, source save, export selection, geometry report and existing adopted-surface regeneration workflow |
| `Tools/Create-CityCivicArt.py` | Civic scene reset, source save, budgets, exports, geometry report and manifest update |

Importing either helper module or either of these two generator entry points does not create Blender data, reset a scene or write files. The generators run through guarded `main()` functions. Other existing generator scripts remain executable workflows; import helpers rather than importing those workflows.

Starter consumers now import helpers directly: alternative facilities, building refinement, civic, hospital, parks, school, sports parks, utility upgrades, waste, wind and seasonal art. Station upgrades reuse the civic vehicle recipe directly. These replace all starter/civic source-prefix reads and `exec()` calls. The existing PowerShell and Blender command paths are unchanged.

Initialize the shared palette explicitly once per authoring run, before constructing meshes. `MATERIALS` is a shared dictionary for one Blender authoring scene, not an independent palette per concurrent job. Reinitialize it when starting another run in the same interpreter. Generators can add their own semantic materials after initialization.

```python
from CityArtKit import box, combine, export_one, initialize_materials

initialize_materials()
obj = combine("SM_Example", [box("Body", (2, 2, 3), (0, 0, 1.5), "Trim")])
export_one(obj, export_dir=temporary_exports)
```

The caller owns directory creation and review/promotion policy. `export_one()` retains the original starter static FBX settings and defaults to the project's `ArtSource/Exports`; pass `export_dir` for staged work. Changing a caller's global `EXPORT_DIR` cannot redirect an imported helper. D79 refinement now supplies its staging destination explicitly.

`combine(..., add_architectural_details=False)` joins already-detailed parts without adding another facade. Seasonal home authoring uses this instead of replacing a helper function in an executed namespace. Civic vehicles support `emergency_vehicle(..., ladder_rungs=False)` for parked upgrade fleets while retaining ladder rails, matching the former source-text substitution.

## Shared park geometry (D95)

`Tools/CityParkGeometry.py` shares connected-component discovery, Blender object appending, bounds/triangle measurement, static corner signatures and the park curve/tree refinement recipe. `Create-CityParkRefinement.py`, `Prepare-CityParkIntegration.py` and `Prepare-CitySportsParkScale.py` import the helpers directly. There are no remaining park-authoring prefix reads or tessellation source substitutions. The authoring entry point now uses a guarded `main()`; importing it or the geometry module performs no scene/file/manifest work.

Refinement takes an immutable `ParkDetail` profile. Defaults preserve the existing review geometry; integration explicitly selects its existing lower-detail profile:

| Setting | `REVIEW_DETAIL` (default) | `INTEGRATION_LOD_DETAIL` |
| --- | --- | --- |
| Cylinder radial segments | 48 | 24 |
| Icosphere crown subdivisions / triangles | 3 / 320 | 2 / 80 |
| Skate ramp panels | 32 | 16 |

The simple park's pruned crown and tapered trunk retain their fixed eight/twelve sides in both profiles. Detail counts must be integers; cylinder segments must be at least twelve and divisible by twelve, and crown subdivisions/ramp panels must be positive. Source loading for the simple park's bark/canopy materials occurs only during refinement; `environment_source` can explicitly supply that blend path when needed.

```python
from CityParkGeometry import refine, INTEGRATION_LOD_DETAIL

changes = refine(obj, asset_id, detail=INTEGRATION_LOD_DETAIL)
```

Each generator still owns its render/export/promotion contracts. D95 corrects one reporting error: a lower-detail crown now reports its actual 80 triangles, rather than carrying the old hard-coded 320. Geometry and default review metadata are unchanged. Citizen rig/animation functions retain identical ASTs; sports scaling uses the shared connected-component helper without changing its body/prop/contact rules.

## Preview sheets from saved blends

Use `Tools/Assets/Blender/Render-CityBlendPreviewSheet.ps1` for reusable mesh preview sheets. It runs Blender via `Render-CityBlendPreviews.py`/`CityAssetPreview.py`, then the bundled Python/Pillow via `Assets/Preview/CityPreviewSheets.py`. Original root command/module paths remain compatibility forwarders; curated comparison tools import the shared `compose()` API directly. It reads the source blend, renders evaluated mesh copies in a separate scene, and never saves the source or exports FBXs. Existing kit-specific render/comparison scripts remain available for their curated assemblies and lighting/layouts.

From the repository root:

```powershell
# Discover saved scene and mesh names without rendering.
& .\Tools\Assets\Blender\Render-CityBlendPreviewSheet.ps1 -BlendFile .\ArtSource\Blender\CityParkRefinement.blend -ListObjects

# Render park assets, with two views per mesh and two columns.
& .\Tools\Assets\Blender\Render-CityBlendPreviewSheet.ps1 -BlendFile .\ArtSource\Blender\CityParkRefinement.blend -ObjectPattern '*Park*' -Views iso,front -Columns 2

# Render actual material nodes/textures using studio lighting.
& .\Tools\Assets\Blender\Render-CityBlendPreviewSheet.ps1 -BlendFile .\ArtSource\Blender\CityResidentialScale.blend -ObjectPattern SM_HomeScale_Refined -Engine eevee

# Snapshot an evaluated, skinned citizen at a chosen pose frame.
& .\Tools\Assets\Blender\Render-CityBlendPreviewSheet.ps1 -BlendFile .\ArtSource\Blender\CityCitizenSportsRefinement.blend -Scene D88_BaseballPitch -Frame 25 -ObjectPattern D88_BaseballPitch_Citizen -Color vertex
```

Default output is a new timestamped folder under `Artifacts/ArtPreview/Sheets/<blend-name>-<timestamp>`. Supply `-OutputDirectory` for a new or empty folder; nonempty folders are rejected before launch to preserve prior renders/reports/logs. `-TimeoutSeconds` bounds each owned render/compose process (default900 seconds). Separate Blender/Compose console and stderr logs are retained; list mode also retains its Blender logs while printing the object list. Fresh reports must match the unchanged source blend and complete render set. Each run creates individual PNGs, `PreviewManifest.json`, numbered `PreviewSheet-01.png` pages, and `PreviewSheets.json` listing the current pages/hashes. Reusing a folder overwrites the same named output files and retains other files; composition uses only images named and hash-verified by the manifest, never a directory glob. Old extra page files can remain; consult the current page list.

Every sheet has column letters, row numbers and an A1-style badge on each tile. Reading left-to-right, then top-to-bottom: A1, B1, C1, A2, B2, C2 for three columns. Each page restarts at row1; say **page2 A1** or **P02-A1** when there are multiple pages. The wrapper prints both the first sheet and its lookup JSON path.

`ReferencedRenders` contains byte-identical full-resolution PNG copies with matching references, for example `P01_A1_001_SM_Building_Park_D83_A_iso.png`. These are the individual renders used by the sheet, with no badges, cropping or resampling applied to their files. The original render filenames and PreviewManifest.json remain valid, allowing another layout without rerendering Blender. Only the sheet thumbnails receive badges.

For agents, `PreviewSheets.json` is the authoritative lookup: `references["P01-A1"]["image"]` gives a path relative to the sheet output directory. Each record also has the cell, page, column, row, sheet filename, object name, view, SHA256 and original source-image path. Each page lists its references in reading order. References identify positions in that specific sheet/layout; changing the ordering, columns or rows can assign an asset a different reference. Use a new output folder to retain a reviewed sheet's mapping; do not infer active references from leftover files.

| Option | Meaning |
| --- | --- |
| `-ObjectPattern` | One or more case-sensitive mesh-name wildcards; default all visible meshes in the selected scene |
| `-Scene`, `-Frame` | Defaults to the saved active scene and saved frame; `-ListObjects` lists scenes/mesh names |
| `-Views` | Any of `iso,front,back,left,right,top`; default `iso`; front looks from authored -Y |
| `-Engine` | `workbench` (fast geometry inspection) or `eevee` (actual material nodes/textures with three studio lights) |
| `-Color` | Workbench `material`, `vertex` or `object`; Eevee always uses material nodes |
| `-IncludeHidden` | Include hidden meshes/LODs; default skips hidden objects, viewport exclusions and hidden render collections |
| `-Resolution` | Square individual-render size, default512, range64..4096 |
| `-Columns`, `-Rows`, `-TileSize` | Sheet columns, maximum rows per page and thumbnail size; defaults3/4/384; fewer columns for smaller render sets |
| `-Title` | Optional sheet title |
| `-Blender`, `-Python` | Optional executable overrides; defaults to the verified Blender5.0 and bundled Python/Pillow locations |

Every tile shows the original object name, view, world-space bounds in meters (using the source scene's unit scale) and evaluated triangle count. Cameras fit each mesh independently with a margin; sheets are not a common-scale ruler. Copies retain evaluated geometry, material slots, UVs, color attributes, pose and world transforms. Source gallery offsets are centered only on the preview copy. Rendering checks projected bounds to reject accidental cropping. Individual meshes are isolated; multipart assemblies, animation sequences, authored scene lighting and production Unreal appearance still use their specialized review tools. Workbench ignores material textures; use Eevee for surface review.

The two Python entry points also work separately:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.0/blender.exe' --background --factory-startup --disable-autoexec --threads 2 --python-exit-code 1 --python Tools/Assets/Blender/Render-CityBlendPreviews.py -- --blend ArtSource/Blender/CityParkRefinement.blend --output Artifacts/MyParkPreview --objects '*Park*' --views iso front
& 'C:/Users/shaun/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' Tools/Assets/Preview/CityPreviewSheets.py --manifest Artifacts/MyParkPreview/PreviewManifest.json --columns 2 --rows 4
```

Do not run the Blender renderer with ordinary Python. The Pillow composer can reflow existing verified renders without rerendering. No installation or environment reset is needed.

## Shared damage generation (D97)

`Tools/CityDamageArt.py` owns the existing damaged-building recipe and its scope-aware manifest replacement. `Create-CityDamageArt.py` and `Create-CityStationDamageArt.py` are guarded entry points that call it directly; importing any of these files creates no Blender data or writes. The station entry point no longer reads/executes/rewrites the generic generator's source.

| Scope | Selection | Source blend | Geometry report under `Artifacts/AssetImport` |
| --- | --- | --- | --- |
| `all` | Eligible unreviewed buildings | `CityDamageKit.blend` | `city_damage_geometry.json` |
| `school` | Eligible school service buildings | `CitySchoolDamageKit.blend` | `D46-school-damage-geometry.json` |
| `parks` | Eligible park service buildings | `CityParkDamageKit.blend` | `D47-park-damage-geometry.json` |
| `station-upgrades` | Eligible entries with `station_upgrade` | `CityStationDamageKit.blend` | `D48-station-damage-geometry.json` |

Defaults retain the existing `ArtSource/Blender`, `ArtSource/Exports` and starter-manifest contracts. Existing CLI commands remain: generic generator with no flags, `-- --SchoolOnly`, or `-- --ParksOnly`; station upgrades use their existing station script. School/park flags are mutually exclusive. Actual generation prints `CITYSIM_DAMAGE_ART_SAVED`; no eligible originals prints `CITYSIM_DAMAGE_ART_SKIPPED` and performs no scene reset, directory creation, source save, export, report or manifest write. A skipped command does not refresh older reports.

The recipe keeps source topology/UVs/pivots and material-index mapping, deforms upper vertices with the existing phase/order, recalculates normals, validates boundaries/face area and the1500-triangle budget, clears damage color attributes to white, and uses the existing static FBX settings. The output source gallery is positioned only after ground-centered exports. Relative manifest source/export references use forward slashes.

Reviewed D91 parent/variant links and D79/D83 damage variants are retained. A protected damage target also prevents regeneration of its base-derived counterpart, avoiding duplicate asset identities or export replacement. Each scope replaces only its unreviewed damaged entries; unrelated entries remain. Current station originals are all reviewed, so the station command currently skips and preserves all four accepted damaged variants and their existing sources/exports.

For staged work, supply a copied manifest and all output paths explicitly. Relative overrides are interpreted beneath the supplied root; generated outputs and the updated manifest must remain under that root:

```python
from CityDamageArt import generate_damage

result = generate_damage(
    ROOT, scope='station-upgrades',
    manifest_path='Artifacts/MyDamage/Manifest.json',  # prepared copy
    source_path='Artifacts/MyDamage/StationDamage.blend',
    export_directory='Artifacts/MyDamage/Exports',
    report_path='Artifacts/MyDamage/Geometry.json',
)
```

The refactor fixes two concrete station-wrapper defects: filter injection inside a multi-line comprehension failed parsing, and its broad replacement would remove accepted station damage even though the reviewed originals were skipped. Its dependency on reading the unrelated shared damage blend is also removed; explicit scope/output ownership keeps that source untouched.

## Shared study FBX verification (D98)

`CityBuildingStudyIO.checked_fbx()` is the shared static study export/readback transaction. Home-window, home-detail, home-scale and building-window studies now use it; existing civic-surface and building-adoption-LOD consumers continue through the compatible `export_checked(obj,path,slots)` wrapper. The building-window study also reuses the unchanged shared mesh snapshot and digest helpers. Its local export wrapper retains study-specific report fields and semantic pane-count checks.

```python
from CityBuildingStudyIO import checked_fbx, window_faces

with checked_fbx(obj, path, dimension_tolerance=.001, expected_sections=4) as readback:
    assert window_faces(readback) == 11
```

The context exports only the selected mesh with the existing study profile (`global_scale=1`, axes -Y/Z, no animation/leaf bones, `STRIP` paths, face smoothing), imports the actual FBX and checks exactly one new mesh, triangle/section counts and dimensions. Pass `slots` for canonical material-label isolation and verification; omit it to keep the home studies' original material naming. `expected_triangles`/`expected_sections` retain fixed study requirements, while defaults compare with the source. `verify_alpha=True` retains the existing civic/helper's exact ordered WindowMask-alpha check. Each caller still owns its acceptance checks inside the context; `window_faces()` shares the original all-corner alpha threshold and optional semantic-material filter.

Home-window/detail checks retain the .001m tolerance and eleven panes. Home detail retains four sections; home scale retains .002m,668 triangles/four sections and eleven panes. Building-window retains .002m, canonical slots and expected opaque-window counts. The compatible civic/LOD helper retains its prior .002m, slots and alpha requirements and unchanged result fields. Independent source/native validators remain independent; starter, skeletal and other export profiles are unchanged.

Source material labels are restored even if export fails. On success or failure the context removes imported objects and unused imported meshes/materials/images, then restores the original selection/active object. Use the readback only inside the `with` block. Callers own output directories and review/promotion; the shared transaction performs no source save or manifest update.

## Existing specialized reuse

Building studies already share `CityBuildingStudyIO`, `CityBuildingSurfaceGeometry`, `CityBuildingWindows`, `CityCivicBuildingSurfaces`, `CityBuildingRoofSigns`, `CityBuildingAdoptionMaterials` and `CitySeasonalBuildingSurfaces`. Citizen/park motion shares `CityCitizenRigControls`, `CityCitizenSportsMotion` and `CitySkateKeyframes`. Preserve these boundaries: window masks, physically scaled UVs, packed textures and skeletal animations have different contracts from starter primitives.

## Further opportunities

The identified matching study transactions are consolidated by D98. There is no pending bulk refactor or acceptance check for D93/D95/D96/D97/D98.

**Preview adoption remains optional:** D96 provides `CityAssetPreview.py` and a generic preview-sheet workflow. Existing kit tools may adopt its mesh-copy/camera helpers when another concrete art task needs them, while retaining curated assemblies, lighting and comparison layouts.

## Verified D99 reference evidence

`Artifacts/D99/GridExample` demonstrates A1/B1/A2/B2 with the saved D96 park renders; `Paginated` demonstrates page-qualified references across two sheets. Both were visually inspected. `WrapperExample` verifies the complete existing PowerShell/Blender/Pillow command on the transformed-parent fixture with two views. `Verification.json` records expected reading-order/cell mappings, page/file hashes, byte-identical full-resolution copies, intact original renders/source blends, a partial final row and rejection of changed input before output. Python source and PowerShell syntax checks pass. No production blend regeneration, native build or device check applies; no required follow-up remains.

## Saved fountain reference test (D100)

**D102 artifact retention2026-10-13:** Completed helper-refactor comparison exports/blend copies under Artifacts/D93/D95/D97/D98 were retired with explicit user approval. Production/study sources, frozen original Python scripts, normalized comparison JSON, top-level verification receipts and logs remain; the exact removed paths/hashes are in [the extended cleanup manifest](visual-changelog/cleanup-candidates-2026-10-13-extended.json). Historical temporary-export paths describe past checks, not available current assets. D96/D99 indexed preview examples and the D100 fountain test remain.

The A1-requested fountain is joined to `SM_Building_Park_D83_A` in the tracked `ArtSource/Blender/Studies/CityParkFountainPreview.blend` (1392 triangles). Its [final sheet](visual-changelog/studies/D100/PreviewSheet-01.png) is retained with the source for repository backup; it remains a preview study with no runtime/FBX promotion. The local original edit, four full-resolution reference renders, index and verification receipt remain under Artifacts/D100. To recreate the indexed previews from the tracked study, run the normal sheet command with that blend, `-ObjectPattern SM_Building_Park_D83_A,SM_Building_SkatePark_D83_A -Views iso,front -Columns 2 -Rows 2`. This restores A1/B1 park and A2/B2 skate references in a fresh output folder.

## Verified D93 evidence

`Artifacts/D93` contains frozen pre-refactor scripts, one-time comparison scripts, logs and receipts. Blender 5.0.1 executed the checks; production art was not regenerated.

- `Comparison.json`: exact mesh snapshots for 49 recipes, including topology, normals, transforms, dimensions, UVs, materials and color attributes; four actual old/new FBX readbacks match. Helper and starter/civic entry-point imports leave Blender data unchanged.
- `SeasonalComparison.json`: all 14 actual seasonal-prefix meshes match, including the three display buildings and the current accepted home-surface application. No render/export/manifest-write stage was executed.
- `GeneratorComparison.json`: civic, school, park and station upgrades run in independent old/new workspaces; all 11 expected exports are produced, and the manifest plus four geometry reports match.
- `StarterComparison.json`: both starter entry points run with a minimal manifest in temporary workspaces; all 23 exports and geometry reports match. The adopted-surface regeneration branch was retained but was not rerun.

These are authoring-equivalence checks, not new Unreal imports, packages, device rendering or comfort evidence. All updated sources parse; no native build is required for this Python-only change.

## Verified D95 evidence

Blender5.0.1 executed the one-time checks against frozen legacy scripts with production inputs read-only and all generated outputs under `Artifacts/D95`:

- `GeometryComparison.json`: ten source meshes at both detail profiles (20 exact geometry comparisons), four actual skate FBX roundtrips and import-side-effect checks. The only expected metadata difference is the corrected lower-detail crown count.
- `IntegrationComparison.json`: actual static integration stages match for 36 normal/damaged/seasonal LOD meshes and their normalized export contracts; both runs pass all 36 FBX roundtrips.
- `AuthoringComparison.json`: actual static authoring stages and the new `main()` match for eight mesh exports, source-gallery geometry and normalized reports; both runs pass eight FBX roundtrips. Image rendering and rig stages were skipped.
- `SportsComparison.json`: both complete sports-scale workflows pass four FBX roundtrips and all contact/body-transform checks; four mesh reports, measurements and all293 body/prop samples match.
- `Verification.json`: four changed Python sources parse, scoped CRLF-aware whitespace checks pass, rig/animation function ASTs remain identical, and recorded production Blender/manifest/refinement-export hashes are preserved. Full rig regeneration, preview rendering and Unreal/device validation were not run.

No current acceptance checks remain for this authoring-only extraction; further damage/preview work is optional scope.

## Verified D96 evidence

`Artifacts/D96` contains inspected park, posed-citizen and Eevee home sheets, individual renders/manifests, a transformed-parent/visibility fixture and paginated sheets. Blender5.0.1 and the existing bundled Python/Pillow execute the complete PowerShell command. Checks cover all six views, source unit scaling, parent transforms, object/collection visibility, hidden inclusion, explicit scene/frame/name filters, camera bounds and pagination. Output hashes/dimensions and three production source hashes verify; helper/entry-point imports and source syntax are checked in adjacent verification scripts/receipts.

Workbench park and citizen images and textured Eevee home appearance were visually inspected. This is Blender preview evidence, not a new runtime asset import, device rendering or comfort observation. Full assembly/animation preview remains specialized scope; no current acceptance check is pending for the generic sheet command.

## Verified D97 evidence

Actual Blender5.0.1 runs in `Artifacts/D97` compare14 generated meshes across all/school/park/station scopes and28 old/new FBX readbacks, plus normalized manifests and geometry reports. All match the existing shared damage recipe. Because the raw legacy station wrapper cannot parse, its comparison baseline repairs only the misplaced filter insertion inside the temporary test; the raw syntax failure is independently recorded rather than presented as a passing legacy run.

`PolicyChecks.json` verifies import safety, current reviewed-station/school no-work runs with byte-identical files and unchanged scene state, and a mixed station job generating three eligible derivatives while preserving one reviewed target and every unrelated/original entry. Production protected-export hashes remain unchanged. `Verification.json` records syntax/whitespace, frozen source/export/manifest hashes and receipt identities. No production art regeneration, preview rendering, native build/import/package or device check was run; no current acceptance check remains.

## Verified D98 evidence

Blender5.0.1 comparisons in `Artifacts/D98/Comparison.json` exercise the actual old/new export blocks on14 saved meshes: all seven home-study variants, four building-window forms including tilted industrial glazing/damage, two civic-surface forms and a mobile LOD. All28 temporary exports pass old/new checks; normalized report fields, source mesh semantics and independently imported FBX topology/normals/UVs/materials/colors/dimensions match. The existing shared snapshot function's AST is unchanged.

`CleanupChecks.json` records import safety, deliberate caller pane-check failure, common triangle-check failure, export failure after material-label isolation and successful retry. Original mesh data/material labels/selection stay intact, and imported objects/unused meshes/materials/images are cleaned up. `Verification.json` records five parsed Python sources, scoped whitespace and frozen production source/manifest hashes. Full authoring/render/native/device work was not run; generation/review tools and current assets remain intact, with no current acceptance check pending.

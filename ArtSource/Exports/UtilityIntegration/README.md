# Approved utility integration preparation — #157 / #125

This is one offline preparation batch of the eleven approved utility versions.
The approved LOD0 blend/FBX, raw approval and all exported source artifacts remain
unchanged under `ArtSource/Generated/blender/issue-N/vN`. This directory contains
derived LOD1/LOD2 FBXs and an optional exact wind body/rotor split. No Unreal,
catalog, runtime, CI or renderer was run by the preparation scripts.

The batch is based on merged-main `a5168fe0bf45f0d93e3b8c5b4b0cc69a310a84e8`.
`ArtSource/Manifests/city_utility_preparation_inputs.json` freezes every exact
source head, issue/version, independent approval/Blender/FBX hash and reservation.
`city_utility_integration.json` contains the measured handoff;
`city_utility_preparation_verification.json` records independent saved-source and
actual Blender FBX readback checks. The local preparation commit identifies the
complete recipe and all derived output hashes. Do not regenerate merely to
review or import these identified outputs.

| Source | Reservation in 8 m cells | LOD0 / LOD1 / LOD2 triangles |
| --- | --- | --- |
| Coal #158 v4 | 4×4 | 5796 / 5484 / 5160 |
| Oil #159 v3 | 4×4 | 5868 / 5520 / 5160 |
| Basic power #160 v2 | 4×4 | 2576 / 2200 / 1812 |
| Basic power medium #163 v3 | 4×4 | 4340 / 3780 / 3208 |
| Basic power high #164 v3 | 4×4 | 4768 / 3916 / 3064 |
| Water treatment #161 v2 | 6×4 | 4848 / 4560 / 4212 |
| Water treatment medium #165 v2 | 6×4 | 6364 / 6076 / 5728 |
| Water treatment high #166 v2 | 6×4 | 8484 / 8196 / 7848 |
| Water tower #167 v3 | 2×2 | 4224 / 3536 / 2544 |
| Wind #168 v1 | 3×3 | 2320 / 2148 / 1816 (whole assembly) |
| Whole solar farm #169 v3 | 4×4 | 6360 / 6024 / 5688 |

The common power and water reservations deliberately cover every authored upgrade
stage. Native #125 adoption must update logical reservations, preview, placement,
upgrade, current save behavior and support/path integration together. These source
dimensions alone do not change the game catalog or capacities.

## Measured geometry and terrain handoff

All geometry is metres/Z-up/front -Y, with ground-centred pivot and nominal grade
zero. The 295 support records contain actual bottom vertices, ordered contact
loops, hole flags/nesting and horizontal mesh triangles. Curved/hollow stack,
tank, clarifier, riser and mast footprints remain their measured contours. Do not
replace them with filled rectangular foundations. Bounds are descriptive only.
Structural ground, portal sill/trim, cable contact and two elevated coal
transformer under-support regions are distinguished. The native owner decides
which structural regions to union or retain separately without covering lanes
or filling holes; no foundation/deck/pavement mesh is supplied here.

There are nineteen actual access records: eight facilities have separate person
and service-bay lanes; tower/wind/solar have maintenance access only. The tower
target is a ladder, not an invented personnel door. Actual target/fixture bounds
and sill heights are measured separately from nominal grade. The wind door sill
is elevated; use its measured height when building the actual landing. Every
lane is checked against all saved source components, including fixtures. Source
anchors remain authoritative; paths/stairs/public joins belong to #125.

Nuclear334 measured the legacy FbxFactory conversion with `convert_scene=True`,
`convert_scene_unit=True` and scale1: native local cm = `[x,-y,z]*100` from source
metres. This batch supplies that **predicted** basis and offset2 presentation
metadata, while retaining all source-space measurements. #125 must verify the
actual imported basis, triangle contacts, portal outward vectors and board yaw
for each asset. Board yaw is `(entityQuarterTurnsClockwise + presentationOffset)
*90deg`; it is applied after the local import transform. No other utility's
native coordinates have been measured by these offline scripts.

## Materials and LODs

The manifest records actual source node links, input constants, procedural
operations, UV layers, texture color spaces and independently hash-matched reuse
paths. Existing CivicAsphalt/CityMechanical maps are reused; proposed native
texture paths are references to verify, not native assets created by this batch.
Reconstruct tint multiplies and any source concrete/PV procedural detail
explicitly or document a reviewed simplification. FBX does not carry Blender
shader graphs. Normal orientation and ORM R=AO/G=roughness/B=metallic require
native checking; source previews are not native shading acceptance.

LOD1/2 conservatively omit listed small fixtures (with coarser ladder rung
spacing), preserving all retained faces, winding, UV layers, material slots,
structural contacts and overall bounds. There is no blanket decimation or
automatic mobile minimum. These are source LOD options, not measured VR budgets;
native #125 owns transition/platform choices and appearance checks. More
aggressive reduction remains possible if integration shows a concrete need.

## Wind and solar

The original joined wind LOD0 FBX remains unchanged. Additional Body/Rotor FBXs
are supplied at all three levels. Body stays ground-centred; rotor vertices are
translated into axle-local coordinates around source `(0,-2.9,22)m`, axis
`(0,1,0)`. Reassemble the rest pose with rotor local offset plus the source axle.
The verifier proves the combined split faces/UVs/slots equal each whole LOD.
Reflected handedness changes rotation sign; verify current native body/rotor
actor offsets and spin/yaw mapping rather than assuming nuclear's static pose.

Solar remains the approved static whole twelve-table farm. The prepared LOD0
blend retains all434 component groups and the exact original tracking contract:
stationary/yaw/pitch roles, pivots, axes, rest yaw0/tilt23, ranges and clearance
metadata. Small-detail omissions in derived LODs are listed. No runtime tracking,
rigging, cable deformation or single-panel replacement is introduced.

## Reproduce only when changing preparation

Use a fresh isolated checkout/output destination and hydrate the selected
approved `.blend`/`.fbx` LFS files. Strict source verification must pass before
any derived output. Approved source directories must retain their scoped `-text`
rules and pinned bytes. This recipe refuses existing derived output directories.

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.0/blender.exe' --background --factory-startup --disable-autoexec --threads 2 --python-exit-code 1 --python Tools/Assets/Blender/Prepare-CityUtilityIntegration.py
& 'C:/Program Files/Blender Foundation/Blender 5.0/blender.exe' --background --factory-startup --disable-autoexec --threads 2 --python-exit-code 1 --python Tools/Assets/Blender/Verify-CityUtilityIntegration.py
```

Blender 5.0.1 successfully reads the reviewed 5.1.1 sources without saving them.
The prepared copies use 5.0.1. Retained Blender allocator shutdown diagnostics
did not change exit status0 or the independent verification results. Recipes
reproduce geometry/contracts; binary serialization timestamps can change hashes
on a new run. Use the recorded output hashes for this identified handoff.

The other fifteen civic/education/waste/recycling sources and nuclear preparation
are outside this batch. #125 owns native import/adoption and focused Windows
delivery; actual in-game and physical headset acceptance remain unconfirmed.

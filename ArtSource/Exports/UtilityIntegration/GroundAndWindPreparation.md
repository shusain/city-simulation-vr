# Contact columns and wind roles — offline extension of d668972

Read `ArtSource/Manifests/city_utility_ground_wind_preparation.json` as the
extension manifest. It pins the unchanged base integration manifest/proof from
`d668972effba3c74ffdbb9b64b1dd5f6df216daf`, the new outputs and233 unchanged
approved/reference files. `city_utility_ground_wind_verification.json` records
the independent saved-mesh and actual FBX readbacks. This preparation introduces
no Unreal asset, runtime behavior, CI job, push, render or approved-art revision.

## Foundations

Twelve unit-column meshes cover all295 measured source contacts: one group per
utility/version/source-top-Z. Coal has separate Z0 and Z0.25m groups; the other
ten utilities have Z0 groups. Every mesh retains source ground-centred XY, with
local top0m and bottom-1m. Native import is expected to convert those to0/-100cm.
Do not recenter these meshes around their bounds or scale their horizontal axes.

Place the top at building body base + the recorded original source support-top
offset, then stretch **only depth** below terrain. Keep the coal .25m offset when
placing its two transformer under-supports. Unit depth is not a required final
foundation depth. Native #125 owns actual contact placement, terrain clearance,
presentation scale and current runtime adoption.

Each individual column is a closed outward-wound solid with top/bottom caps
using the **original measured horizontal contact triangles**, plus boundary side
walls. The nine clarifier holes remain open; curved footprints retain their
actual vertices. Cap UVs use source XY/2m; side UVs use edge length/depth/2m.
`Concrete` is one semantic slot for binding the existing stock concrete on
import. There is no new texture pack, rectangular pad, footprint union or paving.

The295 individual columns remain disjoint in topology, even when their geometry
overlaps. The131 source positive-area XY overlap pairs are retained and flagged
per utility with exact triangle-intersection areas and source top offsets.
Touching/coincident boundaries are preserved as well. No overlap arbitration or
automatic union is implied; the native owner must preserve or deliberately
resolve these contacts without replacing them with filled bounding boxes.
Pairwise areas are not a union-area or performance metric.

The saved foundation blend contains all12 mesh objects, with each original
contact component identified by a vertex group. The independent proof checks
every component's two-face edge incidence, opposite directed edges, positive
signed unit-depth volume, exact source top/bottom triangle footprints, finite UVs
and normal directions. All nine hole centres pass vertical empty-space rays.
Each of the12 actual FBX readbacks also matches positions, UVs, material and
normals against its saved mesh.

## Wind hierarchy

The approved whole mesh and previous whole/body-rotor LOD exports remain
unchanged. New exports in `wind_turbine/ThreeRoles` provide all three roles at
each of the three LODs:

| Role | Source pivot | LOD0 /1 /2 triangles |
| --- | --- | --- |
| Stationary mast, seams and door/access | (0,0,0)m | 836 /700 /452 |
| Weather-yaw collar/nacelle/hatch/vent/spindle | (0,0,21.164999961853027)m | 568 /532 /448 |
| Spinning hub/spinner/three blades | (0,-2.9,22)m | 916 /916 /916 |

The yaw pivot is the midpoint of the **actual measured Yaw_Bearing_Collar
bounds**, Z20.950000762939453–21.3799991607666m. The independent check confirms
its multiple concentric profile rings share the source vertical centre axis.
This is a measured source pivot, not the old whole-body origin. The rotor axle
remains the original approved contract. Its source offset from the yaw pivot is
(0,-2.9,0.8350000381469727)m.

At rest, reassemble stationary positions directly, yaw-local positions plus the
yaw pivot, and spin-local positions plus the rotor axle. The entire building
hierarchy receives the entity/presentation yaw. Weather yaw then rotates only
the nacelle and rotor branches around the measured collar's source Z axis.
Rotor spin acts about its own source Y axis as a child of the weather-yaw branch.
The ground mast, actual door and path target do not weather-yaw.

Nuclear334 measured the legacy FBX source→native local basis `[x,-y,z]*100cm`.
For these new roles it remains predicted until native checking. The predicted
yaw pivot is(0,0,2116.4999961853027)cm; rotor offset from it is
(0,290,83.50000381469727)cm. Reflected coordinates reverse angular handedness;
verify weather-yaw/spin signs separately from entity/presentation yaw in native
runtime. The previous single-body weather yaw must not be applied to the new
stationary role.

The independent verifier proves exact retained face/UV/material rest reassembly
against all three saved whole LODs. Source corner normals are explicitly copied;
maximum saved-role versus approved-source normal-vector error is1.916e-5 from
Blender split-normal serialization, and actual FBX versus saved-role error is
at most1.633e-7. These are float-precision comparisons, not bit-identical binary
normal storage. All nine wind FBXs pass the combined position/UV/material/normal
readback. Actual native animation, materials, path contacts and gameplay remain
#125 checks.

## Reproduction

Use the unchanged base preparation from d668972 in a fresh extension destination:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.0/blender.exe' --background --factory-startup --disable-autoexec --threads 2 --python-exit-code 1 --python Tools/Assets/Blender/Prepare-CityUtilityGroundAndWind.py
& 'C:/Program Files/Blender Foundation/Blender 5.0/blender.exe' --background --factory-startup --disable-autoexec --threads 2 --python-exit-code 1 --python Tools/Assets/Blender/Verify-CityUtilityGroundAndWind.py
```

The recipe refuses existing extension outputs. Do not regenerate identified
assets merely to inspect or import them. The other15 source designs and solar's
static tracking/cable behavior remain outside this extension.

# Solar structural-foot foundation derivative — #157 / #169

This bounded source correction follows the shared-template recipe at `3bfb7e7`
and the terrain/stairs owner's explicit16-foot request. It changes only the
effective solar foundation import input. The frozen33c source files, native
plan and proofs retain their original hashes. No Unreal execution or runtime
body, cable or articulation change was performed during this preparation.

## Physical supports and exclusions

Retain12 tracker feet (`R1T1_SteelFoot` through `R3T4_SteelFoot`) and4 inverter
feet (`Inverter0_Foot0`, `Inverter0_Foot1`, `Inverter1_Foot0`, `Inverter1_Foot1`).
Each column preserves the original contact XY cap and topology, has its top at
local Z0m and bottom at Z−1m, and is closed with positive signed volume. The
single Concrete material slot continues to use existing textured `M_Concrete`.
The derivative has192 triangles,128 vertices and16 physical columns.

Exclude all17 non-foot contacts:12 `R[123]T[1234]_MastCableRiser` contacts,
`EastGroundDuct`, `CabinetGroundFeed`, and `GroundRowDuct0/1/2`.
The first14 retain their source `cable_ground_contact` kind. The three ducts
retain their frozen `structural_ground_contact` kind and are explicitly excluded
by semantic name. Every excluded name, original kind, contact geometry, bounds,
reason and source-record hash is recorded in the new override manifest. A cable
may geometrically overlap a physical foot; exclusion means no separate cable
column, not removal of the legitimate foot beneath it.

## New files and native route

- `ArtSource/Blender/UtilityIntegration/CityUtility_solar_structural_feet_v3.blend`
- `ArtSource/Exports/UtilityIntegration/Foundations/SM_Utility_solar_farm_v3_StructuralFeet_Z0.fbx`
- `ArtSource/Manifests/city_solar_structural_foundation_override.json`
- `ArtSource/Manifests/city_solar_structural_foundation_verification.json`

The original `SM_Utility_solar_farm_v3_Foundation_Z0.fbx` (33 contacts,
396 triangles) remains a pinned reference. All other11 foundation exports and
all healthy/damaged body and wind inputs remain unchanged.

`CityApprovedUtilitySolar.apply_solar_foundation(preflight())` runs in both
native entry points before native guards or writes. Strict original preflight
still validates the original plan and old exports. The pure override validates
new pinned source/proof/FBX and261 protected inputs, then deep-copies the plan
and changes only the solar foundation LOD0 source/facts. Native destination
`/Game/CitySim/Art/Utilities/ApprovedBatch/Foundations/SM_Utility_solar_farm_v3_Foundation_Z0`
and source topZ0 remain stable. The raw plan hash remains
`8765c24b51df26308d838c12da1a7af867b0903e4ecc14b5a6e0aecbbb764b78`.
The effective recipe still has37 mesh packages and54 source LOD inputs.

Both native receipts include the override provenance; mesh execution rejects
a material receipt from a different override or recipe. Source graph validators,
native mesh import/check functions and shared-template material policy are
unchanged. The original saved proofs are never regenerated to fit this route.

## Checks and handoff

`Verify-CitySolarStructuralFoundation.py` independently loads the saved Blender
mesh, checks exact16 vertex groups and exhaustive face/vertex ownership, actual
contact caps, closed unit columns, source kinds and all17 exclusions, then
imports the exported FBX to verify192 triangles, positions, normals and UVs.
It also verifies original frozen input hashes and other11 foundation groups.

`Check-CitySolarStructuralRecipe.py` runs outside Unreal and verifies exactly
one changed record, stable destination,37/54 package/input counts, original
validators against45c, phase routing/receipts, and rejection of stale or changed
input plans. Its receipt is ignored under `Artifacts/UtilitySourcePreparation`.

The terrain/stairs agent owns review/merge, native import, actual shader/buffer
checks, catalog adoption, runtime placement and Windows playtesting. Native
or headset behavior is unconfirmed by these offline checks. This adds two LFS
inputs (one Blender, one FBX) to the previously identified84 utility recipe inputs.

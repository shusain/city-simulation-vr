# Exact native render expectations — #157 / #125

The native owner found two source-to-render discrepancies in failed batch337
and corroborated them with an isolated engine-only18-case diagnostic at source
`24a3ddee`. This code/proof-only revision follows74064ee. All original approved
sources, prepared Blender/FBX files, raw plan and saved proofs remain immutable.
No asset re-authoring, new source export, Unreal execution or CI occurred here.

## Water tower: exact collapsed apex geometry

Approved #167 v3 has4224/3536/2544 raw source triangles. EachLOD includes54
exactly zero-area `Tank_roof` / `Paint_BlueGrey` triangles. The source lathe
creates28 distinct vertices at the same radius0 apex:28 side quads contribute
28 collapsed triangles, and a28-corner coincident cap contributes26 more.
Prepared-source triangle/polygon/loop indices, positions, component/material,
raw binary FBX polygons and independent Blender FBX readback are retained in
`city_water_tower_render_surface_proof.json`. Raw counts are not rewritten.

The effective native totals are exactly4170/3482/2490. Only BlueGrey counts
change1376→1322,1100→1046,548→494; LightGrey and Mechanical stay unchanged.
No generic count tolerance, small-face threshold or other-asset exception exists.

Exactly one original probe is exclusively on collapsed cap201:LOD2 BlueGrey
sample15 at `(0,0,15.4499998093)m`, up normal, UV `(0,0)`. Exact saved-source
loop membership proves there is no matching valid-face corner. Only this probe
is omitted from native render expectations. All143 other original samples are
retained unchanged and match actual native vertices referenced by render indices
within existing0.2cm /0.0002UV /normal-dot>0.99999 gates. No unused native buffer
vertex is sufficient for a match.

The native owner provided snapshot `ImportDiagnostics1-Geometry.json`, SHA256
`5c89be5bb4e2a40054e78e4e086ba2467e964b2160cf1546cfe8c4564d295f85`.
Offline `Verify-CityUtilityNativeRenderBuffers.py` corroborates exact material
counts and all143 probes; its new pinned proof records native provenance and
diagnostic default bindings. These are diagnostic observations, not production
material, batch, gameplay or visual acceptance.

## SpinRotor: globally unused declared material slot

All three sourceLODs declare OffWhite_PaintedSteel, BlueGrey_Machinery and
Mechanical_Mapped. Saved-source and actual FBX triangle use is exactly824+92,
916 total; Mechanical_Mapped has0 triangles and0 probes in everyLOD. Native
import prunes just that third empty slot. The new rotor proof records source
hashes and the material-use union. Only SpinRotor's effective `material_slots`
becomes the first two names. All geometry/counts/bounds/UVs/normals/probes and
source material graphs/105 native material products remain unchanged; other
whole/role records keep their original slots.

## Routing and native-owner boundary

Both phase entry points run strict original `preflight()`, then the pinned
solar foundation override, then `apply_native_render_expectations`. The helper
deep-copies that exact input, validates three new proof pins and changes only
tower native render counts/samples and SpinRotor's effective slot declaration.
All three overrides appear in the existing receipt provenance and same-code,
same-effective-plan material→mesh guard. Original `preflight`, `validate_graph`,
`import_mesh` and `check_mesh` function ASTs remain identical to reviewed45c.

As explicitly owned by #125, native integration must project the main function's
healthy material bindings to `record.material_slots` before `import_mesh`.
That small binding projection is not performed by this source-policy revision.
Strict core slot/count/probe validators stay unchanged. #125 also owns runtime
generation using effective tower counts, native retry and final aggregateCI,
catalog/runtime adoption, Windows review and eventual headset observations.

`Check-CityUtilityRenderExpectations.py` verifies only two render-policy records
change, all other fields/probes remain intact,37 packages/54LOD inputs persist,
and9 negative cases reject broader count/material/probe/tolerance exceptions.
The separate solar and shared-material offline checkers remain applicable.

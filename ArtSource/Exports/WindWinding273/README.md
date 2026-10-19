# Wind winding correction (#273)

This derivative corrects five inward closed solids in approved #168 v1.
The immutable original remains under `ArtSource/Generated/blender/issue-168/v1`.
The original generic loft helper used Z-axis winding for X/Z rings advancing
along Y, which reverses the nacelle, rear band, spindle, hub and spinner.

`city_wind_winding273.json` records all original hashes, the exact corrected
components, corner parity checks and three LODs for whole/yaw/spin roles.
Only polygon order and corresponding corner-normal signs change. Position,
UV, material, pivot, LOD omission and animation contracts remain unchanged.
The stationary mast does not need replacement. Whole healthy/damaged and
the two moving roles require four native mesh products in total.

`Tools/Assets/Blender/Correct-CityWindWinding.py` creates these fresh outputs.
`Tools/Correct-CityWindWinding.py` is the explicit targeted native importer;
it must run in the coordinated native/CI window. Do not rerun the older
uncorrected batch mesh recipe over retained wind products. The effective
native batch manifest identifies the superseding four-package receipt.

`CitySim.Presentation.WindOutwardSurfaces` independently checks actual render
indices, coherent closed edges, outward solid volumes and single-sided
materials at every LOD. `CitySim.Desktop.WindSurfaceStudy` creates twelve
isolated closeups; it requires the CI visual profile and is manually reviewed.
The cooked-batch test verifies the same asset/LOD references in Windows.

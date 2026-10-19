from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from Assets.Manifests.CityAssetCapabilities import has_capability
import json
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from CityToolPaths import repository_root
from Assets.Manifests.CityArtManifest import load_starter_manifest, ManifestValidationError
import unreal


ROOT = os.fspath(repository_root(__file__))
WIND_ANIMATION_ONLY = "-WindAnimationOnly" in unreal.SystemLibrary.get_command_line()
PARKS_ONLY = "-ParksOnly" in unreal.SystemLibrary.get_command_line()
SCHOOL_ONLY = "-SchoolOnly" in unreal.SystemLibrary.get_command_line()
HOSPITAL_ONLY = "-HospitalOnly" in unreal.SystemLibrary.get_command_line()
WASTE_ONLY = "-WasteOnly" in unreal.SystemLibrary.get_command_line()
UTILITY_UPGRADES_ONLY = "-UtilityUpgradesOnly" in unreal.SystemLibrary.get_command_line()
ALTERNATIVE_FACILITIES_ONLY = "-AlternativeFacilitiesOnly" in unreal.SystemLibrary.get_command_line()
UTILITIES_ONLY = "-UtilitiesOnly" in unreal.SystemLibrary.get_command_line()
DAMAGE_ONLY = "-DamageOnly" in unreal.SystemLibrary.get_command_line()
CIVIC_ONLY = "-CivicOnly" in unreal.SystemLibrary.get_command_line()
STATIONS_ONLY = "-StationUpgradesOnly" in unreal.SystemLibrary.get_command_line()
CONSTRUCTION_ONLY = "-ConstructionOnly" in unreal.SystemLibrary.get_command_line()
ROADS_ONLY = "-RoadsOnly" in unreal.SystemLibrary.get_command_line()
ARCHITECTURE_ONLY = "-ArchitectureOnly" in unreal.SystemLibrary.get_command_line()
BUILDING_REFINEMENT_ONLY = "-BuildingRefinementOnly" in unreal.SystemLibrary.get_command_line()
BUILDING_ADOPTION_ONLY = "-BuildingAdoptionOnly" in unreal.SystemLibrary.get_command_line()
DRY_RUN = "-DryRun" in unreal.SystemLibrary.get_command_line()
REPORT_DIR = os.path.join(ROOT, "Artifacts", "AssetImport")
REPORT_PATH = os.path.join(REPORT_DIR, "city_starter_kit_dry_run.json" if DRY_RUN else "city_starter_kit_import.json")
if BUILDING_REFINEMENT_ONLY:
    REPORT_PATH = os.path.join(REPORT_DIR, "D79-building-refinement-dry-run.json" if DRY_RUN else "D79-building-refinement-import.json")
if BUILDING_ADOPTION_ONLY:
    REPORT_PATH = os.path.join(REPORT_DIR, "D91-building-adoption-dry-run.json" if DRY_RUN else "D91-building-adoption-import.json")
MATERIAL_COLORS = {
    "/Game/CitySim/Art/Materials/MI_CityCharred": (0.19,0.15,0.13),
    "/Game/CitySim/Art/Materials/MI_CityCharredTrim": (0.065,0.06,0.055),
    "/Game/CitySim/Art/Materials/MI_CityFireDetail": (0.10, 0.14, 0.19),
    "/Game/CitySim/Art/Materials/MI_CityPoliceDetail": (0.10, 0.14, 0.19),
    "/Game/CitySim/Art/Materials/MI_CityPolice": (0.12, 0.30, 0.65),
    "/Game/CitySim/Art/Materials/MI_CityFire": (0.75, 0.10, 0.065),
    "/Game/CitySim/Art/Materials/MI_CityResidential": (0.22, 0.58, 0.32),
    "/Game/CitySim/Art/Materials/MI_CityCommercial": (0.12, 0.42, 0.78),
    "/Game/CitySim/Art/Materials/MI_CityIndustrial": (0.86, 0.64, 0.10),
    "/Game/CitySim/Art/Materials/MI_CityPower": (0.70, 0.22, 0.08),
    "/Game/CitySim/Art/Materials/MI_CityService": (0.08, 0.52, 0.58),
    "/Game/CitySim/Art/Materials/MI_CityPark": (0.16, 0.58, 0.20),
    "/Game/CitySim/Art/Materials/MI_CityWindowDetail": (0.10, 0.14, 0.19),
    "/Game/CitySim/Art/Materials/MI_CityRoof": (0.10, 0.14, 0.19),
    "/Game/CitySim/Art/Materials/MI_CityRoad": (0.055, 0.065, 0.08),
    "/Game/CitySim/Art/Materials/MI_CityRoadMarking": (0.92, 0.78, 0.28),
    "/Game/CitySim/Art/Materials/MI_CityTrim": (0.95, 0.62, 0.16),
    "/Game/CitySim/Art/Materials/MI_CityTraffic": (0.88, 0.88, 0.86),
}
MASTER_MATERIAL_PATH = "/Game/CitySim/Art/Materials/M_CityColor"
COLOR_PARAMETER = "CityColor"


try:
    manifest, planned = load_starter_manifest(ROOT)
    errors = []
except ManifestValidationError as error:
    for message in error.errors:
        unreal.log_error(f"CITYSIM_ART_VALIDATION_ERROR {message}")
    raise


def ensure_master_material():
    material = unreal.EditorAssetLibrary.load_asset(MASTER_MATERIAL_PATH)
    if material and not isinstance(material, unreal.Material):
        if not unreal.EditorAssetLibrary.delete_asset(MASTER_MATERIAL_PATH):
            raise RuntimeError(f"Could not replace generated master material {MASTER_MATERIAL_PATH}")
        material = None
    if not material:
        package, name = MASTER_MATERIAL_PATH.rsplit("/", 1)
        factory = unreal.MaterialFactoryNew()
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, package, unreal.Material, factory)
        if not material:
            raise RuntimeError(f"Could not create generated master material {MASTER_MATERIAL_PATH}")
        color_expression = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionVectorParameter, -300, 0)
        color_expression.set_editor_property("parameter_name", COLOR_PARAMETER)
        color_expression.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
        if not unreal.MaterialEditingLibrary.connect_material_property(
                color_expression, "", unreal.MaterialProperty.MP_BASE_COLOR):
            raise RuntimeError(f"Could not connect {COLOR_PARAMETER} on {MASTER_MATERIAL_PATH}")
        roughness_expression = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionConstant, -300, 160)
        roughness_expression.set_editor_property("r", 0.82)
        if not unreal.MaterialEditingLibrary.connect_material_property(
                roughness_expression, "", unreal.MaterialProperty.MP_ROUGHNESS):
            raise RuntimeError(f"Could not connect roughness on {MASTER_MATERIAL_PATH}")
    # City presentation uses UInstancedStaticMeshComponent. Editor preview can
    # auto-enable this usage, but cooked builds cannot and fall back to the gray
    # checkerboard unless the shader permutation is authored before cooking.
    material.set_editor_property("used_with_instanced_static_meshes", True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def ensure_material(path, parent):
    if path.endswith(("MI_CityPoliceDetail", "MI_CityFireDetail")):
        parent = unreal.EditorAssetLibrary.load_asset("/Game/CitySim/Art/Materials/M_CityCivicDetail")
        if not parent: raise RuntimeError("Run Create-CityCivicMaterials.py before police import")
    if path.endswith("MI_CityWindowDetail"):
        parent = unreal.EditorAssetLibrary.load_asset("/Game/CitySim/Art/Materials/M_CityWindowDetail")
        if not parent: raise RuntimeError("Run Create-CitySurfaceMaterials.py before importing window details")
    existing = unreal.EditorAssetLibrary.load_asset(path)
    instance = existing
    if instance:
        current_parent = instance.get_editor_property("parent")
        if not current_parent or current_parent.get_path_name() != parent.get_path_name():
            # These four assets are generated exclusively by this importer. Recreate
            # them when the base material changes so no stale compiled parent survives.
            if not unreal.EditorAssetLibrary.delete_asset(path):
                raise RuntimeError(f"Could not replace generated material {path}")
            instance = None
    if not instance:
        factory = unreal.MaterialInstanceConstantFactoryNew()
        package, name = path.rsplit("/", 1)
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, package, unreal.MaterialInstanceConstant, factory)
    if instance and parent:
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
        if path not in MATERIAL_COLORS:
            raise RuntimeError(f"No authored color configured for {path}")
        unreal.MaterialEditingLibrary.clear_all_material_instance_parameters(instance)
        red, green, blue = MATERIAL_COLORS[path]
        parameter_color = unreal.LinearColor(red, green, blue, 1.0)
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            instance, COLOR_PARAMETER, parameter_color)
        actual_color = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(
            instance, COLOR_PARAMETER)
        if actual_color is None or any(abs(actual - expected) > 0.001 for actual, expected in (
            (actual_color.r, parameter_color.r),
            (actual_color.g, parameter_color.g),
            (actual_color.b, parameter_color.b),
        )):
            raise RuntimeError(f"Could not set {COLOR_PARAMETER} on {path}")
        unreal.EditorAssetLibrary.save_loaded_asset(instance)
    return instance


# Validate the whole manifest first, but allow focused geometry-only iterations.
import_entries = [entry for entry in manifest["assets"] if not ARCHITECTURE_ONLY or
                  (entry.get("progression_family") in ("residential", "commercial", "industrial") and
                   entry.get("progression_stage") in ("low", "medium", "high"))]
if ROADS_ONLY:
    import_entries = [entry for entry in import_entries if entry["asset_id"].startswith("road.")]
if CONSTRUCTION_ONLY:
    import_entries = [entry for entry in import_entries if entry.get("progression_stage") == "construction" or entry.get("animation_role")]
if UTILITY_UPGRADES_ONLY:
    import_entries = [entry for entry in import_entries if entry.get("utility_upgrade")]
if WIND_ANIMATION_ONLY:
    import_entries = [entry for entry in import_entries if entry.get("wind_animation")]
if ALTERNATIVE_FACILITIES_ONLY:
    import_entries = [entry for entry in import_entries if entry.get("alternative_facility")]
if UTILITIES_ONLY:
    import_entries = [entry for entry in import_entries if entry["asset_id"] in ("building.power.a", "building.service.a")]
if CIVIC_ONLY:
    import_entries = [entry for entry in import_entries if entry.get("civic_service") and not entry.get("damaged_variant")]
if STATIONS_ONLY:
    import_entries = [entry for entry in import_entries if entry.get("station_upgrade")]
if PARKS_ONLY:
    import_entries = [e for e in import_entries if e.get("civic_service") == "park"]
if SCHOOL_ONLY:
    import_entries = [e for e in import_entries if e.get("civic_service") == "school"]
if HOSPITAL_ONLY:
    import_entries = [e for e in import_entries if e.get("hospital_asset")]
if WASTE_ONLY:
    import_entries = [e for e in import_entries if e.get("waste_asset")]
if DAMAGE_ONLY:
    import_entries = [entry for entry in import_entries if entry.get("damaged_variant")]
selected_ids = {entry["asset_id"] for entry in import_entries}
if BUILDING_REFINEMENT_ONLY:
    import_entries = [e for e in import_entries if has_capability(e, 'building_refinement')]
    if not import_entries:
        raise RuntimeError("Building-refinement import requires authored entries")
    selected_ids = {entry["asset_id"] for entry in import_entries}
if BUILDING_ADOPTION_ONLY:
    import_entries = [e for e in import_entries if has_capability(e, 'reviewed_building')]
    if not import_entries: raise RuntimeError('Building-adoption import requires reviewed entries')
    selected_ids = {entry['asset_id'] for entry in import_entries}
planned = [entry for entry in planned if entry["asset_id"] in selected_ids]

changed = []
measurements = []
if not DRY_RUN:
    reuse_materials = HOSPITAL_ONLY or WASTE_ONLY or BUILDING_REFINEMENT_ONLY or BUILDING_ADOPTION_ONLY
    master_material = unreal.EditorAssetLibrary.load_asset(MASTER_MATERIAL_PATH) if reuse_materials else ensure_master_material()
    material_paths = sorted({path for entry in import_entries for path in entry["material_slots"].values()})
    # New hospital/waste meshes reuse the accepted shared palette. Do not reset
    # parameters or change parent materials belonging to existing city art.
    surface_paths={path for entry in import_entries if has_capability(entry, 'building_surfaces') or has_capability(entry, 'paired_park_lods') or has_capability(entry, 'reviewed_building') for path in entry['material_slots'].values()}
    materials = {path: unreal.EditorAssetLibrary.load_asset(path) if reuse_materials or path in surface_paths else ensure_material(path, master_material) for path in material_paths}
    if any(value is None for value in materials.values()):
        raise RuntimeError("Could not create all shared material instances")

    for entry in import_entries:
        existing_mesh=unreal.EditorAssetLibrary.load_asset(entry['mesh_destination'])
        if existing_mesh:
            existing_data=existing_mesh.get_editor_property('asset_import_data')
            if isinstance(existing_data,unreal.FbxStaticMeshImportData):
                existing_data.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
        destination_package, destination_name = entry["mesh_destination"].rsplit("/", 1)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(ROOT, entry["export"]))
        task.set_editor_property("destination_path", destination_package)
        task.set_editor_property("destination_name", destination_name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("replace_existing_settings", True)
        task.set_editor_property("save", True)
        task.set_editor_property('factory',unreal.FbxFactory())
        options = unreal.FbxImportUI()
        options.set_editor_property('reset_to_fbx_on_material_conflict',True)
        options.set_editor_property("import_mesh", True)
        options.set_editor_property("import_materials", False)
        options.set_editor_property("import_textures", False)
        options.set_editor_property("import_as_skeletal", False)
        static_options = options.get_editor_property("static_mesh_import_data")
        static_options.set_editor_property("combine_meshes", True)
        static_options.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
        static_options.set_editor_property("convert_scene_unit", True)
        static_options.set_editor_property("convert_scene", True)
        static_options.set_editor_property("import_uniform_scale", 1.0)
        static_options.set_editor_property("generate_lightmap_u_vs", True)
        static_options.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
        static_options.set_editor_property("auto_generate_collision", False if has_capability(entry, 'reviewed_building') else entry["collision_policy"] != "none")
        task.set_editor_property("options", options)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        mesh = unreal.EditorAssetLibrary.load_asset(entry["mesh_destination"])
        if not mesh:
            raise RuntimeError(f"Import did not create {entry['mesh_destination']}")
        imported_data=mesh.get_editor_property('asset_import_data')
        if isinstance(imported_data,unreal.FbxStaticMeshImportData) and imported_data.get_editor_property('vertex_color_import_option')!=unreal.VertexColorImportOption.REPLACE:
            imported_data.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            mesh=unreal.EditorAssetLibrary.load_asset(entry['mesh_destination'])
        static_materials = mesh.get_editor_property("static_materials")
        imported_slots = [str(item.get_editor_property("imported_material_slot_name")) for item in static_materials]
        expected_slots = set(entry["material_slots"].keys())
        if set(imported_slots) != expected_slots or len(imported_slots) != len(expected_slots):
            raise RuntimeError(f"{entry['asset_id']}: slots {imported_slots} do not match {sorted(expected_slots)}")
        for material_index, item in enumerate(static_materials):
            slot = str(item.get_editor_property("imported_material_slot_name"))
            mesh.set_material(material_index, materials[entry["material_slots"][slot]])
        if has_capability(entry, 'paired_park_lods'):
            subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.new_object(unreal.StaticMeshEditorSubsystem)
            if subsystem.import_lod(mesh,1,os.path.join(ROOT,entry['native_lod_export']))!=1:
                raise RuntimeError(f"{entry['asset_id']}: Paired park LOD import failed")
            subsystem.set_lod_screen_sizes(mesh,[1,.025]);mesh.set_minimum_lod_for_platform('Android',1)
        if BUILDING_ADOPTION_ONLY or BUILDING_REFINEMENT_ONLY or WASTE_ONLY or HOSPITAL_ONLY or WIND_ANIMATION_ONLY or ALTERNATIVE_FACILITIES_ONLY or UTILITY_UPGRADES_ONLY or ARCHITECTURE_ONLY or ROADS_ONLY or CONSTRUCTION_ONLY or UTILITIES_ONLY or CIVIC_ONLY or DAMAGE_ONLY or SCHOOL_ONLY or PARKS_ONLY:
            bounds = mesh.get_bounding_box()
            extent = bounds.max - bounds.min
            actual = [extent.x / 100.0, extent.y / 100.0, extent.z / 100.0]
            expected = entry["dimensions_m"]
            # FBX axis conversion can swap horizontal axes; footprint and entrance metadata stay explicit.
            if any(abs(a-b) > 0.02 for a, b in zip(sorted(actual[:2])+actual[2:], sorted(expected[:2])+expected[2:])):
                raise RuntimeError(f"{entry['asset_id']}: imported bounds {actual} differ from manifest {expected}")
            triangles = mesh.get_num_triangles(0)
            budget = 800 if (entry.get("utility_upgrade") or entry.get("civic_service")) and entry["asset_id"].startswith("building.") else 500
            if has_capability(entry, 'building_refinement'):
                budget = 1200
            axle_rotor = entry.get("wind_animation") and entry["asset_id"] == "wind.animation.rotor"
            if axle_rotor and (not bounds.min.z < 0 < bounds.max.z or abs(bounds.min.y + bounds.max.y) > 0.1):
                raise RuntimeError("Wind rotor must straddle its axle pivot")
            sections=mesh.get_num_sections(0)
            surface_budget=has_capability(entry, 'building_surfaces')
            park_budget=has_capability(entry, 'paired_park_lods')
            adopted_budget=has_capability(entry, 'reviewed_building')
            limit=entry['sections'] if surface_budget or park_budget or adopted_budget else 4 if entry.get('hospital_asset') else 2
            if surface_budget or park_budget or adopted_budget:
                budget=entry['triangles']
                if triangles!=budget or sections!=limit:raise RuntimeError(f"{entry['asset_id']}: accepted triangle/section count changed")
            expected_minimum_z=entry.get('bounds_min_m',[0,0,0])[2]*100 if adopted_budget else 0
            if not 0 < triangles <= budget or sections > limit or (not entry.get("animation_role") and not axle_rotor and abs(bounds.min.z-expected_minimum_z) > 0.1):
                raise RuntimeError(f"{entry['asset_id']}: geometry budget or ground pivot failed")
            assignments = [mesh.get_material(index).get_path_name().split('.')[0] for index in range(len(static_materials))]
            if assignments != [entry["material_slots"][slot] for slot in imported_slots]:
                raise RuntimeError(f"{entry['asset_id']}: material assignment did not persist")
            measurements.append({"asset_id": entry["asset_id"], "triangles": triangles,
                                 "dimensions_m": actual, "materials": assignments})
        mesh.modify()
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        changed.append(entry["mesh_destination"])
        unreal.log(f"CITYSIM_ART_IMPORTED Asset={entry['asset_id']} Slots={','.join(imported_slots)}")

os.makedirs(REPORT_DIR, exist_ok=True)
with open(REPORT_PATH, "w", encoding="utf-8") as stream:
    json.dump({"dry_run": DRY_RUN, "architecture_only": ARCHITECTURE_ONLY, "roads_only": ROADS_ONLY, "construction_only": CONSTRUCTION_ONLY, "utilities_only": UTILITIES_ONLY, "civic_only": CIVIC_ONLY, "damage_only": DAMAGE_ONLY, "school_only": SCHOOL_ONLY, "parks_only": PARKS_ONLY, "errors": errors, "planned": planned, "changed": changed, "measurements": measurements}, stream, indent=2)
unreal.log(f"CITYSIM_ART_REPORT DryRun={int(DRY_RUN)} Assets={len(planned)} Changed={len(changed)} Path={REPORT_PATH}")

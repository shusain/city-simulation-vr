"""Read-only art manifest contracts shared by native importers and offline checks."""
from .CityAssetCapabilities import has_capability
from pathlib import Path
import json
import os


class ManifestValidationError(RuntimeError):
    def __init__(self, errors):
        self.errors = errors
        super().__init__(f"Manifest validation failed with {len(errors)} error(s)")


def no_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise RuntimeError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def load_starter_manifest(root):
    ROOT = os.fspath(root)
    manifest_path = os.path.join(ROOT, "ArtSource/Manifests/city_starter_kit.json")
    with open(manifest_path, "r", encoding="utf-8") as stream:
        manifest = json.load(stream, object_pairs_hook=no_duplicates)

    errors = []
    planned = []
    seen_ids = set()
    seen_destinations = set()
    progression_stages = {}
    if manifest.get("schema_version") != 1:
        errors.append("schema_version must be 1")

    for entry in manifest.get("assets", []):
        asset_id = entry.get("asset_id", "")
        destination = entry.get("mesh_destination", "")
        export_path = os.path.join(ROOT, entry.get("export", ""))
        source_path = os.path.join(ROOT, entry.get("source", ""))
        slots = entry.get("material_slots", {})
        if not asset_id or asset_id in seen_ids:
            errors.append(f"Missing or duplicate asset_id: {asset_id}")
        if not destination.startswith("/Game/CitySim/Art/Meshes/") or destination in seen_destinations:
            errors.append(f"Invalid or duplicate mesh_destination: {destination}")
        if not os.path.isfile(source_path):
            errors.append(f"Missing Blender source: {source_path}")
        if not os.path.isfile(export_path):
            errors.append(f"Missing FBX export: {export_path}")
        surface=has_capability(entry, 'building_surfaces')
        park_refined=has_capability(entry, 'paired_park_lods')
        adopted=has_capability(entry, 'reviewed_building')
        section_limit=entry['sections'] if surface or park_refined or adopted else 4 if entry.get('hospital_asset') else 2
        if adopted and (not 0<entry.get('triangles',0)<=(4248 if park_refined else 3854) or not 0<section_limit<=7):
            errors.append(f"{asset_id}: invalid reviewed geometry contract")
        if park_refined and (not 0<entry.get('triangles',0)<=4248 or not os.path.isfile(os.path.join(ROOT,entry.get('native_lod_export','')))):
            errors.append(f"{asset_id}: invalid paired geometry contract")
        if surface and not adopted and (section_limit not in (3,4) or not 0<entry.get('triangles',0)<=1200):
            errors.append(f"{asset_id}: invalid per-entry surface budget")
        if not slots or len(slots)>section_limit or (surface and len(slots)!=section_limit):
            errors.append(f"{asset_id}: exceeds semantic material slot budget")
        if len(entry.get("dimensions_m", [])) != 3 or any(value <= 0 for value in entry.get("dimensions_m", [])):
            errors.append(f"{asset_id}: invalid dimensions_m")
        footprint = entry.get("footprint_cells", [])
        if len(footprint) != 2 or any(value < 0 for value in footprint):
            errors.append(f"{asset_id}: invalid footprint_cells")
        if asset_id.startswith("building."):
            entrance_sides = entry.get("entrance_sides_local", [])
            if not entrance_sides or any(side not in ("-Y", "+X", "+Y", "-X") for side in entrance_sides):
                errors.append(f"{asset_id}: invalid entrance_sides_local")
            yaw_offset = entry.get("presentation_yaw_offset_quarter_turns")
            if not isinstance(yaw_offset, int) or yaw_offset < 0 or yaw_offset > 3:
                errors.append(f"{asset_id}: invalid presentation_yaw_offset_quarter_turns")
        progression_family = entry.get("progression_family")
        progression_stage = entry.get("progression_stage")
        if bool(progression_family) != bool(progression_stage):
            errors.append(f"{asset_id}: progression_family and progression_stage must be paired")
        elif progression_family:
            if progression_stage not in ("construction", "low", "medium", "high"):
                errors.append(f"{asset_id}: invalid progression_stage")
            progression_stages.setdefault(progression_family, set()).add(progression_stage)
            if not entry.get("pivot_policy"):
                errors.append(f"{asset_id}: progression assets require pivot_policy")
        animation_role = entry.get("animation_role")
        if animation_role:
            if animation_role not in ("rotating_crane_jib", "bobbing_crane_hook", "park_skateboard", "park_ball"):
                errors.append(f"{asset_id}: invalid animation_role")
            if not entry.get("pivot_policy"):
                errors.append(f"{asset_id}: animated attachments require pivot_policy")
        seen_ids.add(asset_id)
        seen_destinations.add(destination)
        planned.append({"asset_id": asset_id, "destination": destination, "slots": sorted(slots.keys())})

    if progression_stages.get("residential") != {"construction", "low", "medium", "high"}:
        errors.append("residential progression must contain construction, low, medium, and high stages")

    if errors:
        raise ManifestValidationError(errors)
    return manifest, planned


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise RuntimeError('Duplicate manifest key: ' + key)
        result[key] = value
    return result


def load_seasonal_manifest(root):
    ROOT = Path(root)
    manifest = json.loads((ROOT / 'ArtSource/Manifests/city_seasonal_kit.json').read_text(
        encoding='utf-8'), object_pairs_hook=unique_object)
    assert manifest['schema_version'] == 1
    expected_names = {
        'SM_Scenery_Broadleaf_Autumn_A', 'SM_Scenery_Broadleaf_Winter_A',
        'SM_Scenery_Conifer_Winter_A', 'SM_Seasonal_FlowerBed_A',
        'SM_Seasonal_Pumpkin_A', 'SM_Seasonal_Snowman_A',
        'SM_Seasonal_FestiveTree_A', 'SM_Seasonal_Gift_A',
        'SM_Seasonal_LeafPile_A','SM_Seasonal_WreathAutumn_A','SM_Seasonal_WreathWinter_A',
        'SM_Building_Residential_SeasonalSpace_A','SM_Building_Park_December_A',
        'SM_Building_NeighborhoodPark_December_A'}
    entries = manifest['assets']
    assert {entry['name'] for entry in entries} == expected_names and len(entries) == len(expected_names)
    for entry in entries:
        assert (ROOT / entry['source']).is_file() and (ROOT / entry['export']).is_file(), entry['name']
        assert entry['mesh_destination'] == '/Game/CitySim/Art/Meshes/' + entry['name']
        adopted=has_capability(entry, 'reviewed_building')
        park_refined=has_capability(entry, 'paired_park_lods')
        assert len(entry['material_slots']) <= (4 if adopted or park_refined else 3 if entry['name'] == 'SM_Scenery_Conifer_Winter_A' or has_capability(entry, 'building_surfaces') else 2)
        budget=entry['triangles'] if park_refined else entry['triangle_limit']
        assert 0 < entry['triangles'] <= budget <= (4248 if park_refined else 668 if adopted else 500 if 'Building' in entry['name'] else 360)
        if park_refined:assert (ROOT/entry['native_lod_export']).is_file()
        expected_minimum_z=-.20 if entry['name']=='SM_Building_Residential_SeasonalSpace_A' else 0
        expected_pivot = 'body ground center; entrance walk extends20cm below pivot to sit on lawn' if entry['name']=='SM_Building_Residential_SeasonalSpace_A' else 'ground center'
        assert entry['pivot_policy'] == expected_pivot and abs(entry['bounds_min_m'][2]-expected_minimum_z)<.000001
        assert all(value > 0 for value in entry['dimensions_m'])
        assert adopted or has_capability(entry, 'building_surfaces') or all(path in manifest['material_colors'] for path in entry['material_slots'].values())
    return manifest

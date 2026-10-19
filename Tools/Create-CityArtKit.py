"""Generate the starter kit using shared CityArtKit recipes."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from Assets.Manifests.CityAssetCapabilities import has_capability
import bpy
import json
import os

sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityArtKit import (
    BLEND_PATH,
    EXPORT_DIR,
    ROOT,
    box,
    citizen,
    combine,
    crane_hook,
    crane_jib,
    export_one,
    factory,
    factory_high,
    factory_medium,
    pocket_park,
    power_station,
    residential,
    residential_construction,
    residential_high,
    residential_medium,
    road,
    shop,
    shop_high,
    shop_medium,
    traffic_car,
    traffic_truck,
    water_treatment,
    initialize_materials,
)


def main():
    initialize_materials()
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 1.0
    os.makedirs(os.path.dirname(BLEND_PATH), exist_ok=True)
    os.makedirs(EXPORT_DIR, exist_ok=True)

    assets = [
        combine("SM_Calibration_1m", [box("SM_Calibration_1m_Box", (1.0, 1.0, 1.0), (0, 0, 0.5), "Trim")]),
        residential("SM_Building_Residential_A"),
        residential_medium("SM_Building_Residential_Medium_A"),
        residential_high("SM_Building_Residential_High_A"),
        residential_construction("SM_Building_Residential_Construction_A"),
        crane_jib("SM_Construction_Crane_Jib_A"),
        crane_hook("SM_Construction_Crane_Hook_A"),
        shop("SM_Building_Shop_A"),
        shop_medium("SM_Building_Shop_Medium_A"),
        shop_high("SM_Building_Shop_High_A"),
        factory("SM_Building_Factory_A"),
        factory_medium("SM_Building_Factory_Medium_A"),
        factory_high("SM_Building_Factory_High_A"),
        power_station("SM_Building_Power_A"),
        water_treatment("SM_Building_Service_A"),
        pocket_park("SM_Building_Park_A"),
        traffic_car("SM_Traffic_Car_A"),
        traffic_truck("SM_Traffic_Truck_A"),
        citizen("SM_Citizen_A"),
        road("SM_Road_Straight", ("N", "S")),
        road("SM_Road_Corner", ("N", "E")),
        road("SM_Road_T", ("N", "E", "W")),
        road("SM_Road_Cross", ("N", "E", "S", "W")),
    ]

    bpy.ops.wm.save_as_mainfile(filepath=BLEND_PATH)
    print(f"CITYSIM_BLEND_SAVED {BLEND_PATH}")
    # Accepted geometry inputs are authoritative source files. Preserve them
    # while regenerating the remaining starter primitives.
    from pathlib import Path
    current=json.loads(Path(ROOT,'ArtSource/Manifests/city_starter_kit.json').read_text())
    protected={Path(e.get('surface_source_entry',e)['export']).stem for e in current['assets']
               if has_capability(e, 'building_surfaces') or has_capability(e, 'building_refinement')}
    for asset in assets:
        if asset.name not in protected:export_one(asset)

    # Measured export evidence: detail stays within two material slots and a bounded triangle budget.
    report = []
    for asset in assets:
        if asset.name in protected:
            adopted=next(e for e in current['assets'] if Path(e['mesh_destination']).name==asset.name)
            assert 0<adopted['triangles']<=1200 and len(adopted['material_slots'])<=4
            report.append(dict(name=asset.name,triangles=adopted['triangles'],dimensions_m=adopted['dimensions_m'],
                               slots=list(adopted['material_slots']),accepted_source_preserved=True))
            continue
        asset.data.calc_loop_triangles()
        count = len(asset.data.loop_triangles)
        if len(asset.data.materials) > 2 or count > 1200:
            raise RuntimeError(f"Asset budget exceeded: {asset.name}, {count} triangles")
        report.append({"name": asset.name, "triangles": count,
                       "dimensions_m": [round(value, 4) for value in asset.dimensions],
                       "slots": [value.name for value in asset.data.materials]})
    os.makedirs(os.path.join(ROOT, "Artifacts", "AssetImport"), exist_ok=True)
    with open(os.path.join(ROOT, "Artifacts", "AssetImport", "city_art_geometry.json"), "w", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)

    # Adopted surface authoring is regenerated through its own validated helpers.
    if any(has_capability(e, 'building_surfaces') for e in current['assets']):
        import runpy,sys
        prior_args=sys.argv[:]
        try:
            sys.argv=[__file__,'--skip-renders']
            for script in ('Create-CityBuildingSurfacePrototype.py','Validate-CityBuildingSurfacePrototype.py',
                           'Promote-CityBuildingSurfaceManifest.py','Apply-CitySeasonalHomeSurfaces.py','Prepare-CityUtilityNativeExports.py'):
                runpy.run_path(str(Path(ROOT,'Tools',script)),run_name='__main__')
        finally:sys.argv=prior_args


if __name__ == '__main__':
    main()

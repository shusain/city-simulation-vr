"""Integrate the previously imported D61 meshes, preserving existing catalog order."""
from pathlib import Path
import json
import unreal

root = Path(__file__).resolve().parents[1]
catalog = unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog')
if not catalog:
    raise RuntimeError('Existing catalog missing')
definitions = list(catalog.get_editor_property('definitions'))
construction = next(d for d in definitions if str(d.get_editor_property('definition_id')) == 'Fire').get_editor_property('construction_mesh')
report = []
for identity, title, cost, upkeep, education, expansion in (
        ('WasteDepot', 'Waste Depot', 1800, 50, 1, 1200),
        ('Recycling', 'Recycling Center', 2600, 70, 2, 1800)):
    footprint = unreal.CityCellFootprint(width=4, height=3)
    definition = unreal.CityBuildingDefinition()
    starter = unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Building_' + identity + '_A')
    expanded = unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Building_' + identity + '_Medium_A')
    if not starter or not expanded:
        raise RuntimeError('Import the waste assets first')
    for key, value in dict(definition_id=identity, display_name=title, footprint=footprint, cost=cost,
                           presentation_mesh=starter, construction_mesh=construction, logical_height_cm=1100,
                           entrance_sides=1, presentation_yaw_offset_quarter_turns=2, construction_steps=5,
                           player_purchased_levels=True, jobs_provided=4, minimum_staff_education=education,
                           monthly_upkeep=upkeep, pollution_emission=5 if identity=="WasteDepot" else 2, power_demand=12, water_demand=8).items():
        definition.set_editor_property(key, value)
    levels = []
    for index, kind in enumerate((unreal.CityDensityLevel.LOW, unreal.CityDensityLevel.MEDIUM)):
        level = unreal.CityBuildingLevelDefinition()
        for key, value in dict(level=kind, presentation_mesh=(starter, expanded)[index],
                               jobs_provided=(4, 6)[index], monthly_upkeep=(upkeep, upkeep + 30)[index],
                               power_demand=(12, 18)[index], water_demand=(8, 12)[index],
                               upgrade_cost=(0, expansion)[index], civic_construction_steps=(0, 5)[index],
                               civic_road_range=64, civic_protection_capacity=(2400, 4800)[index],
                               civic_response_vehicles=(2, 3)[index]).items():
            level.set_editor_property(key, value)
        levels.append(level)
    definition.set_editor_property('density_levels', levels)
    existing = next((i for i, d in enumerate(definitions) if str(d.get_editor_property('definition_id')) == identity), None)
    if existing is None:
        definitions.append(definition)
    else:
        definitions[existing] = definition
    report.append(dict(id=identity, cost=cost, upkeep=upkeep, education=education, expansion=expansion))
catalog.set_editor_property('definitions', definitions)
if not unreal.EditorAssetLibrary.save_loaded_asset(catalog):
    raise RuntimeError('Waste catalog save failed')
path = root / 'Artifacts/AssetImport/D61-waste-catalog.json'
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('CITYSIM_WASTE_CATALOG_READY')

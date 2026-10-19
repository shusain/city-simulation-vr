import unreal
path = "/Game/CitySim/Data/DA_CityBuildingCatalog"
catalog = unreal.EditorAssetLibrary.load_asset(path)
if not catalog:
    raise RuntimeError("Existing catalog missing; preserve the installed baseline")
definitions = list(catalog.get_editor_property("definitions"))
definitions = [d for d in definitions if str(d.get_editor_property("definition_id")) != "Police"]
definition = unreal.CityBuildingDefinition()
definition.set_editor_property("definition_id", "Police")
definition.set_editor_property("display_name", "Police Station")
footprint = unreal.CityCellFootprint()
footprint.set_editor_property("width", 3)
footprint.set_editor_property("height", 2)
definition.set_editor_property("footprint", footprint)
definition.set_editor_property("cost", 800)
definition.set_editor_property("logical_height_cm", 900.0)
definition.set_editor_property("power_demand", 10)
definition.set_editor_property("water_demand", 5)
definition.set_editor_property("monthly_upkeep", 25)
definition.set_editor_property("construction_steps", 25)
definition.set_editor_property("presentation_mesh", unreal.load_asset("/Game/CitySim/Art/Meshes/SM_Building_Police_A"))
definition.set_editor_property("presentation_yaw_offset_quarter_turns", 2)
definition.set_editor_property("construction_mesh", unreal.load_asset("/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A"))
definitions.append(definition)
catalog.set_editor_property("definitions", definitions)
if not unreal.EditorAssetLibrary.save_loaded_asset(catalog):
    raise RuntimeError("Police catalog save failed")
unreal.log("CITYSIM_POLICE_CATALOG_SAVED: 3x2; $800; upkeep25; power10; water5")

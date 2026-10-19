import unreal


MAP_PATH = "/Game/CitySim/Maps/CitySandbox"
CATALOG_PATH = "/Game/CitySim/Data/DA_CityBuildingCatalog"
VR_TEMPLATE_MAP_PATH = "/Game/VRTemplate/VRTemplateMap"


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

require(
    level_subsystem.load_level(VR_TEMPLATE_MAP_PATH),
    "Could not load VR template navigation source",
)
if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    require(unreal.EditorAssetLibrary.delete_asset(MAP_PATH), f"Could not replace {MAP_PATH}")
require(
    level_subsystem.new_level_from_template(MAP_PATH, VR_TEMPLATE_MAP_PATH),
    f"Could not create {MAP_PATH} from the VR template",
)

nav_bounds = None
for actor in actor_subsystem.get_all_level_actors():
    if actor.get_class().get_name() == "NavMeshBoundsVolume":
        nav_bounds = actor
    elif actor.get_class().get_name() == "RecastNavMesh":
        actor.set_actor_label("RecastNavMesh-Default")
    else:
        actor_subsystem.destroy_actor(actor)
require(nav_bounds, "VR template navigation bounds are unavailable")
nav_bounds.set_actor_label("CitySandboxNavMeshBounds")
require(
    nav_bounds.set_actor_location(unreal.Vector(0.0, 0.0, 100.0), False, True),
    "Could not center sandbox navigation bounds",
)
nav_bounds.set_actor_scale3d(unreal.Vector(1.5, 2.0, 4.0))

editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

environment_class = require(
    unreal.load_class(None, "/Script/CitySimVR.CitySandboxEnvironment"),
    "CitySandboxEnvironment class is unavailable",
)
game_mode_class = require(
    unreal.load_class(None, "/Script/CitySimVR.CitySandboxGameMode"),
    "CitySandboxGameMode class is unavailable",
)
board_class = require(
    unreal.load_class(None, "/Script/CitySimVR.CityBoardActor"),
    "CityBoardActor class is unavailable",
)
catalog_class = require(
    unreal.load_class(None, "/Script/CitySimVR.CityBuildingCatalog"),
    "CityBuildingCatalog class is unavailable",
)

if unreal.EditorAssetLibrary.does_asset_exist(CATALOG_PATH):
    building_catalog = require(
        unreal.EditorAssetLibrary.load_asset(CATALOG_PATH),
        f"Could not load {CATALOG_PATH}",
    )
else:
    data_asset_factory = unreal.DataAssetFactory()
    data_asset_factory.set_editor_property("data_asset_class", catalog_class)
    building_catalog = require(
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "DA_CityBuildingCatalog",
            "/Game/CitySim/Data",
            catalog_class,
            data_asset_factory,
        ),
        f"Could not create {CATALOG_PATH}",
    )


def building_definition(
        definition_id, display_name, width, height, cost, logical_height_cm,
        presentation_mesh=None, entrance_sides=1, presentation_yaw_offset_quarter_turns=0,
        jobs_provided=0, economic_output=0, power_demand=0, water_demand=0,
        pollution_emission=0, monthly_upkeep=0, construction_mesh=None,
        construction_steps=0, density_levels=None,
        development_demand_type=unreal.CityDevelopmentDemandType.NONE):
    footprint = unreal.CityCellFootprint()
    footprint.set_editor_property("width", width)
    footprint.set_editor_property("height", height)
    definition = unreal.CityBuildingDefinition()
    definition.set_editor_property("definition_id", definition_id)
    definition.set_editor_property("display_name", display_name)
    definition.set_editor_property("footprint", footprint)
    definition.set_editor_property("cost", cost)
    definition.set_editor_property("logical_height_cm", logical_height_cm)
    definition.set_editor_property("entrance_sides", entrance_sides)
    definition.set_editor_property(
        "presentation_yaw_offset_quarter_turns", presentation_yaw_offset_quarter_turns)
    definition.set_editor_property("jobs_provided", jobs_provided)
    definition.set_editor_property("economic_output", economic_output)
    definition.set_editor_property("power_demand", power_demand)
    definition.set_editor_property("water_demand", water_demand)
    definition.set_editor_property("pollution_emission", pollution_emission)
    definition.set_editor_property("monthly_upkeep", monthly_upkeep)
    definition.set_editor_property("construction_steps", construction_steps)
    definition.set_editor_property("development_demand_type", development_demand_type)
    if presentation_mesh:
        definition.set_editor_property("presentation_mesh", require(unreal.load_asset(presentation_mesh), f"Missing presentation mesh {presentation_mesh}"))
    if construction_mesh:
        definition.set_editor_property("construction_mesh", require(unreal.load_asset(construction_mesh), f"Missing construction mesh {construction_mesh}"))
    if density_levels:
        definition.set_editor_property("density_levels", density_levels)
    return definition


def density_level(level, presentation_mesh, residents, power, water, upkeep,
                  upgrade_cost=0, minimum_steps=0, minimum_desirability=0,
                  minimum_demand=0, jobs=0, economic_output=0, pollution=0):
    result = unreal.CityBuildingLevelDefinition()
    result.set_editor_property("level", level)
    result.set_editor_property("presentation_mesh", require(unreal.load_asset(presentation_mesh), f"Missing density mesh {presentation_mesh}"))
    result.set_editor_property("residents_provided", residents)
    result.set_editor_property("jobs_provided", jobs)
    result.set_editor_property("economic_output", economic_output)
    result.set_editor_property("power_demand", power)
    result.set_editor_property("water_demand", water)
    result.set_editor_property("monthly_upkeep", upkeep)
    result.set_editor_property("pollution_emission", pollution)
    result.set_editor_property("upgrade_cost", upgrade_cost)
    result.set_editor_property("minimum_operational_steps", minimum_steps)
    result.set_editor_property("minimum_desirability", minimum_desirability)
    result.set_editor_property("minimum_demand", minimum_demand)
    return result


building_catalog.set_editor_property(
    "definitions",
    [
        # FBX import reverses the authored Blender Y frontage. Keep gameplay
        # entrances on local -Y and rotate these imported meshes two turns.
        building_definition(
            "Home", "Compact Home", 2, 2, 200, 900.0,
            "/Game/CitySim/Art/Meshes/SM_Building_Residential_A.SM_Building_Residential_A", 1, 2,
            construction_mesh="/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A.SM_Building_Residential_Construction_A",
            construction_steps=3,
            development_demand_type=unreal.CityDevelopmentDemandType.RESIDENTIAL,
            density_levels=[
                density_level(unreal.CityDensityLevel.LOW, "/Game/CitySim/Art/Meshes/SM_Building_Residential_A.SM_Building_Residential_A", 10, 10, 10, 5),
                density_level(unreal.CityDensityLevel.MEDIUM, "/Game/CitySim/Art/Meshes/SM_Building_Residential_Medium_A.SM_Building_Residential_Medium_A", 20, 20, 20, 10, 400, 50, 55, 10),
                density_level(unreal.CityDensityLevel.HIGH, "/Game/CitySim/Art/Meshes/SM_Building_Residential_High_A.SM_Building_Residential_High_A", 40, 40, 40, 20, 800, 100, 70, 20),
            ]),
        building_definition(
            "Shop", "Corner Shop", 2, 2, 300, 1200.0,
            "/Game/CitySim/Art/Meshes/SM_Building_Shop_A.SM_Building_Shop_A", 1, 2,
            construction_mesh="/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A.SM_Building_Residential_Construction_A",
            construction_steps=3,
            development_demand_type=unreal.CityDevelopmentDemandType.COMMERCIAL,
            density_levels=[
                density_level(unreal.CityDensityLevel.LOW, "/Game/CitySim/Art/Meshes/SM_Building_Shop_A.SM_Building_Shop_A", 0, 10, 10, 10, jobs=10, economic_output=10),
                density_level(unreal.CityDensityLevel.MEDIUM, "/Game/CitySim/Art/Meshes/SM_Building_Shop_Medium_A.SM_Building_Shop_Medium_A", 0, 18, 18, 18, 500, 50, 45, 10, jobs=20, economic_output=22),
                density_level(unreal.CityDensityLevel.HIGH, "/Game/CitySim/Art/Meshes/SM_Building_Shop_High_A.SM_Building_Shop_High_A", 0, 35, 35, 35, 900, 100, 60, 20, jobs=40, economic_output=50),
            ]),
        building_definition("Park", "Pocket Park", 3, 3, 150, 800.0,
            "/Game/CitySim/Art/Meshes/SM_Building_Park_A.SM_Building_Park_A", 15, 2,
            monthly_upkeep=15),
        building_definition("Power", "Power Station", 2, 2, 500, 1600.0, "/Game/CitySim/Art/Meshes/SM_Building_Power_A.SM_Building_Power_A", 1, 2),
        # Keep the schema-v1 ID "Service" for existing saves; display and tuning are water-specific.
        building_definition("Service", "Water Treatment", 2, 2, 400, 1300.0, "/Game/CitySim/Art/Meshes/SM_Building_Service_A.SM_Building_Service_A", 1, 2),
        building_definition(
            "Factory", "Light Factory", 2, 2, 350, 1400.0,
            "/Game/CitySim/Art/Meshes/SM_Building_Factory_A.SM_Building_Factory_A", 1, 2,
            20, 20, 20, 20, 40, 20,
            construction_mesh="/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A.SM_Building_Residential_Construction_A",
            construction_steps=3,
            development_demand_type=unreal.CityDevelopmentDemandType.INDUSTRIAL,
            density_levels=[
                density_level(unreal.CityDensityLevel.LOW, "/Game/CitySim/Art/Meshes/SM_Building_Factory_A.SM_Building_Factory_A", 0, 20, 20, 20, jobs=20, economic_output=20, pollution=40),
                density_level(unreal.CityDensityLevel.MEDIUM, "/Game/CitySim/Art/Meshes/SM_Building_Factory_Medium_A.SM_Building_Factory_Medium_A", 0, 35, 35, 35, 600, 50, 30, 10, jobs=35, economic_output=45, pollution=70),
                density_level(unreal.CityDensityLevel.HIGH, "/Game/CitySim/Art/Meshes/SM_Building_Factory_High_A.SM_Building_Factory_High_A", 0, 60, 60, 60, 1100, 100, 45, 20, jobs=60, economic_output=90, pollution=120),
            ]),
        building_definition("Police", "Police Station", 3, 2, 800, 900.0,
            presentation_mesh="/Game/CitySim/Art/Meshes/SM_Building_Police_A", presentation_yaw_offset_quarter_turns=2,
            power_demand=10, water_demand=5, monthly_upkeep=25, construction_steps=25,
            construction_mesh="/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A.SM_Building_Residential_Construction_A"),
    ],
)
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from CityStorefrontCatalog import configure
configure(building_catalog)
require(
    unreal.EditorAssetLibrary.save_loaded_asset(building_catalog),
    f"Could not save {CATALOG_PATH}",
)
unreal.log(f"CITYSIM_BUILDING_CATALOG_CREATED {CATALOG_PATH}")

environment = require(
    actor_subsystem.spawn_actor_from_class(
        environment_class, unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)
    ),
    "Could not spawn sandbox environment",
)
environment.set_actor_label("CitySandboxEnvironment")
require(
    environment.set_actor_location(unreal.Vector(0.0, 0.0, 0.0), False, True),
    "Could not position sandbox environment",
)

board = require(
    actor_subsystem.spawn_actor_from_class(
        board_class,
        unreal.Vector(-100.0, 0.0, 100.4),
        unreal.Rotator(0.0, 0.0, 0.0),
    ),
    "Could not spawn city board",
)
board.set_actor_label("CityBoard")
board.set_editor_property("building_catalog", building_catalog)
require(
    board.set_actor_location(unreal.Vector(-100.0, 0.0, 100.4), False, True),
    "Could not position city board",
)
board.set_actor_scale3d(unreal.Vector(0.00390625, 0.00390625, 0.00390625))
board_bounds_origin, board_bounds_extent = board.get_actor_bounds(False)
unreal.log(
    f"CITYSIM_BOARD_BOUNDS origin={board_bounds_origin} extent={board_bounds_extent}"
)

player_start = require(
    actor_subsystem.spawn_actor_from_class(
        unreal.PlayerStart,
        unreal.Vector(0.0, -250.0, 0.0),
        unreal.Rotator(roll=0.0, pitch=-25.0, yaw=90.0),
    ),
    "Could not spawn player start",
)
player_start.set_actor_label("PlayerStart")
require(
    player_start.set_actor_location(unreal.Vector(0.0, -250.0, 0.0), False, True),
    "Could not position player start",
)
player_start.set_actor_rotation(
    unreal.Rotator(roll=0.0, pitch=-25.0, yaw=90.0), True
)
unreal.log(f"CITYSIM_PLAYER_START_ROTATION {player_start.get_actor_rotation()}")

world_settings = require(editor_world.get_world_settings(), "World settings are unavailable")
world_settings.set_editor_property("default_game_mode", game_mode_class)

# The stock VR pawn validates teleport destinations against navigation. The
# scaled template-derived volume covers the full 40 m by 40 m sandbox floor
# with enough vertical range for the standing pawn and tabletop.
nav_bounds_origin, nav_bounds_extent = nav_bounds.get_actor_bounds(False)
unreal.log(
    f"CITYSIM_NAV_BOUNDS origin={nav_bounds_origin} extent={nav_bounds_extent}"
)
unreal.SystemLibrary.execute_console_command(editor_world, "RebuildNavigation")
require(level_subsystem.save_current_level(), f"Could not save {MAP_PATH}")
unreal.log(f"CITYSIM_MAP_CREATED {MAP_PATH}")

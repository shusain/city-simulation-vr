"""Native HUD occlusion review in isolated PIE; no map or player save writes."""
from pathlib import Path
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
menu_only = '-CityHudMenuOnly' in unreal.SystemLibrary.get_command_line()
icon_only = '-CityHudIconsOnly' in unreal.SystemLibrary.get_command_line()
out = root / ('Artifacts/D67-HudPointer-Preview' if icon_only else
              'Artifacts/D67-HudDepthMenu-Preview' if menu_only else 'Artifacts/D67-HudDepth-Preview')
out.mkdir(parents=True, exist_ok=True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
map_path = root / 'CitySimVR/Content/CitySim/Maps/CitySandbox.umap'
map_bytes = map_path.read_bytes()
blocker = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -10000))
blocker.set_actor_label('CityHudOccluder')
blocker.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
blocker.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
blocker.static_mesh_component.set_material(0, unreal.load_asset('/Game/CitySim/Art/Materials/M_CityRoomFloor'))
blocker.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
blocker.static_mesh_component.set_cast_shadow(False)
views = ['ClearHud', 'LargeMap', 'OccludedHud', 'OccludedMenu', 'OccludedReport', 'OccludedSetup']
if menu_only:
    views = views[3:]
if icon_only:
    views = ['OccludedSettings', 'OccludedSettingsReopened']
samples = []
phase = 0
index = 0
start = time.monotonic()
deadline = start + 3
failure = None


def master_path(material):
    while isinstance(material, unreal.MaterialInstance):
        material = material.get_editor_property('parent')
    return material.get_path_name() if material else '<engine fallback>'


def finish(error=None):
    global phase, deadline, failure
    failure = error
    level.editor_request_end_play()
    phase, deadline = 90, time.monotonic() + 3


def tick(delta):
    global phase, deadline, index, ui, board, sim, pawn, player, obstacle, pose, location
    try:
        now = time.monotonic()
        if now < deadline:
            return
        if phase < 90 and now - start > 150:
            raise RuntimeError('HUD overlay preview timeout')
        if phase == 90:
            if level.is_in_play_in_editor():
                deadline = now + 1
                return
            actors.destroy_actor(blocker)
            assert map_path.read_bytes() == map_bytes, 'Map bytes changed'
            (out / 'Report.json').write_text(json.dumps(dict(
                samples=samples, error=failure, mapBytesUnchanged=True), indent=2))
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
            return
        if phase == 0:
            level.editor_request_begin_play()
            phase, deadline = 1, now + 6
            return
        world = editor.get_game_world()
        if phase == 1:
            all_actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            ui = next(a for a in all_actors if a.get_class().get_name() == 'CityVRToolController')
            board = next(a for a in all_actors if a.get_class().get_name() == 'CityBoardActor')
            obstacle = next(a for a in all_actors if a.get_actor_label() == 'CityHudOccluder')
            sim = next(o for o in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if o.get_outer() == world)
            error = sim.load_city_from_path(str(root / 'Artifacts/Preview/D66-ResidentialGround-FlatAutumn-City.json'), False)
            if error:
                raise RuntimeError(str(error))
            sim.set_simulation_paused(True)
            ui.return_to_city()
            ui.set_city_summary_visible(True)
            ui.set_objectives_visible(True)
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            pose = board.get_actor_transform()
            center = unreal.MathLibrary.transform_location(pose, unreal.Vector(25600, 25600, 0))
            location = center + unreal.Vector(0, -225, 260)
            player = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            # Menu presentation preserves pawn-owned visuals, including this QA blocker.
            obstacle.set_owner(pawn)
            pawn.set_actor_location(location - unreal.Vector(0, 0, pawn.get_editor_property('base_eye_height')), False, True)
            player.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location, center))
            unreal.SystemLibrary.execute_console_command(world, 'FOV 90')
            unreal.SystemLibrary.execute_console_command(world, 'DisableAllScreenMessages')
            phase, deadline = 2, now + 2
            return
        if phase == 2:
            view = views[index]
            ui.return_to_city()
            board.set_actor_transform(pose, False, True)
            if view == 'LargeMap':
                scale = pose.scale3d * 2
                board.set_actor_scale3d(scale)
                board.set_actor_location(location + unreal.MathLibrary.get_forward_vector(player.get_control_rotation()) * 60
                                         - unreal.Vector(25600 * scale.x, 25600 * scale.y, 50), False, True)
            obstacle.set_actor_location(location + unreal.MathLibrary.get_forward_vector(player.get_control_rotation()) * 45
                                        if view.startswith('Occluded') else unreal.Vector(0, 0, -10000), False, True)
            obstacle.set_actor_rotation(player.get_control_rotation(), True)
            obstacle.set_actor_scale3d(unreal.Vector(.1, 4, 4))
            if view in ('OccludedMenu', 'OccludedMenuReopened'):
                ui.show_main_menu()
            elif view in ('OccludedSettings', 'OccludedSettingsReopened'):
                ui.show_game_settings()
            elif view == 'OccludedReport':
                ui.show_reports(unreal.CityReportPeriod.MONTHLY)
            elif view == 'OccludedSetup':
                ui.show_new_city_setup()
            phase, deadline = 3, now + 3
            return
        if phase == 3:
            # Entry/menu transitions can relocate the avatar. Place the blocker
            # relative to the actual render camera after that transition settles.
            if views[index].startswith('Occluded'):
                # The entry scene hides ordinary world actors on menu entry.
                obstacle.set_actor_hidden_in_game(False)
                obstacle.static_mesh_component.set_visibility(True)
                camera = player.player_camera_manager
                rotation = camera.get_camera_rotation()
                obstacle.set_actor_location(camera.get_camera_location()
                    + unreal.MathLibrary.get_forward_vector(rotation) * 45, False, True)
                obstacle.set_actor_rotation(rotation, True)
            phase, deadline = 30, now + 1
            return
        if phase == 30:
            view = views[index]
            texts = {c.get_name(): str(c.get_editor_property('text'))
                     for c in ui.get_components_by_class(unreal.TextRenderComponent) if c.is_visible()}
            materials = {c.get_name(): master_path(c.get_material(0))
                         for c in ui.get_components_by_class(unreal.PrimitiveComponent)
                         if c.is_visible() and c.get_num_materials() and c.get_material(0)}
            samples.append(dict(view=view, visibleTexts=texts, materials=materials,
                                obstacleDistanceCm=unreal.MathLibrary.vector_distance(
                                    obstacle.get_actor_location(), player.player_camera_manager.get_camera_location())))
            if icon_only:
                icons = [c for c in ui.get_components_by_class(unreal.StaticMeshComponent)
                         if c.get_name().startswith('PaletteIcon') and c.is_visible()]
                assert icons, 'No visible menu icons'
                for c in icons:
                    material = c.get_material(0)
                    assert master_path(material).endswith('M_CityHudIcon.M_CityHudIcon'), c.get_name()
                    assert material.get_texture_parameter_value('IconTexture'), 'Missing icon texture'
                samples[-1]['iconTextures'] = {c.get_name(): c.get_material(0).get_texture_parameter_value(
                    'IconTexture').get_path_name() for c in icons}
            if view.startswith('Occluded'):
                assert abs(samples[-1]['obstacleDistanceCm'] - 45) < .01, 'Blocker is not in front of HUD'
            unreal.SystemLibrary.execute_console_command(world,
                f'HighResShot 1600x1200 filename="{(out / (view + ".png")).as_posix()}"')
            phase, deadline = 4, now + 3
            return
        if phase == 4:
            assert (out / (views[index] + '.png')).exists(), 'Screenshot missing'
            index += 1
            if index == len(views):
                finish()
            else:
                phase, deadline = 2, now + 1
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)

"""Capture imported normal and damaged schools in temporary PIE; never save the map.

Run with UnrealEditor -NoHMD -ExecCmds="py <absolute script path>".
"""
from pathlib import Path
import json
import time
import unreal
import sys

# Find Tools by its shared resolver, including direct execution from Unreal.
tools = next(parent for parent in Path(__file__).resolve().parents if (parent / 'CityToolPaths.py').is_file())
sys.path.insert(0, str(tools))
from CityToolPaths import repository_root
from Capture.CityCaptureSession import CaptureSession, capture_paths

def main(output_directory=None):
    root = repository_root(__file__)
    outputs, report, error = capture_paths(root, 'Schools', output_directory)
    entries = [e for e in json.loads((root / 'ArtSource/Manifests/city_starter_kit.json').read_text(encoding='utf-8'))['assets'] if e.get('civic_service')=='school']
    session = CaptureSession(unreal, error, timeout_message='Damage preview timed out')
    level = session.level
    editor = session.editor
    start = session.started
    deadline = start + 5
    phase = 0
    samples = []


    actors = session.actors
    gallery = session.owned_actors


    def prepare_gallery():
        # Temporary editor actors are copied into PIE. Python does not expose the
        # runtime World spawn API; never save these actors into the sandbox map.
        floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 100, 75))
        gallery.append(floor)
        floor.set_actor_label('SchoolGalleryFloor')
        floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
        floor.static_mesh_component.set_material(0, unreal.load_asset('/Game/CitySim/Art/Materials/MI_CityResidential'))
        floor.set_actor_scale3d(unreal.Vector(8, 5, .05))
        for i, entry in enumerate(entries):
            mesh = unreal.load_asset(entry['mesh_destination'])
            if not mesh:
                raise RuntimeError('Missing damage mesh: '+entry['mesh_destination'])
            location = unreal.Vector((i-.5)*180, 100, 77.5)
            actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, location)
            gallery.append(actor)
            actor.set_actor_label('SchoolGallery'+str(i))
            actor.static_mesh_component.set_static_mesh(mesh)
            actor.set_actor_scale3d(unreal.Vector(.05, .05, .05))
            samples.append({'asset':entry['asset_id'], 'mesh':mesh.get_path_name(), 'position':[location.x, location.y, location.z]})


    def camera(world, location):
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        controller = unreal.GameplayStatics.get_player_controller(world, 0)
        pawn.set_actor_location(location - unreal.Vector(0, 0, pawn.get_editor_property('base_eye_height')), False, True)
        controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector(0, 100, 105)))


    def tick(delta):
        nonlocal phase, deadline
        now = time.monotonic()
        if now < deadline:
            return
        if phase == 0:
            prepare_gallery()
            level.editor_request_begin_play()
            phase = 1
            deadline = now + 8
        elif phase == 1:
            world = editor.get_game_world()
            if not world or not level.is_in_play_in_editor():
                return
            for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
                if actor.get_class().get_name() == "CityBoardActor":
                    actor.set_actor_tick_enabled(False)
                if not actor.get_actor_label().startswith('SchoolGallery'):
                    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
                        component.set_visibility(False, False)
            camera(world, unreal.Vector(0, -500, 780))
            for command in ['FOV 60', 'DisableAllScreenMessages', 't.IdleWhenNotForeground 0', 'r.EyeAdaptationQuality 0']:
                unreal.SystemLibrary.execute_console_command(world, command)
            phase = 2
            deadline = now+5
        elif phase in (2, 4):
            output = outputs[0 if phase == 2 else 1]
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(), f'HighResShot 1600x1200 filename="{output.as_posix()}"')
            phase += 1
            deadline = now+3
        elif phase == 3:
            camera(editor.get_game_world(), unreal.Vector(0, 700, 780))
            phase = 4
            deadline = now+3
        elif all(p.exists() and p.stat().st_size > 0 for p in outputs):
            report.write_text(json.dumps({'source':'imported normal and damaged schools in temporary desktop PIE', 'assets':samples, 'saved_map':False}, indent=2), encoding='utf-8')
            session.finish()


    session.start(tick)


if __name__ == "__main__":
    main()

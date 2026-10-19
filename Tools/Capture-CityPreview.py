"""Run in a fresh UnrealEditor GUI process with -ExecCmds="py <this script>", -NoHMD.

Captures the running sandbox in PIE without saving or editing the map.
Desktop rendering is useful for geometry/material checks, not Quest validation.
"""
from pathlib import Path
import time
import traceback
import unreal

root = Path(__file__).resolve().parent.parent
output = root / 'Artifacts/Preview/City-PIE.png'
output.parent.mkdir(parents=True, exist_ok=True)
output.unlink(missing_ok=True)
error = output.with_suffix('.error.txt')
error.unlink(missing_ok=True)
start = time.monotonic()
phase = 0
deadline = start + 5
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    global phase, deadline
    try:
        now = time.monotonic()
        if now - start > 120:
            raise RuntimeError('PIE capture timed out')
        if now < deadline:
            return
        if phase == 0:
            level.editor_request_begin_play()
            phase = 1
            deadline = now + 8
        elif phase == 1:
            world = editor.get_game_world()
            if not level.is_in_play_in_editor() or not world:
                return
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            if not controller:
                return
            location = unreal.Vector(0, -200, 320)
            rotation = unreal.MathLibrary.find_look_at_rotation(location, unreal.Vector(0, 100, 100))
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            if not pawn:
                raise RuntimeError('PIE has no possessed desktop pawn')
            eye_height = pawn.get_editor_property('base_eye_height')
            pawn.set_actor_location(location - unreal.Vector(0, 0, eye_height), False, True)
            controller.set_control_rotation(rotation)
            unreal.SystemLibrary.execute_console_command(world, 'FOV 60')
            unreal.SystemLibrary.execute_console_command(world, 'DisableAllScreenMessages')
            phase = 2
            deadline = now + 8
        elif phase == 2:
            unreal.SystemLibrary.execute_console_command(
                editor.get_game_world(), f'HighResShot 1280x960 filename="{output.as_posix()}"')
            phase = 3
            deadline = now + 4
        elif output.exists() and output.stat().st_size > 0:
            finish()
    except Exception:
        error.write_text(traceback.format_exc(), encoding='utf-8')
        finish()

handle = unreal.register_slate_post_tick_callback(tick)

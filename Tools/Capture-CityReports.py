"""Inspect D42 native Reports UI with explicitly synthetic chart values in temporary PIE; restore local save bytes."""
from pathlib import Path
import hashlib
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
out = root / "Artifacts/Preview/D42-ReportsMonthly.png"
report = out.with_suffix(".json")
elevated = out.with_name("D42-ReportsAnnual.png")
error = out.with_suffix(".error.txt")
for path in (out, elevated, report, error):
    path.unlink(missing_ok=True)
save = root / "CitySimVR/Saved/SaveGames/CitySaveV1.json"
prior = save.read_bytes() if save.exists() else None
fixture = root / "Artifacts/Preview/ReportHistory-ReportsFixture.json"
restore_needed = False
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start = time.monotonic()
deadline = start + 5
phase = 0
sections = 0

def restore():
    global restore_needed
    if not restore_needed:
        return
    if prior is None:
        save.unlink(missing_ok=True)
    else:
        save.write_bytes(prior)
    restore_needed = False

def finish():
    restore()
    unreal.unregister_slate_post_tick_callback(handle)
    level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    global phase, deadline, restore_needed, sections
    try:
        now = time.monotonic()
        if now - start > 120:
            raise RuntimeError("Road PIE capture timed out")
        if now < deadline:
            return
        if phase == 0:
            level.editor_request_begin_play()
            phase = 1
            deadline = now + 8
        elif phase == 1:
            world = editor.get_game_world()
            if not world or not level.is_in_play_in_editor():
                return
            actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            ui = next(a for a in actors if a.get_class().get_name() == "CityVRToolController")
            board = next(a for a in actors if a.get_class().get_name() == "CityBoardActor")
            save.parent.mkdir(parents=True, exist_ok=True)
            restore_needed = True
            save.write_bytes(fixture.read_bytes())
            ui.show_main_menu()
            ui.review_city_load()
            ui.confirm_reviewed_city_load()
            restore()
            if ui.is_main_menu_environment_active() or board.get_editor_property("hidden"):
                raise RuntimeError("Fixture did not load through the actual menu")
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            pawn.set_actor_location(unreal.Vector(0, 0, 100), False, True)
            unreal.GameplayStatics.get_player_controller(world, 0).set_control_rotation(unreal.Rotator(0, 0, 0))
            ui.show_reports(False)
            for command in ("FOV 90", "DisableAllScreenMessages", "t.IdleWhenNotForeground 0", "r.EyeAdaptationQuality 0"):
                unreal.SystemLibrary.execute_console_command(world, command)
            phase = 2
            deadline = now + 4
        elif phase == 2:
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),
                f'HighResShot 1800x1400 filename="{out.as_posix()}"')
            phase = 3
            deadline = now + 3
        elif phase == 3 and out.exists() and out.stat().st_size > 0:
            world = editor.get_game_world()
            board = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if a.get_class().get_name() == "CityBoardActor")
            ui = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if a.get_class().get_name() == "CityVRToolController")
            ui.show_reports(True)
            phase = 4
            deadline = now + 3
        elif phase == 4:
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(), f'HighResShot 1800x1400 filename="{elevated.as_posix()}"')
            phase = 5
            deadline = now + 3
        elif phase == 5 and elevated.exists() and elevated.stat().st_size > 0:
            after = save.read_bytes() if save.exists() else None
            if after != prior:
                raise RuntimeError("Original local save changed")
            report.write_text(json.dumps({"source": "native Reports screen; isolated synthetic chart values for layout QA",
                "captures": [out.name, elevated.name], "savedMap": False,
                "localSavePreserved": True, "fixtureMonths": 30}, indent=2), encoding="utf-8")
            finish()
    except Exception:
        error.write_text(traceback.format_exc(), encoding="utf-8")
        finish()

handle = unreal.register_slate_post_tick_callback(tick)

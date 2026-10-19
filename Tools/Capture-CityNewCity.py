"""Capture isolated D49 menu, load review and real city transition in temporary PIE."""
from pathlib import Path
import hashlib
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
out = root / "Artifacts/Preview"
out.mkdir(parents=True, exist_ok=True)
report = out / "D50-NewCity.json"
error = report.with_suffix(".error.txt")
names = ("Main", "Setup", "SetupNight", "CreateReview", "LoadReview", "City", "NewCity")
outputs = [out / f"D50-NewCity-{name}.png" for name in names]
for path in outputs + [report, error]:
    path.unlink(missing_ok=True)
save = root / "CitySimVR/Saved/SaveGames/CitySaveV1.json"
prior_bytes = save.read_bytes() if save.exists() else None
prior = hashlib.sha256(prior_bytes).hexdigest() if prior_bytes is not None else None
fixture = root / "Artifacts/Diagnostics/D49-menu-device-before.json"
needs_restore = False
city_dir = save.parent / "Cities"
prior_city_files = set(city_dir.glob("*")) if city_dir.exists() else set()
created_city_files = []
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start = time.monotonic()
deadline = start + 5
phase = 0
samples = []
ui = None
board = None


def finish():
    restore()
    for path in created_city_files:
        path.unlink(missing_ok=True)
    unreal.unregister_slate_post_tick_callback(handle)
    level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()

def restore():
    global needs_restore
    if not needs_restore:
        return
    if prior_bytes is None:
        save.unlink(missing_ok=True)
    else:
        save.write_bytes(prior_bytes)
    needs_restore = False


def tick(delta):
    global phase, deadline, ui, board, needs_restore
    try:
        now = time.monotonic()
        if now - start > 120:
            raise RuntimeError("Entry menu preview timed out")
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
            board = next(a for a in actors if a.get_class().get_name() == "CityBoardActor")
            ui = next(a for a in actors if a.get_class().get_name() == "CityVRToolController")
            ui.show_main_menu()
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            for command in ("FOV 90", "DisableAllScreenMessages", "t.IdleWhenNotForeground 0", "r.EyeAdaptationQuality 0"):
                unreal.SystemLibrary.execute_console_command(world, command)
            phase = 2
            deadline = now + 3
        elif phase in (2, 4, 6, 8, 10, 12, 14):
            index = (phase - 2) // 2
            texts = {c.get_name(): str(c.get_editor_property("text"))
                     for c in ui.get_components_by_class(unreal.TextRenderComponent)
                     if c.get_name().startswith("Palette") and c.is_visible()}
            help_text = texts.get("PaletteHelp", "")
            if index == 0 and "CONTINUE" not in help_text:
                raise RuntimeError("Missing real entry menu")
            if index in (1, 2) and "Review before creating" not in help_text:
                raise RuntimeError("Missing honest setup review")
            active = ui.is_main_menu_environment_active()
            hidden = bool(board.get_editor_property("hidden"))
            if active != (index < 5) or hidden != (index < 5):
                raise RuntimeError("Incorrect menu/city transition visibility")
            if index == 0 and "TOOLS" in texts.values():
                raise RuntimeError("Tools escape remains in start menu")
            if index == 4 and "LOAD SAVED" not in texts.values():
                raise RuntimeError("Missing load confirmation")
            if index >= 5 and texts:
                raise RuntimeError("Menu still visible after load")
            samples.append({"name": names[index], "texts": texts,
                "menu_environment_active": active, "board_hidden": hidden})
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),
                f'HighResShot 1800x1400 filename="{outputs[index].as_posix()}"')
            phase += 1
            deadline = now + 3
        elif phase == 3:
            ui.show_new_city_setup()
            phase = 4
            deadline = now + 3
        elif phase == 5:
            board.set_lighting_preset(unreal.CityLightingPreset.NIGHT)
            phase = 6
            deadline = now + 3
        elif phase == 7:
            ui.review_city_creation()
            phase = 8
            deadline = now + 3
        elif phase == 9:
            save.parent.mkdir(parents=True, exist_ok=True)
            if prior_bytes is not None:
                (out / "D50-NewCity-LocalSaveBackup.json").write_bytes(prior_bytes)
            needs_restore = True
            save.write_bytes(fixture.read_bytes())
            ui.review_city_load()
            phase = 10
            deadline = now + 3
        elif phase == 11:
            ui.confirm_reviewed_city_load()
            restore()
            world = editor.get_game_world()
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            pose = board.get_actor_transform()
            aim = unreal.MathLibrary.transform_location(pose, unreal.Vector(25600, 25600, 1000))
            location = unreal.MathLibrary.transform_location(pose, unreal.Vector(25600, -15000, 40000))
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            pawn.set_actor_location(location - unreal.Vector(0, 0, pawn.get_editor_property("base_eye_height")), False, True)
            unreal.GameplayStatics.get_player_controller(world, 0).set_control_rotation(
                unreal.MathLibrary.find_look_at_rotation(location, aim))
            phase = 12
            deadline = now + 4
        elif phase == 13:
            ui.show_main_menu()
            ui.show_new_city_setup()
            ui.review_city_creation()
            ui.confirm_reviewed_city_creation()
            new_files = set(city_dir.glob("*")) - prior_city_files
            created_city_files.extend(new_files)
            if len(new_files) != 1:
                raise RuntimeError("Creation did not produce exactly one protected save")
            created = next(iter(new_files))
            doc = json.loads(created.read_text(encoding="utf-8"))
            if created.name != doc["cityId"] + ".json" or doc["funds"] != "10000" or doc["buildings"]:
                raise RuntimeError("Invalid fresh city document")
            phase = 14
            deadline = now + 4
        elif all(p.exists() and p.stat().st_size > 0 for p in outputs):
            after = hashlib.sha256(save.read_bytes()).hexdigest() if save.exists() else None
            if after != prior:
                raise RuntimeError("Local save changed during menu preview")
            report.write_text(json.dumps({"source": "actual native setup/review, saved-city load and protected new-city creation in temporary Unreal PIE",
                "samples": samples, "local_save_preserved": True, "saved_map": False}, indent=2), encoding="utf-8")
            finish()
    except Exception:
        error.write_text(traceback.format_exc(), encoding="utf-8")
        finish()


handle = unreal.register_slate_post_tick_callback(tick)

"""Capture D50 native new-city and library previews in temporary PIE; preserve local save/catalog bytes."""
from pathlib import Path
import json
import time
import traceback
import unreal
root = Path(__file__).resolve().parents[1]
outputs = [root/'Artifacts/Preview/D50-NewTerrain.png', root/'Artifacts/Preview/D50-NewCityReview.png', root/'Artifacts/Preview/D50-LoadPreview.png']
report = root/'Artifacts/Preview/D50-TerrainPreview.json'
error = report.with_suffix('.error.txt')
for path in outputs+[report,error]: path.unlink(missing_ok=True)
saved = root/'CitySimVR/Saved/SaveGames'
paths = [saved/'CitySaveV1.json',saved/'Cities/LibraryIndex.json',saved/'Cities/LibraryIndex.json.bak',saved/'Cities/LibraryIndex.json.tmp']
prior = {path: path.read_bytes() if path.exists() else None for path in paths}
fixture = root/'Artifacts/Preview/ReportHistory-ReportsFixture.json'
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start=time.monotonic(); deadline=start+5; phase=0
ui=None

def restore():
    for path,data in prior.items():
        if data is None: path.unlink(missing_ok=True)
        else: path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)

def finish():
    restore()
    unreal.unregister_slate_post_tick_callback(handle)
    level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()

def capture(index):
    unreal.SystemLibrary.execute_console_command(editor.get_game_world(),f'HighResShot 1800x1400 filename="{outputs[index].as_posix()}"')

def tick(delta):
    global phase,deadline,ui
    try:
        now=time.monotonic()
        if now-start>120: raise RuntimeError('Terrain preview capture timed out')
        if now<deadline:return
        if phase==0:
            level.editor_request_begin_play();phase=1;deadline=now+8
        elif phase==1:
            world=editor.get_game_world()
            if not world or not level.is_in_play_in_editor():return
            ui=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if a.get_class().get_name()=='CityVRToolController')
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            pawn.set_actor_location(unreal.Vector(0,0,100),False,True)
            unreal.GameplayStatics.get_player_controller(world,0).set_control_rotation(unreal.Rotator(0,0,0))
            ui.show_main_menu();ui.show_new_city_setup()
            for command in ('FOV 75','DisableAllScreenMessages','t.IdleWhenNotForeground 0','r.EyeAdaptationQuality 0'):
                unreal.SystemLibrary.execute_console_command(world,command)
            phase=2;deadline=now+4
        elif phase==2:
            capture(0);phase=3;deadline=now+3
        elif phase==3 and outputs[0].exists():
            ui.review_city_creation();phase=4;deadline=now+3
        elif phase==4:
            capture(1);phase=5;deadline=now+3
        elif phase==5 and outputs[1].exists():
            paths[0].parent.mkdir(parents=True,exist_ok=True);paths[0].write_bytes(fixture.read_bytes())
            ui.show_main_menu();ui.review_city_load();phase=6;deadline=now+3
        elif phase==6:
            capture(2);phase=7;deadline=now+3
        elif phase==7 and outputs[2].exists():
            restore()
            for path,data in prior.items():
                assert (path.read_bytes() if path.exists() else None)==data,f'Local save/catalog changed: {path}'
            report.write_text(json.dumps({'source':'native new-city setup/review and library terrain previews in temporary PIE','savedMap':False,'localSaveAndCatalogPreserved':True,'captures':[p.name for p in outputs],'loadFixture':'isolated synthetic Reports fixture; never a player city'},indent=2),encoding='utf-8')
            finish()
    except Exception:
        error.write_text(traceback.format_exc(),encoding='utf-8');finish()
handle=unreal.register_slate_post_tick_callback(tick)

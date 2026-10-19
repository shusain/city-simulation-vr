"""Capture D51 native earthquake and tornado effects in temporary PIE; preserve local save/catalog bytes."""
from pathlib import Path
import json
import time
import traceback
import unreal
root = Path(__file__).resolve().parents[1]
outputs = [root/'Artifacts/Preview/D51-OnsetQuake.png', root/'Artifacts/Preview/D51-OnsetTornado.png']
report = root/'Artifacts/Preview/D51-DisasterOnset.json'
error = report.with_suffix('.error.txt')
for path in outputs+[report,error]: path.unlink(missing_ok=True)
saved = root/'CitySimVR/Saved/SaveGames'
paths = [saved/'CitySaveV1.json',saved/'Cities/LibraryIndex.json',saved/'Cities/LibraryIndex.json.bak',saved/'Cities/LibraryIndex.json.tmp']
prior = {path: path.read_bytes() if path.exists() else None for path in paths}
fixture = root/'Artifacts/Preview/NaturalDisasters-NaturalBaseline.json'
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start=time.monotonic(); deadline=start+5; phase=0
ui=None
positions=[]
last_sample=0

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
    global phase,deadline,ui,board,effects,camera,target,last_sample
    try:
        now=time.monotonic()
        if now-start>120: raise RuntimeError('Terrain preview capture timed out')
        if phase==4 and now-last_sample>.05 and effects['TornadoParticles'].is_active():
            p=effects['TornadoParticles'].get_editor_property('relative_location')
            positions.append({'time':now,'x':p.x,'y':p.y,'z':p.z});last_sample=now
        if now<deadline:return
        if phase==0:
            level.editor_request_begin_play();phase=1;deadline=now+8
        elif phase==1:
            world=editor.get_game_world()
            if not world or not level.is_in_play_in_editor():return
            ui=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if a.get_class().get_name()=='CityVRToolController')
            board=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if a.get_class().get_name()=='CityBoardActor')
            paths[0].parent.mkdir(parents=True,exist_ok=True);paths[0].write_bytes(fixture.read_bytes())
            ui.show_main_menu();ui.review_city_load();ui.confirm_reviewed_city_load()
            effects={p.get_name():p for p in board.get_components_by_class(unreal.ParticleSystemComponent)}
            globals()['effects']=effects
            if ui.is_main_menu_environment_active():raise RuntimeError('Isolated disaster fixture failed to load')
            camera=unreal.GameplayStatics.get_player_pawn(world,0)
            target=board.get_actor_transform().transform_location(unreal.Vector(6.5*800,9.5*800,1800))
            origin=unreal.Vector(target.x-45,target.y-60,target.z+65)
            camera.set_actor_location(origin,False,True);camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(origin,target),False)
            view=camera.get_component_by_class(unreal.CameraComponent)
            view.set_editor_property('lock_to_hmd',False);view.set_editor_property('use_pawn_control_rotation',False)
            view.set_relative_location(unreal.Vector(0,0,0),False,True);view.set_relative_rotation(unreal.Rotator(0,0,0),False,True)
            view.set_field_of_view(60)
            unreal.GameplayStatics.get_player_controller(world,0).set_control_rotation(unreal.MathLibrary.find_look_at_rotation(origin,target))
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            for command in ('DisableAllScreenMessages','t.IdleWhenNotForeground 0','r.EyeAdaptationQuality 0'):
                unreal.SystemLibrary.execute_console_command(world,command)
            if not board.start_natural_disaster_test(False):raise RuntimeError('Earthquake test could not start')
            board.set_city_paused(False)
            globals()['board']=board
            phase=2;deadline=now+15
        elif phase==2:
            if not effects['EarthquakeDust'].is_active():raise RuntimeError('Quake emitter did not activate')
            manager=unreal.GameplayStatics.get_player_camera_manager(editor.get_game_world(),0)
            (root/'Artifacts/Preview/D51-OnsetCamera.json').write_text(json.dumps({'board':str(board.get_actor_location()),'scale':str(board.get_actor_scale3d()),'target':str(target),'camera':str(manager.get_camera_location()),'rotation':str(manager.get_camera_rotation())},indent=2),encoding='utf-8')
            road=next(c for c in ui.get_components_by_class(unreal.TextRenderComponent) if c.get_name()=='RoadStatusText')
            if road.get_editor_property('visible'):raise RuntimeError('Disaster HUD alert is still visible')
            capture(0);phase=3;deadline=now+2
        elif phase==3 and outputs[0].exists():
            board.set_city_paused(True)
            paths[0].write_bytes(fixture.read_bytes())
            ui.show_main_menu();ui.review_city_load();ui.confirm_reviewed_city_load()
            if not board.start_natural_disaster_test(True):raise RuntimeError('Tornado test could not start')
            board.set_city_paused(False);phase=4;deadline=now+16
        elif phase==4:
            if not effects['TornadoParticles'].is_active():raise RuntimeError('Tornado emitter did not activate')
            if effects['EarthquakeDust'].is_active():raise RuntimeError('Quake emitter leaked across city reload')
            road=next(c for c in ui.get_components_by_class(unreal.TextRenderComponent) if c.get_name()=='RoadStatusText')
            if road.get_editor_property('visible'):raise RuntimeError('Tornado HUD alert is still visible')
            capture(1);phase=5;deadline=now+2
        elif phase==5 and outputs[1].exists():
            restore()
            for path,data in prior.items():
                assert (path.read_bytes() if path.exists() else None)==data,f'Local save/catalog changed: {path}'
            report.write_text(json.dumps({'source':'native temporary PIE earthquake/tornado tests on isolated four-building baseline','savedMap':False,'localSaveAndCatalogPreserved':True,'quakeToTornadoReloadClearsPreviousEffect':True,'captures':[p.name for p in outputs],'tornadoFramePositions':positions,'disasterHUDHiddenForBothEffects':True,'continuousFrameMotionObserved':len(positions)>15 and max(p['x'] for p in positions)-min(p['x'] for p in positions)>.1},indent=2),encoding='utf-8')
            finish()
    except Exception:
        error.write_text(traceback.format_exc(),encoding='utf-8');finish()
handle=unreal.register_slate_post_tick_callback(tick)

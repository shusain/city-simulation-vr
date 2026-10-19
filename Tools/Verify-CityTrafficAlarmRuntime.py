"""PIE checks of actual idle/driving emitter motion and the earthquake audio cutoff."""
from pathlib import Path
import json
import time
import traceback
import unreal

root=Path(__file__).resolve().parents[1]
out=root/'Artifacts/D58/TrafficAlarmRefinement/EventPlayback'
out.mkdir(parents=True,exist_ok=True)
report=out/'Events.json'
assert not report.exists(), 'Preserve earlier reports'
save=root/'Artifacts/D58/TrafficAlarmRefinement/EventUser2/Saved/SaveGames/CitySaveV1.json'
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
camera=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.CameraActor,unreal.Vector(),transient=False)
camera.set_actor_label('D58TrafficAudioCamera')
start=time.monotonic();deadline=start+2;phase=0;samples=[];checks={};town=[];event=None

def playing():
    return [p for p in board.get_components_by_class(unreal.AudioComponent)
            if p.is_playing() and p.get_editor_property('sound')]

def city_data():
    # Saves migrate the isolated legacy fixture to the existing Cities library.
    candidates=[]
    for path in (save.parent/'Cities').glob('*.json'):
        data=json.loads(path.read_text())
        if 'schemaVersion' in data and 'cityId' in data:candidates.append(data)
    # Legacy migration assigns a fresh GUID; this isolated profile owns just one city.
    assert len(candidates)==1, 'Expected exactly one isolated saved city'
    return candidates[0]

def finish():
    level.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()

def tick(delta):
    global phase,deadline,world,board,simulation,traffic_start,quake_start,event
    try:
        now=time.monotonic()
        if now-start>180: raise RuntimeError('Traffic/alarm probe timed out')
        if now<deadline:return
        if phase==0:
            level.editor_request_begin_play();phase=1;deadline=now+7
        elif phase==1:
            world=editor.get_game_world()
            if not world:return
            actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            board=next(a for a in actors if a.get_class().get_name()=='CityBoardActor')
            ui=next(a for a in actors if a.get_class().get_name()=='CityVRToolController')
            cam=next(a for a in actors if a.get_actor_label()=='D58TrafficAudioCamera')
            unreal.SystemLibrary.execute_console_command(world,'t.IdleWhenNotForeground 0')
            unreal.SystemLibrary.execute_console_command(world,'au.DisableAppVolume 1')
            ui.review_city_load();ui.confirm_reviewed_city_load();ui.set_debug_mode_enabled(True)
            simulation=board.get_city_simulation();simulation.set_simulation_speed(1);board.set_city_paused(False)
            position=unreal.MathLibrary.transform_location(board.get_actor_transform(),unreal.Vector(2800,6800,8000))
            cam.set_actor_location_and_rotation(position,unreal.Rotator(-90,board.get_actor_rotation().yaw,0),False,True)
            unreal.GameplayStatics.get_player_controller(world,0).set_view_target_with_blend(cam)
            settings=unreal.get_default_object(unreal.CityAudioSettings)
            settings.set_ambience_volume(1.0);settings.set_alarm_volume(.75)
            traffic_start=now;phase=2;deadline=now+.1
        elif phase==2:
            engines=[p for p in playing() if p.get_editor_property('sound').get_name().startswith('SC_')]
            assert len(engines)<=3, 'Traffic transition pool exceeded'
            samples.append(dict(seconds=now-traffic_start,engines=[dict(name=p.get_editor_property('sound').get_name(),
                location=[p.get_world_location().x,p.get_world_location().y,p.get_world_location().z]) for p in engines]))
            if now-traffic_start<5.5:
                deadline=now+.1;return
            names={e['name'] for s in samples for e in s['engines']}
            assert 'SC_Truck_Idle' in names and 'SC_Truck_Driving' in names, names
            points=[e['location'] for s in samples for e in s['engines'] if e['name']=='SC_Truck_Driving']
            extent=max(sum((a-b)**2 for a,b in zip(p,points[0]))**.5 for p in points)
            assert extent>.01, ('Actual native vehicle emitter did not move',extent)
            assert not any(p.get_editor_property('sound').get_name() in ('MS_Car_Pass','MS_Truck_Pass') for p in playing())
            checks.update(actualIdleToDriving=True,nativeEmitterMoves=True,trafficTransitionCap=True,oldPassCuesAbsent=True)
            assert simulation.start_earthquake_test(), 'Controlled earthquake rejected'
            simulation.set_simulation_speed(5);assert simulation.save_city()
            event=city_data()['disasters']['events'][-1]
            quake_start=now;phase=3;deadline=now+.15
        elif phase==3:
            assert simulation.save_city()
            state=city_data()['disasters'];step=int(state['lastStep'])
            warning=int(event['warning']);end=int(event['end']);cutoff=warning+(end-warning+1)//2
            names=[p.get_editor_property('sound').get_name() for p in playing()]
            town.append(dict(seconds=now-quake_start,step=step,cutoff=cutoff,playing=names))
            if step<cutoff:
                assert any(n in names for n in ('MS_Town_Intro','MS_Town_Loop')), (step,names)
            if step>=cutoff+1:
                assert not any(n in names for n in ('MS_Town_Intro','MS_Town_Loop')), (step,names)
                assert step<end, 'Earthquake authority was shortened or probe missed cutoff'
                assert int(state['events'][-1]['end'])==end
                checks.update(earthquakeAlarmHalfWindow=True,earthquakeContinuesAfterAlarm=True,authoritativeEndUnchanged=True)
                report.write_text(json.dumps(dict(status='Pass',checks=checks,traffic=samples,town=town,event=event,
                    scope='Native Desktop PIE actual traffic motion and earthquake routing; PCVR/Quest listening and dense timing remain pending'),indent=2))
                finish();return
            deadline=now+.2
    except Exception:
        (out/'Events.error.txt').write_text(traceback.format_exc());unreal.log_error(traceback.format_exc());finish()

handle=unreal.register_slate_post_tick_callback(tick)

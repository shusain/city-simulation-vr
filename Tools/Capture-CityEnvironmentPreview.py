"""Capture isolated native terrain/weather fixtures without editing player saves/maps."""
from pathlib import Path
import time, traceback, json, unreal
root=Path(__file__).resolve().parents[1]
fixtures=root/'Artifacts/Preview'
rain_speed='-D62RainSpeedOnly' in unreal.SystemLibrary.get_command_line()
out=root/('Artifacts/D62-RainSpeed-Preview' if rain_speed else 'Artifacts/Preview'); out.mkdir(parents=True,exist_ok=True)
weather_only=rain_speed or '-D62WeatherOnly' in unreal.SystemLibrary.get_command_line()
views=['Rain','Snow'] if weather_only else ['Lowlands','Rain','Snow','Lakes','Mountains','River','Dry']; index=0; phase=0; start=time.monotonic(); deadline=start+5
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
report=[]
(out/'D62-Environment-PIE.error.txt').unlink(missing_ok=True)
def finish():
    unreal.unregister_slate_post_tick_callback(handle);level.editor_request_end_play();unreal.SystemLibrary.quit_editor()
def tick(delta):
    global phase,deadline,index,ui,board,sim
    try:
        now=time.monotonic()
        if now-start>140: raise RuntimeError('Environment capture timed out')
        if now<deadline:return
        if phase==0:
            level.editor_request_begin_play();phase,deadline=1,now+8
        elif phase==1:
            world=editor.get_game_world()
            if not world:return
            actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            ui=next(a for a in actors if a.get_class().get_name()=='CityVRToolController')
            board=next(a for a in actors if a.get_class().get_name()=='CityBoardActor')
            sim=next(obj for obj in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if obj.get_outer()==world)
            ui.return_to_city();ui.set_debug_mode_enabled(False);ui.set_city_summary_visible(False)
            for command in ('FOV 60','DisableAllScreenMessages','t.IdleWhenNotForeground 0','r.EyeAdaptationQuality 0'):
                unreal.SystemLibrary.execute_console_command(world,command)
            # Move the real desktop pawn to a readable oblique view of the full board.
            pawn=unreal.GameplayStatics.get_player_pawn(world,0);controller=unreal.GameplayStatics.get_player_controller(world,0)
            center=unreal.MathLibrary.transform_location(board.get_actor_transform(),unreal.Vector(25600,25600,0));location=center+unreal.Vector(0,-225,260)
            pawn.set_actor_location(location-unreal.Vector(0,0,pawn.get_editor_property('base_eye_height')),False,True)
            controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location,center))
            phase,deadline=2,now+1
        elif phase==2:
            path=fixtures/('D62-'+views[index]+'.json')
            # Unreal treats the native bool return as success/failure and exposes
            # the output error string to Python. A successful load returns "".
            error=sim.load_city_from_path(str(path),False)
            if error:raise RuntimeError(str(error))
            storm=views[index] in ('Rain','Snow')
            sim.set_simulation_paused(not storm)
            board.set_lighting_preset(unreal.CityLightingPreset.AUTOMATIC if storm else unreal.CityLightingPreset.DAY);board.set_tool_mode(unreal.CityToolMode.INSPECT)
            phase,deadline=3,now+(7 if storm else 4)
        elif phase==3:
            path=out/('D62-'+views[index]+'-PIE.png')
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),f'HighResShot 1600x1200 filename="{path.as_posix()}"')
            pools=[c for c in board.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name().startswith('CityAtmospherePool')]
            particles=board.get_components_by_class(unreal.CityWeatherParticleComponent)
            report.append({'view':views[index],'instances':{c.get_name():c.get_instance_count() for c in pools},
                           'native_particles':{c.get_name():{'live':c.get_num_active_particles(),'active':c.is_active(),'visible':c.is_visible()} for c in particles}})
            phase,deadline=4,now+3
        elif phase==4:
            if not (out/('D62-'+views[index]+'-PIE.png')).exists():raise RuntimeError('Missing screenshot')
            index+=1
            if index<len(views):phase,deadline=2,now+1
            else:
                (out/('D62-WeatherParticles-PIE.json' if weather_only else 'D62-Environment-PIE.json')).write_text(json.dumps(report,indent=2));finish()
    except Exception:
        (out/'D62-Environment-PIE.error.txt').write_text(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)

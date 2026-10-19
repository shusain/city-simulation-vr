"""D63 native translucent water siting review; restores local save bytes after PIE."""
from pathlib import Path
import time, traceback, json, unreal
root=Path(__file__).resolve().parents[1]
out=root/'Artifacts/Preview'; out.mkdir(parents=True,exist_ok=True)
views=['Day','Night','Cleared','LegacyCoast','NewCoast']; index=0; phase=0; start=time.monotonic(); deadline=start+5
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
report=[]
saved=root/'CitySimVR/Saved/SaveGames'
prior={p.relative_to(saved):p.read_bytes() for p in saved.rglob('*') if p.is_file()}
backup=out/('D63-Placement-LocalSaveBackup-'+str(time.time_ns()))
for relative,data in prior.items():
    path=backup/relative; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)
coast=json.loads((out/'D64-WarmCoast.json').read_text(encoding='utf-8-sig'))
coast['terrain']['waterRulesVersion']=1
(out/'D63-Placement-NewCoast.json').write_text(json.dumps(coast),encoding='utf-8')
map_path=root/'CitySimVR/Content/CitySim/Maps/CitySandbox.umap'
map_bytes=map_path.read_bytes()
report_path=out/'D63-Placement-PIE.json'
error_path=out/'D63-Placement-PIE.error.txt'
pending_error=None
finish_started=0
for path in [report_path,error_path]+[out/('D63-Placement-'+view+'-PIE.png') for view in views]:
    path.unlink(missing_ok=True)
def current_saves():
    return {p.relative_to(saved):p.read_bytes() for p in saved.rglob('*') if p.is_file()}

def restore_saves():
    # PIE may autosave its active slot during EndPlay. Restore only after PIE
    # has ended and its teardown writes have had time to settle.
    for path in saved.rglob('*'):
        if path.is_file() and path.relative_to(saved) not in prior:
            path.unlink()
    for relative,data in prior.items():
        path=saved/relative
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(data)
    assert current_saves()==prior, 'Local save restoration did not reproduce exact files/bytes'

def finish(error=None):
    global phase,deadline,pending_error,finish_started
    if phase>=90:
        if error:pending_error=(pending_error or '')+'\n'+error
        return
    pending_error=error
    finish_started=time.monotonic()
    phase,deadline=90,finish_started+3
    try:
        level.editor_request_end_play()
    except Exception:
        pending_error=(pending_error or '')+'\n'+traceback.format_exc()

def tick(delta):
    global phase,deadline,index,ui,board,sim,pending_error
    try:
        now=time.monotonic()
        if phase<90 and now-start>140: raise RuntimeError('Environment capture timed out')
        if now<deadline:return
        if phase==90:
            if level.is_in_play_in_editor():
                level.editor_request_end_play()
                deadline=now+1
                if now-finish_started>45:
                    pending_error=(pending_error or '')+'\nPIE teardown exceeded 45 seconds; awaiting safe save restoration.'
                return
            restore_saves()
            phase,deadline=91,now+2
        elif phase==91:
            # A second delayed equality check catches any teardown write that
            # raced the first restoration. Repair and settle again if needed.
            if current_saves()!=prior:
                restore_saves()
                deadline=now+2
                return
            map_unchanged=map_path.read_bytes()==map_bytes
            if not map_unchanged:
                pending_error=(pending_error or '')+'\nSandbox map bytes changed during the preview.'
            if pending_error:
                error_path.write_text(pending_error+'\nLocal save files/bytes restored; sandbox map unchanged='+str(map_unchanged)+'.\n',encoding='utf-8')
            else:
                report_path.write_text(json.dumps({'samples':report,'localSaveBytesRestored':True,'noMapSaved':True},indent=2)+'\n',encoding='utf-8')
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        elif phase==0:
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
            path=out/('D64-WarmCoast.json' if views[index]=='LegacyCoast' else 'D63-Placement-NewCoast.json' if views[index]=='NewCoast' else 'Water-Pressure.json')
            # Unreal treats the native bool return as success/failure and exposes
            # the output error string to Python. A successful load returns "".
            error=sim.load_city_from_path(str(path),False)
            if error:raise RuntimeError(str(error))
            sim.set_simulation_paused(True)
            board.set_lighting_preset(unreal.CityLightingPreset.NIGHT if views[index]=='Night' else unreal.CityLightingPreset.DAY)
            if views[index]=='Cleared':
                board.set_tool_mode(unreal.CityToolMode.INSPECT)
            else:
                board.set_selected_facility('WaterTower' if views[index]=='Night' else 'Service')
            phase,deadline=3,now+4
        elif phase==3:
            view=views[index]
            mesh=next(c for c in board.get_components_by_class(unreal.ProceduralMeshComponent) if c.get_name()=='WaterPlacementSurface')
            layers={c.get_name():c.is_visible() for c in board.get_components_by_class(unreal.PrimitiveComponent) if c.get_name() in ['RoadSurfaceProfile','SidewalkSurface','BoardSurface','WaterSurface','HeatMapSurface']}
            texts={c.get_name():str(c.get_editor_property('text')) for c in ui.get_components_by_class(unreal.TextRenderComponent) if c.is_visible()}
            if view=='Cleared':
                assert not mesh.is_visible() and mesh.get_num_sections()==0, 'Leaving water tool must clear geometry'
            else:
                expected_sections=1 if view in ['LegacyCoast','NewCoast'] else 2
                assert mesh.is_visible() and mesh.get_num_sections()==expected_sections, 'Water siting must show terrain and any existing road sections'
                assert all(layers.get(n,False) for n in ['RoadSurfaceProfile','SidewalkSurface','BoardSurface','WaterSurface']), 'City layers must remain visible'
                assert 'groundwater yield' in texts.get('HeatMapLegend','') and 'pipe pressure' in texts.get('HeatMapLegend',''), 'Actual UI legend must distinguish fields'
            report.append({'view':view,'overlayVisible':mesh.is_visible(),'sections':mesh.get_num_sections(),'cityLayers':layers,'texts':texts})
            path=out/('D63-Placement-'+view+'-PIE.png')
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),f'HighResShot 1800x1400 filename="{path.as_posix()}"')
            phase,deadline=4,now+3
        elif phase==4:
            if not (out/('D63-Placement-'+views[index]+'-PIE.png')).exists():raise RuntimeError('Missing screenshot')
            index+=1
            if index<len(views):phase,deadline=2,now+1
            else:
                finish()
    except Exception:
        failure=traceback.format_exc()
        if phase>=90:
            pending_error=(pending_error or '')+'\n'+failure
            error_path.write_text(pending_error,encoding='utf-8')
            # Keep the callback alive to retry restoration. Do not quit while
            # player saves differ from the snapshot.
            phase,deadline=90,time.monotonic()+2
        else:
            finish(failure)
handle=unreal.register_slate_post_tick_callback(tick)

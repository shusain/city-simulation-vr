"""D79 real board pools in isolated PIE saves; no map or player-save writes."""
from pathlib import Path
import copy,json,sys,time,traceback,uuid,os
root=Path(__file__).resolve().parents[1]
out=root/os.environ.get('CITYSIM_D79_PREVIEW_OUTPUT','Artifacts/D79/Integration/NativePreview')
assert out.resolve().is_relative_to((root/'Artifacts').resolve())
out.mkdir(parents=True,exist_ok=True)
businesses=('market','bakery','cafe','books','pharmacy','repairs','outfitters','tech')
def business(id):
    mask=(1<<64)-1;k=(id+0x9e3779b97f4a7c15)&mask
    k=((k^(k>>30))*0xbf58476d1ce4e5b9)&mask
    k=((k^(k>>27))*0x94d049bb133111eb)&mask
    return businesses[(k^(k>>31))%8]
def fixtures():
    source=json.loads((root/'Artifacts/Preview/D66-Refinement-Autumn-City.json').read_text(encoding='utf-8-sig'))
    buildings=[];states=[];used=set()
    template=source['development']['states'][0]
    for density in range(3):
        for column,definition in enumerate(('Home','Shop','Factory','Power','Service')+('Shop',)*8):
            id=1000+density*20+column
            if column>=5:
                id=next(i for i in range(1,1000) if i not in used and business(i)==businesses[column-5])
            used.add(id)
            buildings.append(dict(entityId=str(id),definitionId=definition,x=4+column*4,y=9+density*10,quarterTurns=0,width=2,height=2,recordedCost='0'))
            state=copy.deepcopy(template);state.update(entityId=str(id),stage=1,density=density);states.append(state)
    source.update(buildings=buildings,roads=[dict(x=x,y=y,level=0,definitionId='LocalRoad') for y in (8,18,28) for x in range(64)]+[dict(x=0,y=y,level=0,definitionId='LocalRoad') for y in range(8,29) if y not in (8,18,28)],zones=[0]*4096,roadStructures=[],nextEntityId='10000',funds='1000000',cityName='D79 asset gallery',cityId=uuid.uuid5(uuid.NAMESPACE_URL,'citysim-D79-art').hex)
    source['development']['states']=states;source['economy']['paused']=True
    source['fire'].update(activationStep='0',graceEndStep='0')
    # This gallery has its own lots; do not retain the seed fixture's citizens,
    # public employers, vehicles or patient/waste references to previous lots.
    for section in ('population','traffic','health','waste','crime','disasters'):
        for key,value in source[section].items():
            if isinstance(value,list):source[section][key]=[]
    for damaged in (False,True):
        doc=copy.deepcopy(source)
        # Tornado structural damage permits the complete gallery; natural fire
        # intentionally caps damaged lots at eight.
        doc['fire']['damage']=[dict(entityId=e['entityId'],damage=50,cause=2,repairCompleteStep='-1') for e in buildings] if damaged else []
        (out/('Damaged.json' if damaged else 'Normal.json')).write_text(json.dumps(doc,indent=2)+'\n')
fixtures()
if '--fixtures-only' in sys.argv:
    print('CITYSIM_D79_NATIVE_FIXTURES_READY');raise SystemExit(0)
import unreal
(out/'Report.json').unlink(missing_ok=True);(out/'Error.txt').unlink(missing_ok=True)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
views=[(str(d)+'-'+lighting+'-'+group,d,lighting,group,False) for d in range(3) for lighting in ('Day','Night') for group in ('Buildings','Signs')]
views += [('Damaged-'+str(d),d,'Day','Buildings',True) for d in range(3)]
if '-CityMechanicalReviewOnly' in unreal.SystemLibrary.get_command_line():
    views=[v for v in views if v[3]=='Buildings']
if '-CityReflectionCompare' in unreal.SystemLibrary.get_command_line():
    views=[v for v in views if v[0]=='2-Day-Buildings']
phase=0;index=0;start=time.monotonic();deadline=start+4;report=[];failure=None
map_path=root/'CitySimVR/Content/CitySim/Maps/CitySandbox.umap';map_bytes=map_path.read_bytes()
def finish(error=None):
    global phase,deadline,failure
    failure=error;level.editor_request_end_play();phase=90;deadline=time.monotonic()+3
def tick(delta):
    global phase,deadline,index,board,sim,sample
    try:
        now=time.monotonic()
        if now<deadline:return
        if phase!=90 and now-start>360:raise RuntimeError('D79 native preview timed out')
        if phase==90:
            if level.is_in_play_in_editor():level.editor_request_end_play();deadline=now+1;return
            assert map_path.read_bytes()==map_bytes,'Sandbox map changed'
            (out/'Report.json').write_text(json.dumps(dict(samples=report,error=failure,map_unchanged=True),indent=2)+'\n')
            unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
        elif phase==0:
            level.editor_request_begin_play();phase=1;deadline=now+8
        elif phase==1:
            world=editor.get_game_world()
            if not world:return
            actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            board=next(a for a in actors if a.get_class().get_name()=='CityBoardActor')
            ui=next(a for a in actors if a.get_class().get_name()=='CityVRToolController')
            sim=next(o for o in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if o.get_outer()==world)
            ui.return_to_city();ui.set_debug_mode_enabled(False);ui.set_actor_hidden_in_game(True)
            for command in ('FOV 45','DisableAllScreenMessages','t.IdleWhenNotForeground 0'):
                unreal.SystemLibrary.execute_console_command(world,command)
            if '-CityReflectionNoLightmapMix' in unreal.SystemLibrary.get_command_line():
                unreal.SystemLibrary.execute_console_command(world,'r.ReflectionEnvironmentLightmapMixing 0')
            phase=2;deadline=now+1
        elif phase==2:
            name,density,lighting,group,damaged=views[index]
            error=sim.load_city_from_path(str(out/('Damaged.json' if damaged else 'Normal.json')),False)
            if error is None or error:raise RuntimeError('Fixture load failed: '+str(error))
            sim.set_simulation_paused(True);board.set_tool_mode(unreal.CityToolMode.INSPECT)
            board.set_diagnostic_overlay(unreal.CityDiagnosticOverlay.NONE)
            board.set_lighting_preset(unreal.CityLightingPreset.NIGHT if lighting=='Night' else unreal.CityLightingPreset.DAY)
            phase=3;deadline=now+3
        elif phase==3:
            name,density,lighting,group,damaged=views[index]
            world=editor.get_game_world()
            center=unreal.MathLibrary.transform_location(board.get_actor_transform(),unreal.Vector((13 if group=='Buildings' else 39)*800,(10+density*10)*800,900))
            location=center+unreal.Vector(8,-(75 if group=='Buildings' else 105),50)
            pawn=unreal.GameplayStatics.get_player_pawn(world,0);controller=unreal.GameplayStatics.get_player_controller(world,0)
            pawn.set_actor_location(location-unreal.Vector(0,0,pawn.get_editor_property('base_eye_height')),False,True)
            controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location,center))
            pools=[c for c in board.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_instance_count() and c.get_editor_property('static_mesh')]
            active=[dict(name=c.get_name(),mesh=c.get_editor_property('static_mesh').get_path_name(),instances=c.get_instance_count(),materials=[c.get_material(i).get_path_name() for i in range(c.get_num_materials())]) for c in pools]
            assert all('/Prototypes/' not in c['mesh'] for c in active)
            assert any('Residential_SeasonalSpace' in c['mesh'] for c in active) or damaged
            assert len({c['mesh'].split('_Store')[1].split('_Damaged')[0].split('.')[0] for c in active if '_Store' in c['mesh']})==8
            sample=dict(view=name,lighting=lighting,density=density,damaged=damaged,activePools=active,
                reflection_lightmap_mixing=unreal.SystemLibrary.get_console_variable_int_value('r.ReflectionEnvironmentLightmapMixing'))
            phase=4;deadline=now+2
        elif phase==4:
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),f'HighResShot 1800x1200 filename="{(out/(views[index][0]+".png")).as_posix()}"')
            phase=5;deadline=now+2
        elif phase==5:
            assert (out/(views[index][0]+'.png')).is_file()
            report.append(sample);index+=1
            if index==len(views):finish()
            else:phase=2;deadline=now+.5
    except Exception:
        if phase==90:
            (out/'Error.txt').write_text(traceback.format_exc());unreal.SystemLibrary.quit_editor()
        else:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)

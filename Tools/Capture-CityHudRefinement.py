"""Native D59 HUD/cost review through shared commands; restores saves after PIE teardown."""
from pathlib import Path
import json, time, traceback, unreal
root = Path(__file__).resolve().parents[1]
out = root / 'Artifacts/Preview'; out.mkdir(parents=True, exist_ok=True)
import os
capture_prefix = os.environ.get('CITYSIM_HUD_CAPTURE_PREFIX', 'D59-Refinement')
views = ['OverviewDay', 'OverviewNight', 'BuildingCost', 'BuildingNoFunds', 'RoadCost', 'RoadNoFunds', 'RoadCancel', 'MenuCleanup']
if os.environ.get('CITYSIM_HUD_OBJECTIVE_ICON') == '1': views.insert(2,'OverviewCollapsed')
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
saved = root / 'CitySimVR/Saved/SaveGames'
prior = {p.relative_to(saved): p.read_bytes() for p in saved.rglob('*') if p.is_file()}
backup = out / ('D59-Refinement-LocalSaveBackup-' + str(time.time_ns()))
for relative, data in prior.items():
    path = backup / relative; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)
map_path = root / 'CitySimVR/Content/CitySim/Maps/CitySandbox.umap'; map_bytes = map_path.read_bytes()
report_path = out / (capture_prefix+'-PIE.json'); error_path = out / (capture_prefix+'-PIE.error.txt')
start=time.monotonic(); deadline=start+5; phase=0; index=0; samples=[]; pending_error=None; finish_started=0
text_rendering = {}
for path in [report_path, error_path]+[out / (capture_prefix+'-'+v+'-PIE.png') for v in views]: path.unlink(missing_ok=True)
fixture = json.loads((out/'Water-Pressure.json').read_text(encoding='utf-8-sig'))
for name, funds in [('Funded','100000'),('NoFunds','0')]:
    fixture['funds']=funds
    (out/('D59-Refinement-'+name+'.json')).write_text(json.dumps(fixture),encoding='utf-8')

def current_saves():
    return {p.relative_to(saved):p.read_bytes() for p in saved.rglob('*') if p.is_file()}

def component_bottom(component):
    # These 100 cm cube panels have upright HUD roots on the same camera.
    # World AABBs mix the panel depth into Z when the player looks down.
    return (component.get_attach_parent().get_editor_property('relative_location').z
            + component.get_editor_property('relative_location').z
            - component.get_editor_property('relative_scale3d').z * 50)

def restore_saves():
    for p in saved.rglob('*'):
        if p.is_file() and p.relative_to(saved) not in prior: p.unlink()
    for relative,data in prior.items():
        p=saved/relative; p.parent.mkdir(parents=True,exist_ok=True); p.write_bytes(data)
    assert current_saves()==prior, 'Save restoration differs'

def finish(error=None):
    global phase,deadline,pending_error,finish_started
    pending_error=error;finish_started=time.monotonic();phase=90;deadline=finish_started+3
    level.editor_request_end_play()

def aim(cell=None):
    world=editor.get_game_world();controller=unreal.GameplayStatics.get_player_controller(world,0)
    pawn=unreal.GameplayStatics.get_player_pawn(world,0)
    center=unreal.MathLibrary.transform_location(board.get_actor_transform(),unreal.Vector(25600,25600,0))
    location=center+unreal.Vector(0,-225,260)
    target=center if cell is None else unreal.MathLibrary.transform_location(board.get_actor_transform(),unreal.Vector((cell[0]+.5)*800,(cell[1]+.5)*800,0))
    pawn.set_actor_location(location-unreal.Vector(0,0,pawn.get_editor_property('base_eye_height')),False,True)
    controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location,target))
    width,height=controller.get_viewport_size()
    controller.set_mouse_location(width//2,height//2)

def load(path):
    error=sim.load_city_from_path(str(path),False)
    if error:raise RuntimeError(str(error))
    sim.set_simulation_paused(True)

def tick(delta):
    global phase,deadline,index,ui,board,sim,pending_error
    try:
        now=time.monotonic()
        if now<deadline:return
        if phase<90 and now-start>180:raise RuntimeError('HUD preview timeout')
        if phase==90:
            if level.is_in_play_in_editor():
                level.editor_request_end_play();deadline=now+1;return
            restore_saves();phase=91;deadline=now+2;return
        if phase==91:
            if current_saves()!=prior:restore_saves();deadline=now+2;return
            assert map_path.read_bytes()==map_bytes,'Sandbox map changed'
            if pending_error:error_path.write_text(pending_error,encoding='utf-8')
            else:report_path.write_text(json.dumps({'samples':samples,'textRendering':text_rendering,'localSaveBytesRestored':True,'noMapSaved':True},indent=2),encoding='utf-8')
            unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor();return
        if phase==0:
            level.editor_request_begin_play();phase=1;deadline=now+8;return
        if phase==1:
            world=editor.get_game_world()
            if not world:return
            actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            ui=next(a for a in actors if a.get_class().get_name()=='CityVRToolController')
            board=next(a for a in actors if a.get_class().get_name()=='CityBoardActor')
            sim=next(o for o in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if o.get_outer()==world)
            material = unreal.load_asset('/Game/CitySim/UI/M_CityTextUnlit')
            text_rendering['mobileAlphaCoverage'] = bool(material.get_editor_property('use_alpha_to_coverage'))
            text_rendering['msaaSamples'] = unreal.SystemLibrary.get_console_variable_int_value('r.MSAACount')
            assert text_rendering['mobileAlphaCoverage'], 'Font material lacks mobile edge coverage'
            assert text_rendering['msaaSamples'] == 4, 'Expected existing four-sample MSAA baseline'
            ui.return_to_city();ui.set_debug_mode_enabled(False)
            for cmd in ['FOV 90','DisableAllScreenMessages','t.IdleWhenNotForeground 0','r.EyeAdaptationQuality 0']:
                unreal.SystemLibrary.execute_console_command(world,cmd)
            phase=2;deadline=now+1;return
        if phase==2:
            view=views[index];ui.return_to_city();ui.set_city_summary_visible(view.startswith('Overview'))
            ui.set_objectives_visible(view != 'OverviewCollapsed')
            board.set_lighting_preset(unreal.CityLightingPreset.NIGHT if view in ['OverviewNight','RoadNoFunds'] else unreal.CityLightingPreset.DAY)
            if view.startswith('Overview'):
                learning=max((root/'CitySimVR/Saved/Automation/CivicBudget-Civic').rglob('learning.json'),key=lambda p:p.stat().st_mtime)
                load(learning);board.set_tool_mode(unreal.CityToolMode.INSPECT);aim()
            elif view.startswith('Building'):
                load(out/('D59-Refinement-'+('NoFunds' if 'NoFunds' in view else 'Funded')+'.json'))
                board.set_selected_facility('Power');aim((20,4))
            elif view.startswith('Road') and view!='RoadCancel':
                load(out/('D59-Refinement-'+('NoFunds' if 'NoFunds' in view else 'Funded')+'.json'))
                board.set_tool_mode(unreal.CityToolMode.ROAD);aim((20,8));phase=3;deadline=now+1;return
            elif view=='RoadCancel':board.request_cancel()
            else:ui.show_main_menu()
            board.set_lighting_preset(unreal.CityLightingPreset.NIGHT if view in ['OverviewNight','RoadNoFunds'] else unreal.CityLightingPreset.DAY)
            phase=5;deadline=now+3;return
        if phase==3:
            board.request_confirm();phase=4;deadline=now+1;return
        if phase==4:
            aim((20,4));phase=5;deadline=now+3;return
        if phase==5:
            view=views[index]
            components={c.get_name():c for c in ui.get_components_by_class(unreal.SceneComponent)}
            texts={c.get_name():str(c.get_editor_property('text')) for c in ui.get_components_by_class(unreal.TextRenderComponent) if c.is_visible() and not c.get_editor_property('hidden_in_game')}
            anchored=components['ConstructionLeader'].is_visible()
            if view.startswith('Overview'):
                assert 'Month net now' in texts.get('CityTrendText0',''),'Missing realized net label'
                assert all('CityInfoTitle'+str(i) in texts for i in range(6)),'Missing grouped cards'
                assert 'Full monthly bills' in texts.get('CityBudgetDetail',''),'Missing bills breakdown'
                assert texts.get('GrowthNeedsTitle') == 'NEEDS FOR GROWTH', 'Growth card heading missing'
                assert components['GrowthNeedsBackground'].is_visible(), 'Growth card background missing'
                assert not texts.get('NeedsText','').startswith('GROWTH LIMITS'), 'Duplicate growth heading'
                text_rendering['calendarDepthGapCm'] = components['CalendarText'].get_editor_property('relative_location').x - (components['CityTotalsBackground'].get_editor_property('relative_location').x + components['CityTotalsBackground'].get_editor_property('relative_scale3d').x * 50)
                assert text_rendering['calendarDepthGapCm'] > .4, 'Calendar shares panel depth'
                if os.environ.get('CITYSIM_HUD_OBJECTIVE_ICON') == '1':
                    assert components['ObjectiveToggleIcon'].is_visible(), 'Checklist icon missing'
                    assert ('ObjectiveHeader' in texts) == (view != 'OverviewCollapsed'), 'Objective title leaked or missing'
                    text_rendering['objectiveIconBottomZ'] = component_bottom(components['ObjectiveToggle'])
                    text_rendering['cityIconBottomZ'] = component_bottom(components['NeedsToggleBackground'])
                    text_rendering['topPanelBottomZ'] = component_bottom(components['CityTotalsBackground'])
                    assert abs(text_rendering['objectiveIconBottomZ']-text_rendering['topPanelBottomZ']) < .01, 'Objectives button misaligned'
                    assert abs(text_rendering['cityIconBottomZ']-text_rendering['topPanelBottomZ']) < .01, 'City-info button misaligned'
            if view.startswith('Building') or view in ['RoadCost','RoadNoFunds']:
                assert anchored,'Placement pointer missing'
                assert '$' in texts.get('RoadStatusText',''),'Cost missing'
                if 'NoFunds' in view:assert 'insufficient funds' in texts['RoadStatusText'].lower(),'Price failure missing: '+texts['RoadStatusText']
            if view in ['RoadCancel','MenuCleanup']:assert not anchored,'Stale placement pointer'
            if view == 'MenuCleanup':
                assert not components['ObjectiveToggleIcon'].is_visible(), 'Objectives icon leaked into the menu'
            if 'NoFunds' in view:
                color=components['RoadStatusText'].get_editor_property('text_render_color')
                assert (color.r,color.g,color.b)==(255,100,80),'Unaffordable callout is not red'
            samples.append({'view':view,'texts':texts,'placementLeaderVisible':anchored})
            path=out/(capture_prefix+'-'+view+'-PIE.png')
            phase=6;deadline=now+3
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),f'HighResShot 1800x1400 filename="{path.as_posix()}"')
            return
        if phase==6:
            assert (out/(capture_prefix+'-'+views[index]+'-PIE.png')).exists(),'Missing screenshot'
            index+=1
            if index==len(views):finish()
            else:phase=2;deadline=now+1
    except Exception:
        failure=traceback.format_exc()
        if phase>=90:
            pending_error=(pending_error or '')+'\n'+failure;phase=90;deadline=time.monotonic()+2
            error_path.write_text(pending_error,encoding='utf-8')
        else:finish(failure)
handle=unreal.register_slate_post_tick_callback(tick)

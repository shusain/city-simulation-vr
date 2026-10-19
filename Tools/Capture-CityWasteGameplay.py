"""Review real waste Inspect in temporary PIE; preserve local save bytes."""
from pathlib import Path
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
out = root/'Artifacts/Preview'
saved = root/'CitySimVR/Saved/SaveGames'
prior = {p:p.read_bytes() for p in saved.rglob('*') if p.is_file()}
views = [('Starter','Waste-Starter.json',unreal.CityLightingPreset.DAY,'WasteDepot'),
         ('Recycling','Waste-Starter.json',unreal.CityLightingPreset.DAY,'Recycling'),
         ('Upgrading','Waste-Upgrading.json',unreal.CityLightingPreset.DAY,'WasteDepot'),
         ('Expanded','Waste-Expanded.json',unreal.CityLightingPreset.NIGHT,'WasteDepot'),
         ('Damaged','Waste-Damaged.json',unreal.CityLightingPreset.DAY,'WasteDepot'),
         ('Overlay','Waste-Starter.json',unreal.CityLightingPreset.DAY,'Home')]
outputs = [out/('D61-Waste-Inspect-'+name+'.png') for name,_,_,_ in views]
report = out/'D61-Waste-Gameplay.json'
error = report.with_suffix('.error.txt')
for path in outputs+[report,error]:
    path.unlink(missing_ok=True)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start = time.monotonic()
deadline = start+5
phase = 0
index = 0
inspection_id = 0
samples = []


def finish():
    level.editor_request_end_play()
    for path in saved.rglob('*'):
        if path.is_file() and path not in prior:
            path.unlink()
    for path,data in prior.items():
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(data)
    assert {p:p.read_bytes() for p in saved.rglob('*') if p.is_file()} == prior
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    global phase,deadline,index,inspection_id
    try:
        now = time.monotonic()
        if now-start>180:
            raise RuntimeError('Waste preview timed out')
        if now<deadline:
            return
        if phase==0:
            level.editor_request_begin_play()
            phase,deadline = 1,now+8
        elif phase==1:
            world = editor.get_game_world()
            if not world or not level.is_in_play_in_editor():
                return
            global ui,board,sim
            actors = unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
            ui = next(a for a in actors if a.get_class().get_name()=='CityVRToolController')
            board = next(a for a in actors if a.get_class().get_name()=='CityBoardActor')
            sim = next(obj for obj in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if obj.get_outer()==world)
            # These isolated automation fixtures use a faster four-step month.
            # Match their calendar before native validation; never rewrite clocks.
            tuning = sim.get_editor_property('economy_tuning')
            tuning.set_editor_property('fixed_step_seconds',.125)
            tuning.set_editor_property('fixed_steps_per_month',4)
            sim.set_editor_property('economy_tuning',tuning)
            for command in ('FOV 90','DisableAllScreenMessages','t.IdleWhenNotForeground 0','r.EyeAdaptationQuality 0'):
                unreal.SystemLibrary.execute_console_command(world,command)
            phase,deadline = 2,now+1
        elif phase==2:
            name,fixture,preset,definition = views[index]
            source = out/fixture
            document = json.loads(source.read_text(encoding='utf-8-sig'))
            # Use the saved stable ID; never infer identity from a rendered mesh.
            waste = next(e for e in document['buildings'] if e['definitionId']==definition)
            saved.mkdir(parents=True,exist_ok=True)
            (saved/'CitySaveV1.json').write_bytes(source.read_bytes())
            ui.return_to_city()
            if not sim.load_city():
                raise RuntimeError('Native waste fixture did not load: '+fixture)
            sim.set_simulation_paused(True)
            board.set_lighting_preset(preset)
            board.set_tool_mode(unreal.CityToolMode.INSPECT)
            board.set_diagnostic_overlay(unreal.CityDiagnosticOverlay.WASTE if name=='Overlay' else unreal.CityDiagnosticOverlay.NONE)
            ui.set_debug_mode_enabled(False)
            inspection_id = int(waste['entityId'])
            # Let the ordinary generation-change cleanup finish before selecting.
            phase,deadline = 5,now+3
        elif phase==5:
            ui.inspect_building(inspection_id)
            phase,deadline = 3,now+3
        elif phase==3:
            texts = {c.get_name():str(c.get_editor_property('text')) for c in ui.get_components_by_class(unreal.TextRenderComponent) if c.is_visible() and not c.get_editor_property('hidden_in_game')}
            inspector = texts.get('InspectorText','')
            (out/'D61-Waste-Gameplay-texts.json').write_text(json.dumps(texts,indent=2),encoding='utf-8')
            flat = inspector.replace('<br>',' ').replace('\n',' ')
            if views[index][0]!='Overlay' and ('Waste storage' not in flat or 'largest piles' not in flat):
                raise RuntimeError('Missing waste inspection details: '+flat)
            if index==0 and '$1200' not in texts.get('InspectorUpgradeLabel',''):
                raise RuntimeError('Missing priced waste depot upgrade')
            if index==1 and '$1800' not in texts.get('InspectorUpgradeLabel',''):
                raise RuntimeError('Missing priced recycling upgrade')
            if index==3 and texts.get('InspectorUpgradeLabel','')!='FULLY UPGRADED':
                raise RuntimeError('Expanded depot offers an unsupported third stage')
            samples.append({'view':views[index][0],'texts':texts})
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(),f'HighResShot 1800x1400 filename="{outputs[index].as_posix()}"')
            phase,deadline = 4,now+3
        elif phase==4:
            if not outputs[index].exists():
                raise RuntimeError('Missing waste screenshot')
            index += 1
            if index<len(views):
                phase,deadline = 2,now+1
            else:
                report.write_text(json.dumps({'source':'Actual native waste fixtures and Inspect in desktop PIE','samples':samples,'localSaveCatalogPreserved':True,'savedMap':False},indent=2),encoding='utf-8')
                finish()
    except Exception:
        error.write_text(traceback.format_exc(),encoding='utf-8')
        finish()


handle = unreal.register_slate_post_tick_callback(tick)

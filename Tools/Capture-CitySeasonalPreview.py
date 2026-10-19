"""D66 native city/park review, with exact local save restoration after PIE.

UnrealEditor -NoHMD -ExecCmds="py <absolute script path>".
Use ordinary Python with --fixtures-only to prepare isolated schema58 JSON;
never edits player saves or the sandbox map during fixture generation.
"""
from pathlib import Path
import copy
import json
import sys
import time
import traceback
import uuid

root = Path(__file__).resolve().parents[1]
out = root / 'Artifacts/Preview'
out.mkdir(parents=True, exist_ok=True)


def climate_seed(warmth):
    def hash_field(seed):
        value = seed ^ 0x4c11ab1e
        value ^= value >> 13
        value = (value * 0x85ebca6b) & 0xffffffff
        value ^= value >> 16
        return 30 + (value % 3) * 25
    return next(seed for seed in range(1, 100) if hash_field(seed) == warmth)


def prepare_fixtures():
    source = json.loads((out / 'D47-Sports-City.json').read_text(encoding='utf-8-sig'))
    assert source['schemaVersion'] == 58, 'Use the current migrated schema58 park fixture'
    source['terrain'].update(broadElevationAmplitudeCm=0, hillAmplitudeCm=0,
                             ridgeAmplitudeCm=0, waterRulesVersion=0, sceneryVersion=1)
    # Spare lots front the existing secondary road at Y24. No new road IDs,
    # trip records or money transfers are invented by presentation fixtures.
    for x in [1, 4, 7, 15, 18, 40, 43, 46, 49, 52, 55, 58]:
        entity_id = str(int(source['nextEntityId']))
        source['nextEntityId'] = str(int(entity_id) + 1)
        source['buildings'].append(dict(entityId=entity_id, definitionId='Home', x=x, y=25,
                                        quarterTurns=0, width=2, height=2, recordedCost='200'))
        source['development']['states'].append(dict(entityId=entity_id, stage=1, density=0,
            constructionCompleteStep='0', operationalSinceStep='0', pendingCivicLevel=-1,
            civicUpgradeCompleteStep='0', stationCompleteHalfDay='-1'))
    source['revision'] = str(int(source['revision']) + 12)
    source['disasterFrequency'] = 0
    variants = [('Spring', 2, 0, 1, False, 30), ('Summer', 5, 1, 2, False, 30),
                ('Autumn', 9, 2, 1, False, 30), ('Winter', 10, 3, 0, True, 30),
                ('December', 12, 3, 0, True, 30), ('WarmDecember', 12, 3, 1, False, 80)]
    paths = {}
    for label, month, season, temperature, precipitation, warmth in variants:
        document = copy.deepcopy(source)
        step = (month - 1) * 25 + 1
        document['cityId'] = uuid.uuid5(uuid.NAMESPACE_URL, 'citysim-d66-native-' + label).hex
        document['cityName'] = 'D66 ' + label + ' preview'
        document['terrain']['seed'] = climate_seed(warmth)
        document['economy'].update(fixedStepId=str(step), monthId=str(month - 1),
            lastBilledMonthId=str(month - 1), accumulatorSeconds=0, paused=True, speed=1)
        document['environment'].update(lastFixedStepId=str(step), nextWeatherChangeStep=str(month * 25),
            season=season, temperature=temperature, precipitation=precipitation)
        document['development']['lastFixedStepId'] = str(step)
        document['fire']['lastFixedStepId'] = str(step)
        document['crime']['lastFixedStepId'] = str(step)
        document['disasters'].update(lastStep=str(step), nextCheck=str(step + 49))
        document['health']['step'] = str(step)
        document['waste']['step'] = str(step)
        # Traffic, safety and economy clocks must agree for a valid restore.
        document['traffic']['fixedStepId'] = str(step)
        path = out / ('D66-Refinement-' + label + '-City.json')
        path.write_text(json.dumps(document, indent=2) + '\n', encoding='utf-8')
        paths[label] = path
    return paths


fixtures = prepare_fixtures()
if '--fixtures-only' in sys.argv:
    print('CITYSIM_D66_FIXTURES_READY', ', '.join(str(p) for p in fixtures.values()))
    raise SystemExit(0)

import unreal

views = [('Spring', 'Spring', 'Day', 'Board'), ('Summer', 'Summer', 'Day', 'Board'),
         ('Autumn', 'Autumn', 'Day', 'Board'), ('Winter', 'Winter', 'Day', 'Board'),
         ('December', 'December', 'Day', 'Board'), ('DecemberNight', 'December', 'Night', 'Board'),
         ('SpringParks', 'Spring', 'Day', 'Parks'), ('AutumnParks', 'Autumn', 'Day', 'Parks'),
         ('DecemberParks', 'December', 'Day', 'Parks'), ('DecemberPocket', 'December', 'Day', 'Pocket'),
         ('WarmDecember', 'WarmDecember', 'Day', 'Parks'), ('Diagnostic', 'December', 'Day', 'Diagnostic'),
         ('Menu', None, 'Day', 'Menu'), ('ReloadDecember', 'December', 'Day', 'Board'),
         ('ReloadSpring', 'Spring', 'Day', 'Board')]
final_tree_review = '-SeasonalFinalTreeReview' in unreal.SystemLibrary.get_command_line()
if final_tree_review:
    views = [('FinalTreeParks', 'December', 'Day', 'Parks'),
             ('FinalTreePocket', 'December', 'Day', 'Pocket'),
             ('FinalTreeNight', 'December', 'Night', 'Parks'),
             ('FinalTreeWarm', 'WarmDecember', 'Day', 'Parks'),
             ('FinalTreeSpring', 'Spring', 'Day', 'Parks')]
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start = time.monotonic()
deadline = start + 5
phase = 0
index = 0
report = []
last_attempt = None
prior_signatures = {}
saved = root / 'CitySimVR/Saved/SaveGames'
prior = {p.relative_to(saved): p.read_bytes() for p in saved.rglob('*') if p.is_file()}
backup = out / ('D66-Refinement-LocalSaveBackup-' + str(time.time_ns()))
for relative, data in prior.items():
    path = backup / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
map_path = root / 'CitySimVR/Content/CitySim/Maps/CitySandbox.umap'
map_bytes = map_path.read_bytes()
report_path = out / ('D66-Refinement-FinalTree-PIE.json' if final_tree_review else 'D66-Refinement-Seasonal-PIE.json')
error_path = report_path.with_suffix('.error.txt')
pending_error = None
finish_started = 0
for path in [report_path, error_path] + [out / ('D66-Refinement-' + v[0] + '-PIE.png') for v in views]:
    path.unlink(missing_ok=True)


def current_saves():
    return {p.relative_to(saved): p.read_bytes() for p in saved.rglob('*') if p.is_file()}


def restore_saves():
    # EndPlay can write the active slot. Wait for completed PIE teardown first.
    for path in saved.rglob('*'):
        if path.is_file() and path.relative_to(saved) not in prior:
            path.unlink()
    for relative, data in prior.items():
        path = saved / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    assert current_saves() == prior, 'Local saves did not restore exact files/bytes'


def finish(error=None):
    global phase, deadline, pending_error, finish_started
    if phase >= 90:
        if error:
            pending_error = (pending_error or '') + '\n' + error
        return
    pending_error = error
    finish_started = time.monotonic()
    phase, deadline = 90, finish_started + 3
    try:
        level.editor_request_end_play()
    except Exception:
        pending_error = (pending_error or '') + '\n' + traceback.format_exc()


def camera(world, framing):
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    if framing == 'Pocket':
        logical, offset = unreal.Vector(56.5 * 800, 10.5 * 800, 0), unreal.Vector(0, -35, 48)
    elif framing == 'Parks':
        logical, offset = unreal.Vector(29 * 800, 13 * 800, 0), unreal.Vector(0, -75, 95)
    else:
        logical, offset = unreal.Vector(25600, 25600, 0), unreal.Vector(0, -225, 260)
    center = unreal.MathLibrary.transform_location(board.get_actor_transform(), logical)
    location = center + offset
    pawn.set_actor_location(location - unreal.Vector(0, 0, pawn.get_editor_property('base_eye_height')), False, True)
    controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location, center))


def pool_state():
    state = {}
    for component in board.get_components_by_class(unreal.InstancedStaticMeshComponent):
        name = component.get_name()
        mesh = component.get_editor_property('static_mesh')
        reviewed_building = mesh and mesh.get_name() in ('SM_Building_Park_A', 'SM_Building_Park_December_A', 'SM_Building_NeighborhoodPark_A', 'SM_Building_NeighborhoodPark_December_A', 'SM_Building_Residential_SeasonalSpace_A')
        if not (name.startswith('SeasonalDecoration') or name.startswith('CityAtmospherePool') or reviewed_building):
            continue
        mesh = component.get_editor_property('static_mesh')
        count = component.get_instance_count()
        state[name] = dict(instances=count, visible=component.is_visible(),
                          mesh=mesh.get_path_name() if mesh else None,
                          triangles=mesh.get_num_triangles(0) if mesh else 0,
                          materialSlots=len(mesh.get_editor_property('static_materials')) if mesh else 0,
                          collision=str(component.get_collision_enabled()))
        if name.startswith('SeasonalDecoration'):
            state[name]['transforms'] = [numeric_transform(component.get_instance_transform(i, False)) for i in range(count)]
    return state


def numeric_transform(pose):
    # Unreal struct repr can contain transient wrapper addresses. Compare the
    # actual transform fields, not Python object identity or formatting.
    t, q, s = pose.translation, pose.rotation, pose.scale3d
    return [round(float(v), 6) for v in (t.x, t.y, t.z, q.x, q.y, q.z, q.w, s.x, s.y, s.z)]


def decoration_signature(pools):
    return {name: (state['mesh'], state.get('transforms', []))
            for name, state in pools.items() if name.startswith('SeasonalDecoration')}


def assert_pools(pools, fixture, framing):
    decoration = {name: s for name, s in pools.items() if name.startswith('SeasonalDecoration')}
    assert len(decoration) == 8, 'Expected exactly eight bounded seasonal prop pools'
    assert sum(s['instances'] for s in decoration.values()) <= 64, 'Seasonal cap exceeded'
    for state in decoration.values():
        assert state['mesh'] and 'NO_COLLISION' in state['collision'], 'Seasonal props must load and never intercept picking'
    visible = sum(s['instances'] for s in decoration.values() if s['visible'])
    count = lambda kind: decoration['SeasonalDecoration' + kind]['instances']
    if framing in ('Menu', 'Diagnostic'):
        assert visible == 0, 'Menu/analytical view retained visible decorations'
        if framing == 'Menu':
            assert all(s['instances'] == 0 for s in decoration.values()), 'Menu must clear stale geometry'
        return
    assert visible > 0, 'Operational park fixture must display seasonal dressing'
    if fixture in ('Spring', 'Summer'):
        assert count('FlowerBed') > 0 and count('Pumpkin') == 0 and count('FestiveTree') == 0, 'Wrong flower season dressing'
    elif fixture == 'Autumn':
        assert count('Pumpkin') > 0 and count('LeafPile') > 0 and count('WreathAutumn') > 0 and count('WreathWinter') == 0 and count('FlowerBed') == 0 and count('FestiveTree') == 0, 'Wrong autumn path/leaves/door dressing'
    elif fixture == 'Winter':
        assert count('Pumpkin') > 0 and count('Snowman') > 0 and count('WreathWinter') > 0 and count('WreathAutumn') == 0 and count('FestiveTree') == 0, 'October cold-winter/harvest windows must coexist'
    elif fixture in ('December', 'WarmDecember'):
        assert count('FestiveTree') > 0 and count('Gift') > 0 and count('WreathWinter') > 0 and count('Pumpkin') == 0 and count('FlowerBed') == 0, 'December dressing is incomplete'
        assert (count('Snowman') > 0) == (fixture == 'December'), 'Warm and cold park snow suitability differs'
    meshes = {state['mesh'].split('.')[-1]: state for state in pools.values() if state['mesh']}
    assert 'SM_Building_Residential_SeasonalSpace_A' in meshes, 'Reviewed porch-space houses must be in use'
    for family in ('Park', 'NeighborhoodPark'):
        expected = 'SM_Building_' + family + ('_December_A' if fixture in ('December', 'WarmDecember') else '_A')
        assert expected in meshes and meshes[expected]['instances'] > 0, 'Wrong seasonal center park display: ' + expected
    trees = pools.get('CityAtmospherePool0', {})
    if fixture != 'WarmDecember':
        assert trees.get('instances', 0) > 0, 'Native review needs real natural deciduous tree samples'
    if fixture == 'Autumn':
        assert '_Autumn_A' in trees['mesh'], 'Autumn must use the authored foliage variant'
    elif fixture in ('Winter', 'December'):
        assert '_Winter_A' in trees['mesh'], 'Cold-climate winter must use authored bare trees'
        assert '_Winter_A' in pools['CityAtmospherePool1']['mesh'], 'Accumulated snow must select snowy conifer variant'
    elif fixture in ('Spring', 'Summer'):
        assert '_Autumn_A' not in trees['mesh'] and '_Winter_A' not in trees['mesh'], 'Leafy season retained stale seasonal mesh'
    if fixture == 'WarmDecember':
        warm_trees = pools.get('CityAtmospherePool4', {})
        assert warm_trees.get('instances', 0) > 0 and '_Winter_A' in warm_trees['mesh'], 'Ordinary warm broadleaf must lose winter foliage'


def tick(delta):
    global phase, deadline, index, ui, board, sim, pending_error, last_attempt
    try:
        now = time.monotonic()
        if phase < 90 and now - start > 240:
            raise RuntimeError('D66 native review timed out')
        if now < deadline:
            return
        if phase == 90:
            if level.is_in_play_in_editor():
                level.editor_request_end_play()
                deadline = now + 1
                if now - finish_started > 45:
                    pending_error = (pending_error or '') + '\nPIE teardown taking over45 seconds; waiting before save restoration.'
                return
            restore_saves()
            phase, deadline = 91, now + 2
        elif phase == 91:
            if current_saves() != prior:
                restore_saves()
                deadline = now + 2
                return
            map_unchanged = map_path.read_bytes() == map_bytes
            if not map_unchanged:
                pending_error = (pending_error or '') + '\nSandbox map bytes changed during native review.'
            if pending_error:
                error_path.write_text(pending_error + '\nLocal save files/bytes restored; map unchanged=' + str(map_unchanged) + '.\n', encoding='utf-8')
                report_path.write_text(json.dumps(dict(samples=report, lastAttempt=last_attempt, error=pending_error,
                    localSaveBytesRestored=True, localSaveFileCount=len(prior), noMapSaved=map_unchanged), indent=2) + '\n', encoding='utf-8')
            else:
                report_path.write_text(json.dumps(dict(samples=report, localSaveBytesRestored=True,
                    localSaveFileCount=len(prior), noMapSaved=True), indent=2) + '\n', encoding='utf-8')
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
        elif phase == 0:
            level.editor_request_begin_play()
            phase, deadline = 1, now + 8
        elif phase == 1:
            world = editor.get_game_world()
            if not world:
                return
            actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            ui = next(a for a in actors if a.get_class().get_name() == 'CityVRToolController')
            board = next(a for a in actors if a.get_class().get_name() == 'CityBoardActor')
            sim = next(o for o in unreal.ObjectIterator(unreal.CitySimulationSubsystem) if o.get_outer() == world)
            ui.return_to_city()
            ui.set_debug_mode_enabled(False)
            ui.set_city_summary_visible(False)
            for command in ('FOV 60', 'DisableAllScreenMessages', 't.IdleWhenNotForeground 0', 'r.EyeAdaptationQuality 0'):
                unreal.SystemLibrary.execute_console_command(world, command)
            phase, deadline = 2, now + 1
        elif phase == 2:
            view, fixture, lighting, framing = views[index]
            if framing == 'Menu':
                ui.show_main_menu()
            else:
                ui.return_to_city()
                error = sim.load_city_from_path(str(fixtures[fixture]), False)
                if error:
                    raise RuntimeError(str(error))
                sim.set_simulation_paused(True)
                board.set_tool_mode(unreal.CityToolMode.INSPECT)
                board.set_diagnostic_overlay(unreal.CityDiagnosticOverlay.POLLUTION if framing == 'Diagnostic' else unreal.CityDiagnosticOverlay.NONE)
                board.set_lighting_preset(unreal.CityLightingPreset.NIGHT if lighting == 'Night' else unreal.CityLightingPreset.DAY)
                camera(editor.get_game_world(), framing)
            phase, deadline = 3, now + 4
        elif phase == 3:
            view, fixture, lighting, framing = views[index]
            pools = pool_state()
            last_attempt = dict(view=view, fixture=fixture, lighting=lighting, framing=framing, pools=pools)
            assert_pools(pools, fixture, framing)
            if framing not in ('Menu', 'Diagnostic'):
                signature = decoration_signature(pools)
                if fixture in prior_signatures:
                    assert signature == prior_signatures[fixture], 'Reload/camera/day-night changed seeded dressing'
                else:
                    prior_signatures[fixture] = signature
            report.append(dict(view=view, fixture=fixture, lighting=lighting, framing=framing, pools=pools))
            path = out / ('D66-Refinement-' + view + '-PIE.png')
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(), f'HighResShot 1800x1400 filename="{path.as_posix()}"')
            phase, deadline = 4, now + 3
        elif phase == 4:
            assert (out / ('D66-Refinement-' + views[index][0] + '-PIE.png')).exists(), 'Missing native screenshot'
            index += 1
            if index < len(views):
                phase, deadline = 2, now + 1
            else:
                finish()
    except Exception:
        failure = traceback.format_exc()
        if phase >= 90:
            pending_error = (pending_error or '') + '\n' + failure
            error_path.write_text(pending_error, encoding='utf-8')
            # Keep retrying while save bytes differ; never quit on a failed restore.
            phase, deadline = 90, time.monotonic() + 2
        else:
            finish(failure)


handle = unreal.register_slate_post_tick_callback(tick)

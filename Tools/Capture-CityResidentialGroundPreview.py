"""D66 native residential lawn-pad and terrain support review.

Prepare isolated fixtures with ordinary Python and --fixtures-only. Then run
UnrealEditor -NoHMD -ExecCmds="py <absolute script path>" on CitySandbox.
This reads real pooled transforms and procedural terrain vertices, captures
flat seasonal regressions and two sloped Homes, and restores all local saves
and sandbox-map bytes after completed PIE teardown. It never saves the map.
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


def prepare_fixtures():
    paths = {}
    for season in ('Autumn', 'December'):
        source_path = out / ('D66-Refinement-' + season + '-City.json')
        source = json.loads(source_path.read_text(encoding='utf-8-sig'))
        assert source['schemaVersion'] == 58, 'Use the validated current seasonal fixture'
        for sloped in (False, True):
            label = ('Sloped' if sloped else 'Flat') + season
            document = copy.deepcopy(source)
            document['cityId'] = uuid.uuid5(uuid.NAMESPACE_URL, 'citysim-d66-residential-' + label).hex
            document['cityName'] = 'D66 ground ' + label
            # Generator0 and seed1 retain the known dry road/lot layout. The
            # slope amplitudes are60% of CityTerrain.h defaults500/650/450cm.
            document['terrain'].update(seed=1, generatorVersion=0,
                broadElevationAmplitudeCm=300 if sloped else 0,
                hillAmplitudeCm=390 if sloped else 0,
                ridgeAmplitudeCm=270 if sloped else 0)
            document['economy']['paused'] = True
            path = out / ('D66-ResidentialGround-' + label + '-City.json')
            path.write_text(json.dumps(document, indent=2) + '\n', encoding='utf-8')
            paths[label] = path
    return paths


fixtures = prepare_fixtures()
if '--fixtures-only' in sys.argv:
    print('CITYSIM_RESIDENTIAL_GROUND_FIXTURES_READY', ', '.join(map(str, fixtures.values())))
    raise SystemExit(0)

import unreal

views = [('FlatAutumnOverview', 'FlatAutumn', 0, 'Day', 'Overview'),
         ('FlatAutumnPorch', 'FlatAutumn', 0, 'Day', 'Detail'),
         ('FlatDecemberOverview', 'FlatDecember', 0, 'Day', 'Overview'),
         ('FlatDecemberPorch', 'FlatDecember', 0, 'Day', 'Detail'),
         ('SlopedHome1Overview', 'SlopedAutumn', 0, 'Day', 'Overview'),
         ('SlopedHome1Porch', 'SlopedAutumn', 0, 'Day', 'Detail'),
         ('SlopedHome1Night', 'SlopedAutumn', 0, 'Night', 'Overview'),
         ('SlopedHome2Overview', 'SlopedAutumn', 1, 'Day', 'Overview'),
         ('SlopedHome2Porch', 'SlopedAutumn', 1, 'Day', 'Detail'),
         ('SlopedHome2Night', 'SlopedAutumn', 1, 'Night', 'Overview'),
         ('SlopedDecemberPorch', 'SlopedDecember', 0, 'Day', 'Detail')]
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
backup = out / ('D66-ResidentialGround-LocalBackup-' + str(time.time_ns()))
backup.mkdir(parents=True, exist_ok=True)
for relative, data in prior.items():
    path = backup / 'SaveGames' / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
map_path = root / 'CitySimVR/Content/CitySim/Maps/CitySandbox.umap'
map_bytes = map_path.read_bytes()
(backup / 'CitySandbox.umap').write_bytes(map_bytes)
report_path = out / 'D66-ResidentialGround-PIE.json'
error_path = out / 'D66-ResidentialGround-PIE.error.txt'
pending_error = None
for path in [report_path, error_path] + [out / ('D66-ResidentialGround-' + v[0] + '-PIE.png') for v in views]:
    path.unlink(missing_ok=True)


def current_saves():
    return {p.relative_to(saved): p.read_bytes() for p in saved.rglob('*') if p.is_file()}


def restore_saves():
    for path in saved.rglob('*'):
        if path.is_file() and path.relative_to(saved) not in prior:
            path.unlink()
    for relative, data in prior.items():
        path = saved / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    assert current_saves() == prior, 'Local save files/bytes differ after restoration'


def emit_partial():
    report_path.write_text(json.dumps(dict(samples=report, lastAttempt=last_attempt,
        error=pending_error, restorationPending=True), indent=2) + '\n', encoding='utf-8')


def finish(error=None):
    global phase, deadline, pending_error
    if phase >= 90:
        if error:
            pending_error = (pending_error or '') + '\n' + error
        return
    pending_error = error
    emit_partial()
    phase, deadline = 90, time.monotonic() + 3
    try:
        level.editor_request_end_play()
    except Exception:
        pending_error = (pending_error or '') + '\n' + traceback.format_exc()


def numeric_transform(pose):
    t, q, s = pose.translation, pose.rotation, pose.scale3d
    return [round(float(v), 6) for v in (t.x, t.y, t.z, q.x, q.y, q.z, q.w, s.x, s.y, s.z)]


def terrain_vertices():
    surface = next(c for c in board.get_components_by_class(unreal.ProceduralMeshComponent)
                   if c.get_name() == 'BoardSurface')
    samples = {}
    for section in range(surface.get_num_sections()):
        vertices = unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(surface, section)[0]
        for p in vertices:
            key = (round(p.x / 400), round(p.y / 400))
            assert abs(p.x - key[0] * 400) < .01 and abs(p.y - key[1] * 400) < .01
            if key in samples:
                assert abs(samples[key] - p.z) < .01, 'Terrain chunks disagree at shared edge'
            samples[key] = float(p.z)
    assert len(samples) == 129 * 129, 'Read the complete actual two-samples-per-cell terrain'
    return samples


def checked_pad_mesh(pool):
    mesh = pool.get_editor_property('static_mesh')
    assert mesh and mesh.get_name() == 'SM_Building_LotFoundation_A', 'Home must use authored lawn/concrete pad'
    assert pool.is_visible() and 'NO_COLLISION' in str(pool.get_collision_enabled())
    bounds = mesh.get_bounding_box()
    assert all(abs(v + 50) < .01 for v in (bounds.min.x, bounds.min.y, bounds.min.z))
    assert all(abs(v - 50) < .01 for v in (bounds.max.x, bounds.max.y, bounds.max.z))
    assert mesh.get_num_triangles(0) == 12 and mesh.get_num_sections(0) == 2
    materials, top_triangles, other_triangles = [], 0, 0
    for section in range(2):
        material = pool.get_material(section)
        assert material, 'Every residential pad section has a material'
        path = material.get_path_name().split('.')[0]
        assert path in ('/Game/CitySim/Art/Materials/MI_CityPark', '/Game/CitySim/Art/Materials/MI_CityTraffic'), path
        materials.append(path)
        vertices, triangles, normals, uvs, tangents = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, section)
        for i in range(0, len(triangles), 3):
            points = [vertices[triangles[i + j]] for j in range(3)]
            on_top = all(abs(p.z - 50) < .01 for p in points)
            assert path == ('/Game/CitySim/Art/Materials/MI_CityPark' if on_top else '/Game/CitySim/Art/Materials/MI_CityTraffic'), 'Lawn belongs only on upward face; concrete belongs on sides/bottom'
            top_triangles += on_top
            other_triangles += not on_top
    assert top_triangles == 2 and other_triangles == 10
    return dict(mesh=mesh.get_path_name(), triangles=12, materials=materials,
                lawnTopTriangles=top_triangles, concreteSideBottomTriangles=other_triangles)


def matching_instance(pool, x, y):
    for i in range(pool.get_instance_count()):
        pose = pool.get_instance_transform(i, False)
        if abs(pose.translation.x - x) < .1 and abs(pose.translation.y - y) < .1:
            return pose
    return None


def review_homes(document, sloped):
    pools = board.get_components_by_class(unreal.InstancedStaticMeshComponent)
    foundation_pool = next(c for c in pools if c.get_name() == 'ResidentialFoundationInstances')
    pad = checked_pad_mesh(foundation_pool)
    height_samples = terrain_vertices()
    homes = [e for e in document['buildings'] if e['definitionId'] == 'Home']
    assert foundation_pool.get_instance_count() == len(homes), 'Exactly one lawn foundation per Home'
    home_pools = [c for c in pools if c.get_editor_property('static_mesh') and
                  c.get_editor_property('static_mesh').get_name() == 'SM_Building_Residential_SeasonalSpace_A']
    assert home_pools, 'Low Home uses reviewed porch-space display mesh'
    for home_pool in home_pools:
        mesh = home_pool.get_editor_property('static_mesh')
        bounds = mesh.get_bounding_box()
        assert abs(bounds.min.z + 20) < .1, 'Lowered authored driveway rests on pad at house pivot minus20cm'
        assert max(abs(bounds.min.x), abs(bounds.max.x), abs(bounds.min.y), abs(bounds.max.y)) <= 800.1, 'Baked walk stays inside owning16m lot'
    neutral_pool = next(c for c in pools if c.get_name() == 'FoundationInstances')
    assert neutral_pool.get_material(0).get_name() == 'MI_CityTraffic', 'Other building foundations use pale concrete instead of prototype checkerboard'
    candidates, all_foundations = [], []
    for owner in homes:
        assert owner['quarterTurns'] == 0 and owner['width'] == 2 and owner['height'] == 2, 'This fixture intentionally uses low unrotated Homes'
        center_x, center_y = (owner['x'] + 1) * 800, (owner['y'] + 1) * 800
        foundation = matching_instance(foundation_pool, center_x, center_y)
        assert foundation, 'Native residential pad matches logical Home owner'
        footprint_heights = [height_samples[(x, y)]
            for y in range(owner['y'] * 2, (owner['y'] + 2) * 2 + 1)
            for x in range(owner['x'] * 2, (owner['x'] + 2) * 2 + 1)]
        minimum, maximum = min(footprint_heights), max(footprint_heights)
        top = foundation.translation.z + foundation.scale3d.z * 50
        bottom = foundation.translation.z - foundation.scale3d.z * 50
        assert abs(top - (maximum + 20)) < .1, 'Full-lot pad clears highest fine terrain sample by20cm'
        assert abs(bottom - (minimum - 20)) < .1, 'Pad skirt extends20cm below lowest fine terrain sample'
        assert abs(foundation.scale3d.x - 16) < .001 and abs(foundation.scale3d.y - 16) < .001
        house_pose, house_pool = None, None
        for candidate_pool in home_pools:
            pose = matching_instance(candidate_pool, center_x, center_y)
            if pose:
                house_pose, house_pool = pose, candidate_pool
                break
        assert house_pose, 'Native house instance matches logical Home owner'
        assert abs(house_pose.translation.z - (maximum + 40)) < .1, 'House shares maximum-sample base with pad and props'
        data = dict(owner=owner, terrainFineSamplesCm=footprint_heights, terrainMinimumCm=minimum,
            terrainMaximumCm=maximum, terrainReliefCm=maximum-minimum,
            padTopCm=top, padBottomCm=bottom, padTransform=numeric_transform(foundation),
            houseMesh=house_pool.get_editor_property('static_mesh').get_path_name(),
            houseTransform=numeric_transform(house_pose), props=[])
        ground_pose = None
        for pool in pools:
            if not pool.get_name().startswith('SeasonalDecoration'):
                continue
            mesh = pool.get_editor_property('static_mesh')
            if not mesh:
                continue
            bounds = mesh.get_bounding_box()
            kind = pool.get_name().removeprefix('SeasonalDecoration')
            for i in range(pool.get_instance_count()):
                pose = pool.get_instance_transform(i, False)
                dx, dy = pose.translation.x-center_x, pose.translation.y-center_y
                if abs(dx) >= 800 or abs(dy) >= 800:
                    continue
                assert pool.is_visible() and 'NO_COLLISION' in str(pool.get_collision_enabled())
                prop_bottom = pose.translation.z + bounds.min.z * pose.scale3d.z
                wreath = kind.startswith('Wreath')
                if wreath:
                    assert abs(dx) < .1 and abs(dy + 648) < .1
                    assert abs(prop_bottom - (house_pose.translation.z + 215)) < .1
                    assert abs(pose.scale3d.x - 1) < .001
                else:
                    assert abs(abs(dx) - 240) < .1 and abs(dy + 710) < .1
                    assert .449 < pose.scale3d.x < .501
                    assert abs(prop_bottom - (top + 2)) < .1, 'Porch accent rests2cm above actual lawn pad'
                    ground_pose = pose
                data['props'].append(dict(kind=kind, mesh=mesh.get_path_name(), triangles=mesh.get_num_triangles(0),
                    transform=numeric_transform(pose), bottomCm=prop_bottom,
                    padSupportClearanceCm=prop_bottom-top, wallMounted=wreath))
        all_foundations.append(data)
        if ground_pose and (not sloped or maximum-minimum >= 10):
            candidates.append((data, ground_pose))
    candidates.sort(key=lambda item: (-item[0]['terrainReliefCm'], int(item[0]['owner']['entityId'])))
    assert len(candidates) >= (2 if sloped else 1), 'Need distinct visibly sloped decorated Homes, not flat starter lots'
    assert any(p['kind'] == ('Gift' if 'December' in document['cityName'] else 'Pumpkin') for data, pose in candidates for p in data['props'])
    assert any(p['wallMounted'] for data, pose in candidates for p in data['props']), 'Seasonal door wreath regression'
    return candidates, dict(pad=pad, homeCount=len(homes), actualTerrainSampleCount=len(height_samples), homes=all_foundations)


def camera(data, prop_pose, framing):
    world = editor.get_game_world()
    transform = board.get_actor_transform()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    owner = data['owner']
    sign = 1 if prop_pose.translation.x >= (owner['x'] + 1) * 800 else -1
    target = unreal.Vector((owner['x'] + 1) * 800, (owner['y'] + 1) * 800, data['padTopCm'] + 250)
    if framing == 'Detail':
        target = unreal.Vector(prop_pose.translation.x * .5 + target.x * .5,
                               prop_pose.translation.y, data['padTopCm'] + 130)
    center = unreal.MathLibrary.transform_location(transform, target)
    offset = unreal.Vector(sign * 11, -20, 19) if framing == 'Overview' else unreal.Vector(sign * 8, -14, 5)
    location = center + offset
    pawn.set_actor_location(location - unreal.Vector(0, 0, pawn.get_editor_property('base_eye_height')), False, True)
    controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location, center))


def tick(delta):
    global phase, deadline, index, ui, board, sim, pending_error, last_attempt, fixture, selected, geometry
    try:
        now = time.monotonic()
        if phase < 90 and now-start > 360:
            raise RuntimeError('Native residential ground review timed out')
        if now < deadline:
            return
        if phase == 90:
            if level.is_in_play_in_editor():
                level.editor_request_end_play()
                deadline = now + 1
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
                map_path.write_bytes(map_bytes)
                pending_error = (pending_error or '') + '\nSandbox map bytes changed and were restored.'
            payload = dict(samples=report, lastAttempt=last_attempt, localSaveBytesRestored=True,
                localSaveFileCount=len(prior), sandboxMapBytesRestored=map_path.read_bytes() == map_bytes,
                noMapSaved=map_unchanged, backup=str(backup), elapsedSeconds=round(now-start, 2))
            if pending_error:
                payload['error'] = pending_error
                error_path.write_text(pending_error, encoding='utf-8')
            report_path.write_text(json.dumps(payload, indent=2) + '\n', encoding='utf-8')
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
            for command in ('FOV 45', 'DisableAllScreenMessages', 't.IdleWhenNotForeground 0', 'r.EyeAdaptationQuality 0'):
                unreal.SystemLibrary.execute_console_command(world, command)
            phase, deadline = 2, now + 1
        elif phase == 2:
            view, label, selected_index, lighting, framing = views[index]
            fixture = json.loads(fixtures[label].read_text(encoding='utf-8'))
            error = sim.load_city_from_path(str(fixtures[label]), False)
            if error:
                raise RuntimeError(str(error))
            sim.set_simulation_paused(True)
            board.set_tool_mode(unreal.CityToolMode.INSPECT)
            board.set_diagnostic_overlay(unreal.CityDiagnosticOverlay.NONE)
            board.set_lighting_preset(unreal.CityLightingPreset.NIGHT if lighting == 'Night' else unreal.CityLightingPreset.DAY)
            phase, deadline = 3, now + 3
        elif phase == 3:
            view, label, selected_index, lighting, framing = views[index]
            candidates, geometry = review_homes(fixture, label.startswith('Sloped'))
            selected, pose = candidates[selected_index]
            signature = json.dumps(geometry, sort_keys=True)
            if label in prior_signatures:
                assert signature == prior_signatures[label], 'Repeated load/night camera must preserve exact pads and seasonal transforms'
            prior_signatures[label] = signature
            last_attempt = dict(view=view, fixture=label, lighting=lighting, framing=framing,
                                selectedHome=selected, geometry=geometry)
            camera(selected, pose, framing)
            phase, deadline = 4, now + 3
        elif phase == 4:
            report.append(last_attempt)
            path = out / ('D66-ResidentialGround-' + views[index][0] + '-PIE.png')
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(), f'HighResShot 1800x1400 filename="{path.as_posix()}"')
            phase, deadline = 5, now + 3
        elif phase == 5:
            assert (out / ('D66-ResidentialGround-' + views[index][0] + '-PIE.png')).exists(), 'Missing native residential ground screenshot'
            index += 1
            if index < len(views):
                phase, deadline = 2, now + 1
            else:
                finish()
    except Exception:
        failure = traceback.format_exc()
        if phase >= 90:
            pending_error = (pending_error or '') + '\n' + failure
            emit_partial()
            error_path.write_text(pending_error, encoding='utf-8')
            phase, deadline = 90, time.monotonic() + 2
        else:
            finish(failure)


handle = unreal.register_slate_post_tick_callback(tick)

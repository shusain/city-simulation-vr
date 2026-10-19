"""Native D66 small city-lot accent support/entrance review.

Run after Capture-CitySeasonalPreview.py, using its isolated schema58 fixtures.
Find a real small pooled gift/pumpkin and its owning lot before framing it.
Preserve every local save file and the sandbox map through completed EndPIE.
"""
from pathlib import Path
import json
import time
import traceback
import unreal

root = Path(__file__).resolve().parents[1]
out = root / 'Artifacts/Preview'
views = [('LotGiftOverview', 'December', 'Gift', 'Overview'),
         ('LotGiftFront', 'December', 'Gift', 'Front'),
         ('LotPumpkinOverview', 'Autumn', 'Pumpkin', 'Overview'),
         ('LotPumpkinFront', 'Autumn', 'Pumpkin', 'Front'),
         ('LotAutumnWreathOverview', 'Autumn', 'WreathAutumn', 'Overview'),
         ('LotAutumnWreathFront', 'Autumn', 'WreathAutumn', 'Front'),
         ('LotWinterWreathOverview', 'December', 'WreathWinter', 'Overview'),
         ('LotWinterWreathFront', 'December', 'WreathWinter', 'Front')]
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
start = time.monotonic()
deadline = start + 5
phase = 0
index = 0
report = []
last_attempt = None
saved = root / 'CitySimVR/Saved/SaveGames'
prior = {p.relative_to(saved): p.read_bytes() for p in saved.rglob('*') if p.is_file()}
backup = out / ('D66-Refinement-Lot-LocalSaveBackup-' + str(time.time_ns()))
for relative, data in prior.items():
    path = backup / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
map_path = root / 'CitySimVR/Content/CitySim/Maps/CitySandbox.umap'
map_bytes = map_path.read_bytes()
report_path = out / 'D66-Refinement-Lot-PIE.json'
error_path = out / 'D66-Refinement-Lot-PIE.error.txt'
pending_error = None
finish_started = 0
for path in [report_path, error_path] + [out / ('D66-Refinement-' + v[0] + '-PIE.png') for v in views]:
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


def numeric_transform(pose):
    t, q, s = pose.translation, pose.rotation, pose.scale3d
    return [round(float(v), 6) for v in (t.x, t.y, t.z, q.x, q.y, q.z, q.w, s.x, s.y, s.z)]


def select_lot(kind, document):
    pool = next(c for c in board.get_components_by_class(unreal.InstancedStaticMeshComponent)
                if c.get_name() == 'SeasonalDecoration' + kind)
    mesh = pool.get_editor_property('static_mesh')
    for instance in range(pool.get_instance_count()):
        pose = pool.get_instance_transform(instance, False)
        if not kind.startswith('Wreath') and pose.scale3d.x > .55:
            continue  # Park samples are substantially larger than city accents.
        x, y = pose.translation.x / 800, pose.translation.y / 800
        owner = next((e for e in document['buildings'] if e['definitionId'] == 'Home'
                      and e['x'] <= x < e['x'] + e['width']
                      and e['y'] <= y < e['y'] + e['height']), None)
        if not owner:
            continue
        bounds = mesh.get_bounding_box()
        # The new display model keeps its logical16m lot and full foundation,
        # while body/roof XY are88% of the original. Wreaths mount on the
        # authored door; ground props flank the entrance walk in the front strip.
        center_x, center_y = (owner['x'] + owner['width'] / 2) * 800, (owner['y'] + owner['height'] / 2) * 800
        local_x, local_y = pose.translation.x - center_x, pose.translation.y - center_y
        radius = max(abs(bounds.min.x), abs(bounds.max.x), abs(bounds.min.y), abs(bounds.max.y)) * pose.scale3d.x * 1.415
        wreath = kind.startswith('Wreath')
        if wreath:
            assert abs(local_x) < .1 and abs(local_y + 648) < .1, 'Wreath must mount on the authored door face'
            assert abs(pose.scale3d.x - 1) < .001, 'Door wreath must retain reviewed size'
        else:
            assert abs(abs(local_x) - 240) < .1 and abs(local_y + 710) < .1, 'Ground accent must flank the porch walk'
            assert -local_y - radius > 616, 'Ground prop overlaps the88% body'
            assert abs(local_x) - radius > 150, 'Ground prop overlaps the center entrance walk'
        assert abs(local_x) + radius < 800 and abs(local_y) + radius < 800, 'Decoration crosses the owning footprint boundary'
        assert pool.is_visible() and 'NO_COLLISION' in str(pool.get_collision_enabled()), 'Prop must remain visible and not intercept inspection'
        building_mesh = None
        building_pose = None
        for candidate in board.get_components_by_class(unreal.InstancedStaticMeshComponent):
            candidate_mesh = candidate.get_editor_property('static_mesh')
            if not candidate_mesh or candidate_mesh.get_name() != 'SM_Building_Residential_SeasonalSpace_A':
                continue
            for j in range(candidate.get_instance_count()):
                p = candidate.get_instance_transform(j, False)
                if abs(p.translation.x - center_x) < .1 and abs(p.translation.y - center_y) < .1:
                    building_mesh, building_pose = candidate_mesh, p
                    break
        assert building_pose, 'Actual native low Home instance must match the selected owner'
        foundation_pool = next(c for c in board.get_components_by_class(unreal.InstancedStaticMeshComponent)
                               if c.get_name() == 'ResidentialFoundationInstances')
        foundation = None
        for j in range(foundation_pool.get_instance_count()):
            p = foundation_pool.get_instance_transform(j, False)
            if abs(p.translation.x - center_x) < .1 and abs(p.translation.y - center_y) < .1:
                foundation = p
                break
        assert foundation, 'Actual full-lot foundation must match the accent owner'
        foundation_top = foundation.translation.z + foundation.scale3d.z * 50
        bottom = pose.translation.z + bounds.min.z * pose.scale3d.z
        if wreath:
            assert abs(bottom - (building_pose.translation.z + 215)) < .1, 'Wreath must mount at the reviewed door height'
        else:
            assert abs(bottom - (foundation_top + 2)) < .1, 'Ground prop must stand2cm above the real foundation'
        return pose, owner, dict(owner=owner, prop=kind, pool=pool.get_name(), instance=instance,
            transform=numeric_transform(pose), mesh=mesh.get_path_name(), triangles=mesh.get_num_triangles(0),
            boundsMinCm=[bounds.min.x, bounds.min.y, bounds.min.z], boundsMaxCm=[bounds.max.x, bounds.max.y, bounds.max.z],
            conservativeRadiusCm=radius, frontBodyClearanceCm=-local_y - radius - 616,
            footprintClearanceCm=min(800 - abs(local_x) - radius, 800 - abs(local_y) - radius),
            bottomCm=bottom, foundationTopCm=foundation_top, supportClearanceCm=bottom - foundation_top,
            foundationTransform=numeric_transform(foundation),
            homeMesh=building_mesh.get_path_name(), homeTransform=numeric_transform(building_pose),
            note='Door wreath at reviewed mount height' if wreath else 'Grounded porch-side accent, clear of centered entrance walk and smaller house body.')
    raise RuntimeError('No actual small ' + kind + ' instance on a low Home lot')


def camera(pose, owner, framing):
    world = editor.get_game_world()
    transform = board.get_actor_transform()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    center_local = unreal.Vector((owner['x'] + 1) * 800, (owner['y'] + 1) * 800, 0)
    sign_x = 1 if pose.translation.x >= center_local.x else -1
    if framing == 'Overview':
        center = unreal.MathLibrary.transform_location(transform, center_local)
        location = center + unreal.Vector(sign_x * 8, -17, 23)
    else:
        target = pose.translation + unreal.Vector(0, 0, 60 if views[index][2].startswith('Wreath') else 0)
        center = unreal.MathLibrary.transform_location(transform, target)
        location = center + unreal.Vector(sign_x * 8, -14, 4)
    pawn.set_actor_location(location - unreal.Vector(0, 0, pawn.get_editor_property('base_eye_height')), False, True)
    controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(location, center))


def tick(delta):
    global phase, deadline, index, ui, board, sim, pending_error, last_attempt, fixture, selected, owner
    try:
        now = time.monotonic()
        if phase < 90 and now - start > 220:
            raise RuntimeError('Native lot review timed out')
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
                pending_error = (pending_error or '') + '\nSandbox map bytes changed.'
            payload = dict(samples=report, lastAttempt=last_attempt, localSaveBytesRestored=True,
                           localSaveFileCount=len(prior), noMapSaved=map_unchanged)
            if pending_error:
                payload['error'] = pending_error
                error_path.write_text(pending_error + '\nLocal saves restored; map unchanged=' + str(map_unchanged), encoding='utf-8')
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
            view, season, kind, framing = views[index]
            path = out / ('D66-Refinement-' + season + '-City.json')
            fixture = json.loads(path.read_text(encoding='utf-8'))
            error = sim.load_city_from_path(str(path), False)
            if error:
                raise RuntimeError(str(error))
            sim.set_simulation_paused(True)
            board.set_tool_mode(unreal.CityToolMode.INSPECT)
            board.set_diagnostic_overlay(unreal.CityDiagnosticOverlay.NONE)
            board.set_lighting_preset(unreal.CityLightingPreset.DAY)
            phase, deadline = 3, now + 3
        elif phase == 3:
            view, season, kind, framing = views[index]
            selected, owner, data = select_lot(kind, fixture)
            last_attempt = dict(view=view, framing=framing, **data)
            camera(selected, owner, framing)
            phase, deadline = 4, now + 3
        elif phase == 4:
            report.append(last_attempt)
            path = out / ('D66-Refinement-' + views[index][0] + '-PIE.png')
            unreal.SystemLibrary.execute_console_command(editor.get_game_world(), f'HighResShot 1800x1400 filename="{path.as_posix()}"')
            phase, deadline = 5, now + 3
        elif phase == 5:
            assert (out / ('D66-Refinement-' + views[index][0] + '-PIE.png')).exists(), 'Missing native lot screenshot'
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
            phase, deadline = 90, time.monotonic() + 2
        else:
            finish(failure)


handle = unreal.register_slate_post_tick_callback(tick)

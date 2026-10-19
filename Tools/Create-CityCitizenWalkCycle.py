"""D121: editable walk study derived from D87; never imports into Unreal."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import sys

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Tools'))
from CityCitizenRigControls import move_control, seed_hinges, update

SOURCE = ROOT / 'ArtSource/Blender/CityCitizenWalkCycle-v2.blend'
MANIFEST = ROOT / 'ArtSource/Manifests/city_citizen_walk_cycle_v2.json'
PERIOD = 28
FPS = 24
STANCE = .62
REACH = .28
SCALE = .811
STRIDE = 2 * REACH / STANCE


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def foot_path(t):
    """Linear stance, velocity-continuous recovery and a flat planted sole."""
    u = t % 1
    if u < STANCE:
        return REACH - 2 * REACH * u / STANCE, 0
    v = (u - STANCE) / (1 - STANCE)
    smooth = v * v * (3 - 2 * v)
    tangent = -2 * REACH * (1 - STANCE) / STANCE
    x = -REACH + 2 * REACH * smooth + tangent * (2*v**3 - 3*v*v + v)
    return x, .105 * math.sin(math.pi * v)**2


def pose(rig, t):
    for bone in rig.pose.bones:
        bone.matrix_basis.identity()
    seed_hinges(rig)
    phase = math.tau * t
    move_control(rig, 'pelvis', (0, .012*math.sin(phase), -.045-.012*math.cos(phase*2)))
    # Local Y runs along the torso. Opposing pelvis/chest angles must not
    # cancel at the shoulders: retain a visible net upper-body twist.
    rig.pose.bones['pelvis'].rotation_euler.y = -math.radians(4)*math.cos(phase)
    rig.pose.bones['spine'].rotation_euler.y = math.radians(7)*math.cos(phase)
    rig.pose.bones['chest'].rotation_euler.y = math.radians(7)*math.cos(phase)
    rig.pose.bones['neck'].rotation_euler.y = -math.radians(3)*math.cos(phase)
    rig.pose.bones['head'].rotation_euler.y = -math.radians(4)*math.cos(phase)
    rig.pose.bones['pelvis'].rotation_euler.z = -math.radians(1.5)
    rig.pose.bones['spine'].rotation_euler.z = -math.radians(3)
    rig.pose.bones['chest'].rotation_euler.z = -math.radians(2)
    rig.pose.bones['neck'].rotation_euler.z = math.radians(1.5)
    rig.pose.bones['head'].rotation_euler.z = math.radians(1)
    for side, offset, sign in (('L', 0, 1), ('R', .5, -1)):
        x, z = foot_path(t + offset)
        foot = rig.data.bones['CTRL_foot.'+side].head_local
        goal = Vector((x, foot.y, foot.z+z))
        move_control(rig, 'CTRL_foot.'+side, goal-foot)
        hand = rig.data.bones['CTRL_hand.'+side].head_local
        goal = Vector((.025-.14*math.cos(phase+offset*math.tau), sign*.24,
                       .97+.015*math.sin(phase+offset*math.tau)))
        move_control(rig, 'CTRL_hand.'+side, goal-hand)
    update(rig)
    # Follow the solved forearm, then straighten the source wrist's neutral
    # bend. Preserve wrist position and forearm roll while aligning hand Y.
    for side in ('L', 'R'):
        fore = rig.pose.bones['forearm.'+side]
        control = rig.pose.bones['CTRL_hand.'+side]
        matrix = fore.matrix @ fore.bone.matrix_local.inverted() @ control.bone.matrix_local
        direction = matrix.to_3x3() @ Vector((0,1,0))
        correction = direction.rotation_difference((fore.tail-fore.head).normalized())
        aligned = correction.to_matrix().to_4x4() @ matrix
        aligned.translation = control.head
        control.matrix = aligned
    update(rig)


def linear(action):
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for point in curve.keyframe_points:
                        point.interpolation = 'LINEAR'
                    curve.modifiers.new('CYCLES')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--replace-generated', action='store_true')
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    if SOURCE.exists():
        assert args.replace_generated and MANIFEST.exists(), 'Existing source preserved'
        previous = json.loads(MANIFEST.read_text())
        assert previous['status'] != 'approved', 'Approved source frozen; choose a new revision/path'
        assert sha(SOURCE) == previous['source_sha256'], 'Artist edits preserved'
    inputs = [ROOT/'ArtSource/Blender/CityCitizenControlRig.blend',
              ROOT/'ArtSource/Manifests/city_citizen_control_rig.json',
              ROOT/'ArtSource/Blender/CityCitizenSportsScale.blend',
              ROOT/'ArtSource/Blender/CityCitizenWalkCycle.blend',
              ROOT/'ArtSource/Manifests/city_citizen_walk_cycle.json',
              ROOT/'Tools/CityCitizenRigControls.py', ROOT/'Tools/CityCitizenSportsMotion.py']
    protected = {p.relative_to(ROOT).as_posix(): sha(p) for p in inputs}
    bpy.ops.wm.open_mainfile(filepath=str(inputs[0]))
    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    scene = bpy.context.scene
    scene.name = 'D121_Walk_AllOutfits'
    for obj in scene.objects:
        obj.hide_render = True
    records = []
    for style, y in (('Casual', -1.1), ('Office', 0), ('Worker', 1.1)):
        for suffix in ('', '_LOD1'):
            mesh = bpy.data.objects['SK_Citizen_Rig_'+style+suffix]
            rig = mesh.parent
            rig.animation_data_clear()
            rig.location = (0, y, 0)
            rig.rotation_euler = (0, 0, 0)
            rig.scale = (SCALE,)*3
            mesh.hide_set(bool(suffix)); rig.hide_set(bool(suffix))
            mesh.hide_render = bool(suffix)
            rig.animation_data_create()
            action = bpy.data.actions.new('D121_Walk_'+style+suffix+'_Controls')
            action.use_fake_user = True
            rig.animation_data.action = action
            for frame in range(1, PERIOD+2):
                scene.frame_set(frame)
                pose(rig, (frame-1)/PERIOD)
                for bone in rig.pose.bones:
                    if bone.name.startswith('CTRL_') or bone.name in (
                            'root', 'pelvis', 'spine', 'chest', 'neck', 'head', 'clavicle.L', 'clavicle.R'):
                        for channel in ('location', 'rotation_euler'):
                            bone.keyframe_insert(channel, frame=frame, group=bone.name)
            linear(action)
            records.append(dict(style=style, lod=int(bool(suffix)), mesh=mesh.name,
                                rig=rig.name, action=action.name))
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1
    scene.frame_start = 1; scene.frame_end = PERIOD; scene.render.fps = FPS
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_x = 1000; scene.render.resolution_y = 620
    scene.render.resolution_percentage = 100
    shade = scene.display.shading
    shade.light = 'STUDIO'; shade.color_type = 'VERTEX'
    shade.show_shadows = True; shade.show_cavity = True
    shade.background_type = 'WORLD'
    scene.world.color = (.065, .085, .115)
    scene.view_settings.view_transform = 'Standard'
    data = bpy.data.cameras.new('D121_ReviewCamera')
    camera = bpy.data.objects.new('D121_ReviewCamera', data)
    scene.collection.objects.link(camera); scene.camera = camera
    camera.location = (5, -7, 3.3)
    camera.rotation_euler = (Vector((0, 0, .73))-camera.location).to_track_quat('-Z', 'Y').to_euler()
    data.type = 'ORTHO'; data.ortho_scale = 4.4
    # A simple ground reference; the animation remains entirely in place.
    bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -.003))
    ground = bpy.context.object; ground.name = 'D121_ReviewGround'
    ground.color = (.15, .19, .23, 1)
    material = bpy.data.materials.new('D121_ReviewGround')
    material.diffuse_color = (.15, .19, .23, 1); ground.data.materials.append(material)
    for old in list(bpy.data.actions):
        if not old.name.startswith('D121_'):
            old.use_fake_user = False
            if old.users == 0: bpy.data.actions.remove(old)
    text = bpy.data.texts.new('D121_Walk_README')
    text.write('D121 v2 walk-cycle SOURCE PREVIEW, awaiting human review.\n'
               'Play 1-28 at 24fps; 29 duplicates frame 1. +X forward.\n'
               'Casual, Office, Worker and LOD1 have separate editable control actions.\n'
               'The root stays in place. Grounded feet move backward at constant speed.\n'
               'Use the original D87 controls; do not key hidden deform bones.\n'
               'Save an artist copy before changing this generated file.\n'
               'No game assets, routing, arrival rules or packages changed.\n')
    scene.frame_set(1)
    bpy.ops.object.select_all(action='DESELECT')
    hero = bpy.data.objects['SK_Citizen_Rig_Casual'].parent
    hero.hide_set(False); hero.select_set(True); bpy.context.view_layer.objects.active = hero
    bpy.ops.object.mode_set(mode='POSE')
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                area.spaces.active.region_3d.view_perspective = 'CAMERA'
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))
    doc = dict(task='D121', version=2, status='preview-unreviewed',
               source=SOURCE.relative_to(ROOT).as_posix(), source_sha256=sha(SOURCE),
               protected_inputs=protected, period_frames=PERIOD, fps=FPS,
               duplicate_endpoint=PERIOD+1, stance_fraction=STANCE,
               forward_axis='+X', units='metres', rig_scale=SCALE,
               stride_authoring_m=STRIDE, stride_preview_m=STRIDE*SCALE,
               nominal_preview_speed_m_s=STRIDE*SCALE*FPS/PERIOD,
               root_motion='In-place; no accumulated translation or rotation', records=records,
               provenance='OpenAI Codex-authored control curves; existing original project D87 meshes/rigs',
               revision_notes='Hands follow solved forearms; visible shoulder/pelvis counter-twist and 6.5-degree forward torso lean; original v1 stride retained',
               previous_source='ArtSource/Blender/CityCitizenWalkCycle.blend',
               native_status='Not imported; review source before integration')
    MANIFEST.write_text(json.dumps(doc, indent=2)+'\n')
    for path, digest in protected.items(): assert sha(ROOT/path) == digest, path
    print('CITYSIM_D121_CREATE_PASS', SOURCE)


if __name__ == '__main__':
    main()

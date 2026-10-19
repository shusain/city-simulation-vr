"""D87 build a separate constrained Blender authoring file from accepted D83."""
from pathlib import Path
import bpy, sys, json, hashlib
from mathutils import Vector, Quaternion
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tools'))
from CityCitizenRigControls import configure, preset, BASE_BONES, BODY_LIMITS, HINGE_LIMITS, update, seed_hinges

SOURCE=ROOT/'ArtSource/Blender/CityCitizenRigPrototype.blend'
OUTPUT=ROOT/'ArtSource/Blender/CityCitizenControlRig.blend'
OUT=ROOT/'Artifacts/D87';OUT.mkdir(parents=True,exist_ok=True)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
if OUTPUT.exists():
    previous=ROOT/'ArtSource/Manifests/city_citizen_control_rig.json'
    if '--replace-generated' not in sys.argv:
        raise RuntimeError('Authoring file already exists. Save artist work separately before --replace-generated.')
    if not previous.exists() or sha(OUTPUT)!=json.loads(previous.read_text())['source_sha256']:
        raise RuntimeError('Authoring file differs from generated baseline; preserve artist edits and choose a separate source.')
protected=[SOURCE,ROOT/'ArtSource/Blender/CityCitizenPrototype.blend',ROOT/'ArtSource/Blender/CityParkAnimationPreview.blend',
           ROOT/'ArtSource/Manifests/city_park_refinement.json',ROOT/'ArtSource/Manifests/city_park_integration.json']
protected+=list((ROOT/'ArtSource/Exports/ParkRefinement').glob('SK_Citizen*.fbx'))
before={p.relative_to(ROOT).as_posix():sha(p) for p in protected}
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
scene=bpy.context.scene;scene.name='D87_Citizen_Animation_Workbench';scene.render.fps=24
meshes=sorted([o for o in bpy.data.objects if o.type=='MESH' and o.name.startswith('SK_Citizen_Rig_')],key=lambda o:o.name)
rigs=[m.parent for m in meshes]
assert len(meshes)==6
for o in list(bpy.data.objects):
    if o not in meshes+rigs:bpy.data.objects.remove(o,do_unlink=True)
records=[]
for i,mesh in enumerate(meshes):
    rig=mesh.parent;mesh.hide_set(False);rig.hide_set(False)
    bind={b.name:[list(row) for row in b.matrix_local] for b in rig.data.bones}
    setup=configure(rig)
    assert {n:v['matrix'] for n,v in json.loads(rig['D87_original_bind']).items()}==bind
    # Shoes are rigid under their foot handle; blended shin weights made the
    # old prototypes' soles tilt/deform even when the ankle target was planted.
    neighbors={v.index:set() for v in mesh.data.vertices}
    for edge in mesh.data.edges:
        a,b=edge.vertices;neighbors[a].add(b);neighbors[b].add(a)
    remaining=set(neighbors)
    while remaining:
        stack=[min(remaining)];comp=set(stack)
        while stack:
            for other in neighbors[stack.pop()]-comp:comp.add(other);stack.append(other)
        remaining-=comp;coords=[mesh.data.vertices[j].co for j in comp]
        if min(v.z for v in coords)<1e-5 and max(v.z for v in coords)<.11:
            side='L' if sum(v.y for v in coords)>0 else 'R'
            for j in comp:
                for group in list(mesh.data.vertices[j].groups):mesh.vertex_groups[group.group].remove([j])
            mesh.vertex_groups['foot.'+side].add(list(comp),1,'REPLACE')
    rig.location=(0,(i//2)*1.1,0)
    low=mesh.name.endswith('LOD1');mesh.hide_render=low;mesh.hide_set(low);rig.hide_set(low)
    records.append(dict(mesh=mesh.name,rig=rig.name,controls=[b.name for b in rig.data.bones if b.name.startswith('CTRL_')],
                        original_bones=list(bind),bone_count=len(rig.data.bones),calibration=setup))
# Accepted actions stay in their unchanged original source. Keep this action
# browser focused on usable control animation rather than incompatible FK clips.
for reference in list(bpy.data.actions):bpy.data.actions.remove(reference)
hero=bpy.data.objects['SK_Citizen_Rig_Casual'];rig=hero.parent
rig.location=(0,0,0);rig.hide_set(False);hero.hide_set(False)
rig.animation_data_create();action=bpy.data.actions.new('D87_Demo_PlantedCrouch');action.use_fake_user=True
rig.animation_data.action=action
for frame,depth in ((1,0),(13,-.22),(25,0),(37,-.22),(49,0)):
    for p in rig.pose.bones:p.matrix_basis.identity()
    seed_hinges(rig)
    rig.pose.bones['pelvis'].location=(0,depth,0)
    for side in ('L','R'):
        p=rig.pose.bones['CTRL_hand.'+side]
        p.location=rig.data.bones[p.name].matrix_local.to_3x3().inverted()@Vector((.22,0,.08))
    update(rig)
    for p in rig.pose.bones:
        if p.name in ('root','pelvis','spine','chest','neck','head') or p.name.startswith('CTRL_'):
            p.keyframe_insert('location',frame=frame,group=p.name);p.keyframe_insert('rotation_euler',frame=frame,group=p.name)
for layer in action.layers:
    for strip in layer.strips:
        for bag in strip.channelbags:
            for c in bag.fcurves:
                for p in c.keyframe_points:p.interpolation='BEZIER'
scene.frame_start=1;scene.frame_end=48;scene.frame_set(1)
text=bpy.data.texts.new('D87_START_HERE')
text.write('Citizen authoring rig (D87)\n\nPose Mode: move CTRL_hand/CTRL_foot; rotate them for orientation.\nMove diamond elbow/knee poles to steer the bend plane.\nMove pelvis to crouch while feet remain planted; root moves everything.\nRotate blue spine/chest/neck/head/clavicle controls for body posing.\nAlt-G / Alt-R clears selected pose channels. Scale is locked.\n\nTimeline 1-48 previews the new planted crouch demo at24fps.\nTo author: unlink the demo, create a new action, key the Body + IK + Pole collections.\nOriginal D83 actions remain references in their unchanged source file.\nDo not directly assign them to this constrained rig: they animate deform bones.\nReveal Limb bones collection only for inspecting constraints/limits.\n\nExport: Tools/Bake-CityCitizenControlRig.py bakes a chosen new action\ninto a clean original20-bone skeleton, stripping all controls/constraints.\nDetailed ranges and commands: docs/CITIZEN_RIGGING.md.\n')
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig
rig.data.bones.active=rig.data.bones['pelvis'];rig.pose.bones['pelvis'].select=True
bpy.ops.object.mode_set(mode='POSE')
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.shading.color_type='VERTEX'
            area.spaces.active.region_3d.view_distance=3.4
            area.spaces.active.region_3d.view_location=(0,0,.9)
            area.spaces.active.region_3d.view_rotation=Quaternion((.723,.66,.138,.139)).normalized()
            area.spaces.active.overlay.show_relationship_lines=False
            area.spaces.active.overlay.show_bones=True
scene['D87_authoring_only']=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT))
assert before=={p.relative_to(ROOT).as_posix():sha(p) for p in protected}
manifest=dict(task='D87',source=OUTPUT.relative_to(ROOT).as_posix(),source_sha256=sha(OUTPUT),
              input_source=SOURCE.relative_to(ROOT).as_posix(),protected_inputs=before,
              original_bones=list(BASE_BONES),control_count=8,records=records,
              body_local_xyz_limits_degrees=BODY_LIMITS,hinge_local_z_limits_degrees=HINGE_LIMITS,
              native_selected=False,notes=['Source-only; native assets and accepted D83 clips unchanged',
                                         'Body rotation limits are practical authoring defaults, not a medical model',
                                         'IK hinge limits are on the solver; pole targets steer upper limb rotation',
                                         'Original reference actions require baking/retargeting before constrained editing',
                                         'Authoring limb rolls aligned; original joint positions/lengths retained; bake restores original20-bone bind matrices',
                                         'Authoring derivative uses rigid shoe weights; original accepted weights unchanged'])
(ROOT/'ArtSource/Manifests/city_citizen_control_rig.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('CITYSIM_D87_AUTHORING_PASS',len(records),'rigs;28 bones each;8 controls;original20-bone export bind retained')

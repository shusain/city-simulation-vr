"""D83 isolated Blender curve/tree/rig prototypes; never imports or builds Unreal.

blender --background --factory-startup --threads 2 --python this-file
Only writes D83-specific sources, exports, manifest and preview evidence.
"""
from pathlib import Path
import bpy, bmesh, hashlib, json, math
from mathutils import Vector, Quaternion
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from CityParkGeometry import append, components, bounds, triangles, mesh_corners, refine

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'Artifacts/ArtPreview/D83'
EXPORT = ROOT/'ArtSource/Exports/ParkRefinement'
SOURCE = ROOT/'ArtSource/Blender/CityParkRefinement.blend'
RIG_SOURCE = ROOT/'ArtSource/Blender/CityCitizenRigPrototype.blend'
MANIFEST = ROOT/'ArtSource/Manifests/city_park_refinement.json'
EVIDENCE = ROOT/'Artifacts/D83'
BOARD_SCALE=.65
BOARD_DECK=.08*BOARD_SCALE
BOARD_CLEARANCE=.13*BOARD_SCALE
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def reset():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1
    scene.world = bpy.data.worlds.new('D83World')
    scene.world.color = (.11, .13, .16)
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_x = scene.render.resolution_y = 800
    scene.render.resolution_percentage = 100
    scene.render.fps = 24
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'MATERIAL'
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.display.shading.background_type = 'WORLD'
    scene.view_settings.view_transform = 'Standard'
    cam = bpy.data.cameras.new('D83ReviewCamera')
    camera = bpy.data.objects.new('D83ReviewCamera', cam)
    scene.collection.objects.link(camera)
    scene.camera = camera
    cam.type = 'ORTHO'
    return scene


def render(objects, path, scale, aim, direction=(30, -44, 25), vertex=False):
    scene = bpy.context.scene
    for obj in scene.objects:
        if obj.type == 'MESH':
            obj.hide_render = obj not in objects
    camera = scene.camera
    aim = Vector(aim)
    camera.location = aim+Vector(direction)
    camera.rotation_euler = (aim-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.ortho_scale = scale
    scene.display.shading.color_type = 'VERTEX' if vertex else 'MATERIAL'
    scene.render.filepath = str(OUT/path)
    bpy.ops.render.render(write_still=True)


def export_static(obj):
    path = EXPORT/(obj.name+'.fbx')
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    expected = mesh_corners(obj)
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH'},
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE',
        axis_forward='-Y', axis_up='Z', bake_anim=False, add_leaf_bones=False)
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path))
    imported = [o for o in set(bpy.data.objects)-before if o.type == 'MESH']
    assert len(imported) == 1
    check = imported[0]
    bpy.context.view_layer.objects.active = check
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    assert triangles(check) == triangles(obj), obj.name
    assert mesh_corners(check) == expected, 'Geometry/color/UV/normals roundtrip: '+obj.name
    for other in set(bpy.data.objects)-before:
        bpy.data.objects.remove(other, do_unlink=True)
    return path


def main():
    for folder in (OUT, EXPORT, EVIDENCE):
        folder.mkdir(parents=True, exist_ok=True)

    starter = json.loads((ROOT/'ArtSource/Manifests/city_starter_kit.json').read_text())
    ids = ('building.watertower.a', 'building.service.a', 'building.service.medium.a',
           'building.service.high.a', 'building.park.a', 'building.park.neighborhood',
           'building.park.city', 'building.park.skate')
    entries = [e for e in starter['assets'] if e['asset_id'] in ids
               or e['asset_id'] in tuple(i+'.damaged' for i in ids)]
    entries = [e.get('park_refinement_source_entry',e) for e in entries]
    citizen_manifest = json.loads((ROOT/'ArtSource/Manifests/city_citizen_prototype.json').read_text())
    protected = {ROOT/e['source'] for e in entries+citizen_manifest['assets']}
    protected |= {ROOT/e['export'] for e in entries+citizen_manifest['assets']}
    protected |= {ROOT/'ArtSource/Manifests'/p for p in ('city_citizen_prototype.json', 'city_citizen_presentation.json')}
    protected |= {ROOT/'ArtSource/Blender/CityEnvironmentKit.blend', ROOT/'ArtSource/Exports/SM_Scenery_Broadleaf_A.fbx'}
    hashes = {p.relative_to(ROOT).as_posix(): digest(p) for p in protected}

    scene = reset()
    records = []
    # Eight normal prototypes. Damaged versions are isolated follow-up work: their
    # baked deformation should be regenerated from the accepted damage recipes later.
    for entry in [e for e in entries if e['asset_id'] in ids]:
        name = Path(entry['export']).stem
        obj = append(ROOT/entry['source'], name)
        original_bounds = bounds(obj)
        count = triangles(obj)
        scale = 17 if 'WaterTower' in name else 28 if 'Service' in name else 36 if name=='SM_Building_Park_A' else 64 if 'CityPark' in name else 50
        aim = (0,0,5 if 'WaterTower' in name or 'Service' in name else 1.5)
        render([obj], name+'-Before.png', scale, aim)
        changes = refine(obj, entry['asset_id'])
        assert max(abs(a-b) for row_a,row_b in zip(bounds(obj),original_bounds) for a,b in zip(row_a,row_b)) < .001
        obj.name = name
        render([obj], name+'-After.png', scale, aim)
        if 'Skate' in name:
            render([obj], 'Skate-Ramp-Detail.png', 14, (10.5,0,1.6), (12,-20,7))
        path = export_static(obj)
        records.append(dict(asset_id=entry['asset_id'], name=obj.name, source=SOURCE.relative_to(ROOT).as_posix(),
                            source_input=entry['source'], export=path.relative_to(ROOT).as_posix(),
                            sha256=digest(path), triangles_before=count, triangles_after=triangles(obj),
                            bounds_m=bounds(obj), material_slots=[m.name.split('.')[0] for m in obj.data.materials],
                            changes=changes, roundtrip_pass=True, footprint_cells=entry['footprint_cells'],
                            entrance_sides_local=entry['entrance_sides_local'], prototype_only=True))
    # Arrange a source gallery only after ground-centered exports.
    for i,obj in enumerate([o for o in scene.objects if o.type=='MESH']):
        obj.hide_render = False
        obj.location = ((i%4)*70,(i//4)*80,0)
    scene.camera.location=(295,-240,205)
    scene.camera.rotation_euler=(Vector((105,40,3))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
    scene.camera.data.ortho_scale=310
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type=='VIEW_3D':area.spaces.active.region_3d.view_perspective='CAMERA'
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))

    scene = reset()
    CLIPS = {'Idle':48, 'Walk':24, 'SkateBalance':48, 'Cheer':48, 'SeatedIdle':48,
             'SkatePush':48, 'SkateCarve':96, 'BaseballPitch':72, 'BaseballSwing':48,
             'SoccerRun':24, 'SoccerKick':48, 'GoalkeeperSave':48,
             'BasketballDribble':48, 'BasketballShot':72,
             'SkateKickflip':72,'Skate180':96,'SkateLipStall':120}

    def make_rig(mesh):
        name = mesh.name.replace('SM_', 'SK_').replace('_PC_', '_Rig_')
        armature = bpy.data.armatures.new(name+'_Skeleton')
        rig = bpy.data.objects.new(name+'_Rig', armature)
        scene.collection.objects.link(rig)
        bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True)
        bpy.context.view_layer.objects.active=rig
        bpy.ops.object.mode_set(mode='EDIT')
        bones = {}
        def bone(label,head,tail,parent=None,deform=True):
            b = armature.edit_bones.new(label); b.head=head; b.tail=tail
            b.use_deform=deform
            if parent: b.parent=bones[parent]
            bones[label]=b
        bone('root',(0,0,0),(0,0,.2),deform=False)
        bone('pelvis',(0,0,.88),(0,0,1.04),'root')
        bone('spine',(0,0,1.04),(0,0,1.23),'pelvis')
        bone('chest',(0,0,1.23),(0,0,1.43),'spine')
        bone('neck',(0,0,1.43),(0,0,1.52),'chest')
        bone('head',(0,0,1.52),(0,0,1.765),'neck')
        for side,sign in (('L',1),('R',-1)):
            bone('clavicle.'+side,(0,0,1.39),(0,sign*.185,1.39),'chest')
            bone('upper_arm.'+side,(0,sign*.185,1.39),(-.025*sign,sign*.25,1.18),'clavicle.'+side)
            bone('forearm.'+side,(-.025*sign,sign*.25,1.18),(-.07*sign,sign*.27,.98),'upper_arm.'+side)
            bone('hand.'+side,(-.07*sign,sign*.27,.98),(-.07*sign,sign*.27,.92),'forearm.'+side)
            bone('thigh.'+side,(0,sign*.087,.88),(.042*sign,sign*.085,.44),'pelvis')
            bone('shin.'+side,(.042*sign,sign*.085,.44),(.07*sign,sign*.088,.09),'thigh.'+side)
            bone('foot.'+side,(.07*sign,sign*.088,.09),(.07*sign+.16,sign*.088,.04),'shin.'+side)
        bpy.ops.object.mode_set(mode='OBJECT')
        groups = {b.name:mesh.vertex_groups.new(name=b.name) for b in armature.bones if b.use_deform}
        for comp in components(mesh.data):
            coords = [mesh.data.vertices[i].co for i in comp]
            low=min(v.z for v in coords); high=max(v.z for v in coords)
            lateral=sum(v.y for v in coords)/len(coords)
            side='L' if lateral>0 else 'R'
            if high < .11: labels=['foot.'+side]
            elif low<.11: labels=['shin.'+side,'thigh.'+side]
            elif low>.87 and high<1.01 and abs(lateral)<.1: labels=['pelvis']
            elif low>1.48: labels=['head']
            elif low>1.42 and high>1.5: labels=['neck']
            elif abs(lateral)>.18:
                labels=['hand.'+side] if high<1.01 else ['forearm.'+side] if high<1.2 else ['upper_arm.'+side]
            else: labels=['spine','chest']
            for i in comp:
                if len(labels)==1: weights=[1]
                else:
                    z=mesh.data.vertices[i].co.z
                    t=max(0,min(1,(z-.38)/.12)) if 'shin' in labels[0] else max(0,min(1,(z-1.12)/.18))
                    weights=[1-t,t]
                for label,weight in zip(labels,weights):
                    if weight>0:
                        # Keep disconnected loft seams attached to their parent joint.
                        # The original source topology and color corners remain exact.
                        parent=None; blend=0
                        if label.startswith('thigh.'):
                            parent='pelvis'; blend=max(0,min(1,(mesh.data.vertices[i].co.z-.80)/.08))
                        elif label.startswith('upper_arm.'):
                            parent='chest'; blend=max(0,min(1,(mesh.data.vertices[i].co.z-1.36)/.07))
                        elif label.startswith('forearm.'):
                            parent='upper_arm.'+side; blend=max(0,min(1,(mesh.data.vertices[i].co.z-1.10)/.08))
                        elif label.startswith('hand.'):
                            parent='forearm.'+side; blend=max(0,min(1,(mesh.data.vertices[i].co.z-.95)/.04))
                        elif label.startswith('foot.'):
                            parent='shin.'+side; blend=max(0,min(1,(mesh.data.vertices[i].co.z-.02)/.08))
                        if weight*(1-blend)>0:groups[label].add([i],weight*(1-blend),'REPLACE')
                        if parent and weight*blend>0:groups[parent].add([i],weight*blend,'REPLACE')
        for v in mesh.data.vertices:
            assert abs(sum(g.weight for g in v.groups)-1)<1e-6
            assert 1<=len(v.groups)<=2
        mesh.name=name
        modifier=mesh.modifiers.new('D83HumanoidSkin','ARMATURE'); modifier.object=rig
        mesh.parent=rig; rig.show_in_front=True
        grip=rig.pose.bones['forearm.L'].constraints.new('IK')
        grip.name='D83BatGrip';grip.target=rig;grip.subtarget='hand.R';grip.chain_count=2
        grip.influence=0
        return rig

    def rotation(rig,label,axis,degrees):
        pose=rig.pose.bones[label]; pose.rotation_mode='QUATERNION'
        local=rig.data.bones[label].matrix_local.to_quaternion().inverted()@Vector(axis)
        pose.rotation_quaternion=Quaternion(local, math.radians(degrees))@pose.rotation_quaternion

    def pulse(t,start,end):
        return math.sin(math.pi*(t-start)/(end-start))**2 if start<t<end else 0

    def animate(rig):
        actions=[]
        for clip,period in CLIPS.items():
            rig.animation_data_create()
            action=bpy.data.actions.new(rig.name+'_'+clip); action.use_fake_user=True
            rig.animation_data.action=action
            for frame in range(1,period+2):
                scene.frame_set(frame)
                phase=(frame-1)/period*math.tau
                t=(frame-1)/period
                for pose in rig.pose.bones:
                    pose.rotation_mode='QUATERNION'; pose.rotation_quaternion=Quaternion()
                    pose.location=(0,0,0)
                grip=rig.pose.bones['forearm.L'].constraints['D83BatGrip']
                grip.influence=1 if clip=='BaseballSwing' else 0
                grip.keyframe_insert('influence',frame=frame)
                rotation(rig,'chest',(0,1,0),1.5*math.sin(phase))
                if clip in ('Walk','SoccerRun'):
                    for side,sign in (('L',1),('R',-1)):
                        swing=math.sin(phase)*sign
                        rotation(rig,'thigh.'+side,(0,1,0),(40 if clip=='SoccerRun' else 26)*swing)
                        rotation(rig,'shin.'+side,(0,1,0),(60 if clip=='SoccerRun' else 32)*max(0,-swing))
                        rotation(rig,'foot.'+side,(0,1,0),-6*max(0,-swing))
                        rotation(rig,'upper_arm.'+side,(0,1,0),-20*swing)
                        rotation(rig,'forearm.'+side,(0,1,0),-10)
                    if clip=='SoccerRun':rotation(rig,'chest',(0,1,0),8)
                elif clip=='SkateBalance':
                    rotation(rig,'pelvis',(0,1,0),8)
                    rotation(rig,'chest',(1,0,0),4*math.sin(phase))
                    for side,sign in (('L',1),('R',-1)):
                        rotation(rig,'thigh.'+side,(0,1,0),-18)
                        rotation(rig,'shin.'+side,(0,1,0),32)
                        rotation(rig,'upper_arm.'+side,(1,0,0),sign*32)
                        rotation(rig,'forearm.'+side,(0,1,0),-18)
                elif clip=='Cheer':
                    for side,sign in (('L',1),('R',-1)):
                        rotation(rig,'upper_arm.'+side,(1,0,0),sign*(135+5*math.sin(phase)))
                        rotation(rig,'forearm.'+side,(0,1,0),-25)
                elif clip=='SeatedIdle':
                    for side in ('L','R'):
                        rotation(rig,'thigh.'+side,(0,1,0),-85)
                        rotation(rig,'shin.'+side,(0,1,0),85)
                        rotation(rig,'upper_arm.'+side,(0,1,0),-30)
                        rotation(rig,'forearm.'+side,(0,1,0),-70)
                    rig.pose.bones['root'].location.y=-.38
                elif clip in ('SkatePush','SkateCarve'):
                    crouch=pulse(t,.05,.95)
                    rotation(rig,'pelvis',(0,1,0),10)
                    if clip=='SkateCarve':rotation(rig,'chest',(0,1,0),28*crouch)
                    rotation(rig,'chest',(1,0,0),4*math.sin(phase))
                    for side,sign in (('L',1),('R',-1)):
                        angle=-18-16*crouch if clip=='SkateCarve' else -14
                        bend=32+26*crouch if clip=='SkateCarve' else 18
                        if clip=='SkatePush' and side=='R':
                            angle=-14+42*math.sin(phase);bend=20+35*max(0,-math.sin(phase))
                        rotation(rig,'thigh.'+side,(0,1,0),angle)
                        rotation(rig,'shin.'+side,(0,1,0),bend)
                        rotation(rig,'upper_arm.'+side,(1,0,0),sign*(26+12*crouch))
                        rotation(rig,'forearm.'+side,(0,1,0),-20)
                elif clip in ('SkateKickflip','Skate180','SkateLipStall'):
                    prep=pulse(t,.02,.37);tuck=pulse(t,.27,.72);landing=pulse(t,.64,.98)
                    if clip=='SkateLipStall':
                        prep=pulse(t,.02,.33);tuck=pulse(t,.25,.80);landing=pulse(t,.70,.98)
                    rotation(rig,'pelvis',(0,1,0),8)
                    rotation(rig,'chest',(0,1,0),-8*tuck if clip=='SkateLipStall' else 8*tuck)
                    for side,sign in (('L',1),('R',-1)):
                        rotation(rig,'thigh.'+side,(0,1,0),-16-22*prep-42*tuck-20*landing)
                        rotation(rig,'shin.'+side,(0,1,0),24+28*prep+50*tuck+26*landing)
                        rotation(rig,'upper_arm.'+side,(1,0,0),sign*(22+30*tuck))
                        rotation(rig,'forearm.'+side,(0,1,0),-16)
                    if clip=='SkateKickflip':rotation(rig,'foot.R',(0,0,1),18*pulse(t,.16,.44))
                elif clip=='BaseballPitch':
                    windup=pulse(t,.05,.50);release=pulse(t,.35,.70);follow=pulse(t,.50,.95)
                    rotation(rig,'chest',(0,0,1),-30*windup+35*follow)
                    rotation(rig,'chest',(0,1,0),12*follow)
                    rotation(rig,'thigh.L',(0,1,0),-62*windup)
                    rotation(rig,'shin.L',(0,1,0),85*windup)
                    rotation(rig,'upper_arm.R',(0,1,0),-135*windup-90*release-25*follow)
                    rotation(rig,'forearm.R',(0,1,0),75*windup-5*release)
                    rotation(rig,'upper_arm.L',(0,1,0),-65*windup-30*release)
                    rotation(rig,'forearm.L',(0,1,0),-35*windup)
                elif clip=='BaseballSwing':
                    windup=pulse(t,.03,.49);hit=pulse(t,.31,.71);follow=pulse(t,.48,.98)
                    rotation(rig,'pelvis',(0,0,1),-25*windup+35*follow)
                    rotation(rig,'chest',(0,0,1),-25*windup+45*follow)
                    for side,sign in (('L',1),('R',-1)):
                        rotation(rig,'upper_arm.'+side,(0,1,0),-55-20*hit)
                        rotation(rig,'upper_arm.'+side,(1,0,0),sign*8)
                        rotation(rig,'forearm.'+side,(0,1,0),-65*windup-10*hit)
                        rotation(rig,'thigh.'+side,(0,1,0),-8)
                        rotation(rig,'shin.'+side,(0,1,0),12)
                elif clip=='SoccerKick':
                    windup=pulse(t,.04,.40);strike=pulse(t,.25,.69);follow=pulse(t,.49,.97)
                    rotation(rig,'chest',(0,1,0),8*strike)
                    rotation(rig,'thigh.R',(0,1,0),35*windup-65*strike-15*follow)
                    rotation(rig,'shin.R',(0,1,0),55*windup+8*follow)
                    rotation(rig,'foot.R',(0,1,0),-18*strike)
                    rotation(rig,'upper_arm.L',(0,1,0),-30*strike)
                    rotation(rig,'upper_arm.R',(0,1,0),20*strike)
                elif clip=='GoalkeeperSave':
                    reach=pulse(t,.05,.95)
                    rotation(rig,'chest',(1,0,0),-10*reach)
                    for side,sign in (('L',1),('R',-1)):
                        rotation(rig,'upper_arm.'+side,(1,0,0),sign*(25+55*reach))
                        rotation(rig,'forearm.'+side,(0,1,0),-20)
                        rotation(rig,'thigh.'+side,(1,0,0),sign*15)
                        rotation(rig,'shin.'+side,(0,1,0),22)
                elif clip=='BasketballDribble':
                    bounce=(1-math.cos(phase*2))/2
                    rotation(rig,'chest',(0,1,0),6)
                    rotation(rig,'upper_arm.R',(0,1,0),-25)
                    rotation(rig,'forearm.R',(0,1,0),-15-32*bounce)
                    rotation(rig,'upper_arm.L',(1,0,0),20)
                elif clip=='BasketballShot':
                    gather=pulse(t,.04,.59);shoot=pulse(t,.33,.90)
                    for side,sign in (('L',1),('R',-1)):
                        rotation(rig,'upper_arm.'+side,(0,1,0),-55*gather-125*shoot)
                        rotation(rig,'forearm.'+side,(0,1,0),-55*gather+15*shoot)
                        rotation(rig,'thigh.'+side,(0,1,0),-18*gather)
                        rotation(rig,'shin.'+side,(0,1,0),30*gather)
                # In-place pose test: correct foot-grounding at every sample. Root
                # translation is exported for evaluation; runtime motion stays separate.
                bpy.context.view_layer.update()
                child=next(o for o in rig.children if o.type=='MESH')
                evaluated=child.evaluated_get(bpy.context.evaluated_depsgraph_get())
                skin=evaluated.to_mesh()
                minimum=min(v.co.z for v in skin.vertices)
                evaluated.to_mesh_clear()
                # The vertical root bone's local Y axis is world Z.
                rig.pose.bones['root'].location.y-=minimum
                for pose in rig.pose.bones:
                    pose.keyframe_insert('rotation_quaternion',frame=frame,group=pose.name)
                    pose.keyframe_insert('location',frame=frame,group=pose.name)
            actions.append(action)
        rig.animation_data.action=actions[0]
        return actions

    rig_records=[]
    for entry in citizen_manifest['assets']:
        mesh=append(ROOT/entry['source'], entry['name'])
        original_geometry=[tuple(v.co) for v in mesh.data.vertices]
        original_colors=[tuple(c.color) for c in mesh.data.color_attributes.active_color.data]
        rig=make_rig(mesh)
        actions=animate(rig)
        for action,period in zip(actions,CLIPS.values()):
            track=rig.animation_data.nla_tracks.new()
            track.name=action.name
            strip=track.strips.new(action.name,1,action)
            strip.action_frame_start=1;strip.action_frame_end=period+1
            strip.frame_start=1;strip.frame_end=period+1
        rig.animation_data.action=None
        assert original_geometry==[tuple(v.co) for v in mesh.data.vertices]
        assert original_colors==[tuple(c.color) for c in mesh.data.color_attributes.active_color.data]
        path=EXPORT/(mesh.name+'.fbx')
        bpy.ops.object.select_all(action='DESELECT'); mesh.select_set(True);rig.select_set(True)
        bpy.context.view_layer.objects.active=rig
        scene.frame_start=1;scene.frame_end=max(CLIPS.values())+1
        bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH','ARMATURE'},
            apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',mesh_smooth_type='FACE',
            use_mesh_modifiers=False,axis_forward='-Y',axis_up='Z',add_leaf_bones=False,
            bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=True,
            bake_anim_simplify_factor=0,bake_anim_step=1)
        for track in rig.animation_data.nla_tracks: track.mute=True
        rig_records.append(dict(name=mesh.name,source_input=entry['export'],
                                source=RIG_SOURCE.relative_to(ROOT).as_posix(),export=path.relative_to(ROOT).as_posix(),
                                sha256=digest(path),triangles=triangles(mesh),bones=list(rig.data.bones.keys()),
                                maximum_weights_per_vertex=2,all_weights_normalized=True,
                                actions=[a.name for a in actions],clips=CLIPS,prototype_only=True,
                                original_geometry_and_rgba_preserved=True))
        if not mesh.name.endswith('LOD1'):
            for action in actions:
                rig.animation_data.action=action
                scene.frame_set(9 if action.name.endswith('Walk') else 13)
                render([mesh], mesh.name+'-'+action.name.rsplit('_',1)[-1]+'.png',2.3,(0,0,.88),(6,-4,1.5),vertex=True)
            if mesh.name.endswith('Casual'):
                rig.animation_data.action=actions[1]
                for frame in range(1,25):
                    scene.frame_set(frame)
                    scene.render.resolution_x=scene.render.resolution_y=480
                    render([mesh], f'Walk-{frame:02}.png',2.3,(0,0,.88),(6,-4,1.5),vertex=True)
                scene.render.resolution_x=scene.render.resolution_y=800
        rig.animation_data.action=actions[0]

    # Park previews use linked rigged meshes rather than the legacy capsule stand-ins.
    # Keep each activity as a separate object; it is not baked into building exports.
    for mesh in [o for o in scene.objects if o.type=='MESH']:
        mesh.hide_render=True
    for park_name, source_name, width in (
            ('SM_Building_Park_A','SM_Building_Park_A',36),
            ('SM_Building_SkatePark_A','SM_Building_SkatePark_A',50)):
        before=set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(EXPORT/(source_name+'.fbx')))
        park=next(o for o in set(bpy.data.objects)-before if o.type=='MESH')
        bpy.context.view_layer.objects.active=park
        bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
        for material in park.data.materials:
            palette={'Park':(.18,.62,.22,1),'Trim':(.82,.73,.48,1),'Traffic':(.88,.88,.86,1),
                     'Bark':(.22,.12,.055,1),'Canopy':(.12,.32,.07,1)}
            material.diffuse_color=palette[material.name.split('.')[0]]
        people=[];boards=[]
        for i,label in enumerate(('Casual','Office','Worker')):
            mesh=bpy.data.objects.get('SK_Citizen_Rig_'+label)
            rig=mesh.parent
            action=next(a for a in bpy.data.actions if a.name==rig.name+('_SkateBalance' if 'Skate' in park_name else '_Walk'))
            rig.animation_data.action=action
            rig.location=((i-1)*4,0,.47 if 'Skate' not in park_name else .36+BOARD_CLEARANCE+BOARD_DECK)
            # Preserve current presentation world-height: the accepted game citizen
            # is ~1.24m with park scale1.15, so normalize this source before that scale.
            rig.scale=(.811,.811,.811)
            people.append(mesh)
            if 'Skate' in park_name:
                before=set(bpy.data.objects)
                bpy.ops.import_scene.fbx(filepath=str(ROOT/'ArtSource/Exports/SM_Park_Skateboard_A.fbx'))
                board=next(o for o in set(bpy.data.objects)-before if o.type=='MESH')
                bpy.context.view_layer.objects.active=board
                bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
                board.location=((i-1)*4,0,.36+BOARD_CLEARANCE);board.scale=(BOARD_SCALE,)*3
                for material in board.data.materials:
                    material.diffuse_color={'Traffic':(.88,.88,.86,1),'Roof':(.1,.14,.19,1)}[material.name.split('.')[0]]
                attr=board.data.color_attributes.active_color
                for p in board.data.polygons:
                    for j in p.loop_indices:attr.data[j].color=board.data.materials[p.material_index].diffuse_color
                boards.append(board)
        scene.frame_set(9)
        render([park]+people+boards,park_name+'-Visitors.png',width,(0,0,1.5),vertex=False)
        # Workbench vertex display shows the citizens' palettes and recolors park
        # opaque faces from their accepted material; keep authoring masks intact.
        copy=park.data.copy();park.data=copy
        colors=copy.color_attributes.active_color
        for p in copy.polygons:
            for i in p.loop_indices:
                colors.data[i].color=copy.materials[p.material_index].diffuse_color
        render([park]+people+boards,park_name+'-Visitors.png',width,(0,0,1.5),vertex=True)
        render([park]+people+boards,park_name+'-Visitors-Close.png',13,(0,0,1),(12,-16,9),vertex=True)
        bpy.data.objects.remove(park,do_unlink=True)
        for board in boards:bpy.data.objects.remove(board,do_unlink=True)

    for i,record in enumerate(rig_records):
        mesh=bpy.data.objects[record['name']];rig=mesh.parent
        rig.animation_data.action=bpy.data.actions[record['actions'][0]]
        rig.location=((i//2)*1.1,(i%2)*1.5,0);rig.scale=(1,1,1)
        mesh.hide_render=mesh.name.endswith('LOD1')
        mesh.hide_set(mesh.name.endswith('LOD1'));rig.hide_set(mesh.name.endswith('LOD1'))
    scene.frame_set(1)
    render([o for o in scene.objects if o.type=='MESH' and not o.name.endswith('LOD1')],
           'Rig-Citizens.png',4.5,(1.1,0,.88),(6,-4,1.5),vertex=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(RIG_SOURCE))
    for path,expected in hashes.items():
        assert digest(ROOT/path)==expected, 'Protected accepted source changed: '+path
    manifest=dict(task='D83',schema_version=1,runtime_selected=False,assets=records,citizens=rig_records,
                  protected_inputs=hashes,source_sha256=digest(SOURCE),rig_source_sha256=digest(RIG_SOURCE),
                  shading='Authored smooth side/curve normals; flat caps and architectural edges. No custom shader.',
                  tree_detail='Simple park: single symmetric64-triangle flat crown and flat12-sided tapered bark trunk; other crowns320 triangles',
                  animation='20-bone original project humanoid; seventeen in-place Blender clips, 24fps',
                  skateboard_preview=dict(scale=BOARD_SCALE,length_m=1.1*BOARD_SCALE,deck_width_m=.35*BOARD_SCALE,
                                          wheel_span_m=.45*BOARD_SCALE,deck_top_m=BOARD_DECK,wheel_clearance_m=BOARD_CLEARANCE,
                                          citizen_height_m=1.765*.811),
                  limitations=['Blender-only prototype; not imported, built, deployed or runtime approved',
                               'Damaged and December park counterparts remain a later isolated authoring gate',
                               '32-panel ramps use quadratic height; runtime still uses old piecewise height until coordinated integration',
                               'Current runtime ISM pools cannot play skeletal clips; bounded platform animation policy needs separate design/measurement',
                               'PCVR/Quest appearance, export import policy and rendering/animation costs unverified'])
    MANIFEST.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    (EVIDENCE/'authoring.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print('CITYSIM_D83_AUTHORING_PASS',len(records),'static meshes;',len(rig_records),'rigged meshes')


if __name__ == '__main__':
    main()

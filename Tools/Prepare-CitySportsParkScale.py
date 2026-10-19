"""D88 scale derivative: retain approved skin actions; stage four cosmetic meshes."""
from pathlib import Path
import bpy,bmesh,json,hashlib,sys,copy,math
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityCitizenRigControls import update
from CityCitizenSportsMotion import palm
from CityParkGeometry import components
OUT=ROOT/'Artifacts/D88/Scale';OUT.mkdir(parents=True,exist_ok=True)
EXPORT=ROOT/'ArtSource/Exports/CitizenSportsScale';EXPORT.mkdir(parents=True,exist_ok=True)
SOURCE=ROOT/'ArtSource/Blender/CityCitizenSportsScale.blend'
REPORT=ROOT/'ArtSource/Manifests/city_citizen_sports_scale.json'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
if SOURCE.exists():
    assert '--replace-generated' in sys.argv and sha(SOURCE)==json.loads(REPORT.read_text())['source_sha256'],'Preserve artist edits'
inputs={p.relative_to(ROOT).as_posix():sha(p) for p in (ROOT/'ArtSource/Blender').glob('*.blend') if p!=SOURCE and p.name!='CityResidentialWindows.blend'}
doc=json.loads((ROOT/'ArtSource/Manifests/city_citizen_sports_refinement.json').read_text())
native=json.loads((ROOT/'ArtSource/Manifests/city_park_integration.json').read_text())
bpy.ops.wm.open_mainfile(filepath=str(ROOT/doc['source']))
def bounds(points):
    low=Vector(tuple(min(p[i] for p in points) for i in range(3)))
    high=Vector(tuple(max(p[i] for p in points) for i in range(3)));return low,high
def select(o):
    bpy.ops.object.select_all(action='DESELECT');o.hide_set(False);o.select_set(True);bpy.context.view_layer.objects.active=o
def signature(o):
    color=o.data.color_attributes.active_color;uv=o.data.uv_layers.active
    return sorted((tuple(round(v,4) for v in o.data.vertices[o.data.loops[i].vertex_index].co),
                   tuple(round(v,3) for v in color.data[i].color) if color else (),
                   tuple(round(v,4) for v in uv.data[i].uv) if uv else ()) for p in o.data.polygons for i in p.loop_indices)
def export(original,name):
    o=original.copy();o.data=original.data.copy();bpy.context.scene.collection.objects.link(o)
    o.parent=None;o.animation_data_clear();o.location=(0,0,0);o.rotation_euler=(0,0,0);o.scale=(1,1,1)
    if name.startswith('SM_Building_'):
        # The D88 source recolors WindowMask for Workbench previews. Production
        # park geometry originally has an all-white mask; retain that contract.
        mask=o.data.color_attributes.get('WindowMask');assert mask
        for c in mask.data:c.color=(1,1,1,1)
        o.data.color_attributes.active_color=mask
    expected=signature(o);select(o);path=EXPORT/(name+'.fbx')
    names={m:m.name for m in bpy.data.materials}
    for i,m in enumerate(names):m.name='_D88ScaleExport'+str(i)
    for m in o.data.materials:m.name=names[m].split('.')[0]
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},apply_unit_scale=True,
        axis_forward='-Y',axis_up='Z',bake_anim=False,mesh_smooth_type='FACE',path_mode='STRIP')
    before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(path))
    imported=set(bpy.data.objects)-before;check=next(v for v in imported if v.type=='MESH');select(check)
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    assert signature(check)==expected,('FBX geometry/color/UV readback',name)
    for v in imported:bpy.data.objects.remove(v,do_unlink=True)
    for i,m in enumerate(names):m.name='_D88ScaleRestore'+str(i)
    for m,n in names.items():m.name=n
    o.data.calc_loop_triangles();low,high=bounds([v.co for v in o.data.vertices])
    entry=dict(name=name,export=path.relative_to(ROOT).as_posix(),sha256=sha(path),triangles=len(o.data.loop_triangles),
               bounds_m=[list(low),list(high)],roundtrip_pass=True)
    bpy.data.objects.remove(o,do_unlink=True);return entry

changes={};statics=[];props=[];clips=[];old_goals=None
for record in doc['scenes']:
    s=bpy.data.scenes[record['scene']];bpy.context.window.scene=s
    field=next(o for o in s.objects if o.name.endswith('_Park'))
    kind=record['sport'];changed=[]
    if kind=='Soccer' and old_goals is None:old_goals=field.copy();old_goals.data=field.data.copy()
    for comp in components(field.data):
        low,high=bounds([field.data.vertices[i].co for i in comp]);center=(low+high)/2;size=high-low
        transform=None;label=None
        if abs(size.x-56)<.001 and abs(size.y-56)<.001:
            label='lawn';transform=lambda v:Vector((v.x,v.y,v.z*(.2/.12)))
        elif kind=='Soccer' and abs(center.z-.24)<.001 and size.z<=.061:
            label='paint';transform=lambda v:Vector((v.x,v.y,.206+(v.z-.24)*.2))
        elif kind=='Baseball' and abs(center.z-.22)<.001 and size.z>.19 and min(size.x,size.y)<.201:
            label='baseline'
            # Keep every base/foul-line endpoint, thinning only width and paint height.
            long_x=size.x>size.y
            transform=lambda v,c=center,lx=long_x:Vector((v.x if lx else c.x+(v.x-c.x)*.4,
                c.y+(v.y-c.y)*.4 if lx else v.y,.212+(v.z-.22)*.15))
        elif kind=='Soccer' and 21.9<=abs(center.x)<=24.1 and abs(center.y)<=3.1 and size.y<=6.3 and size.z>.03 and high.z<2.8 and center.z>.15:
            sign=1 if center.x>0 else -1
            # Goal mouth remains on the end line; all supports/net move together.
            anchor=Vector((sign*22,0,.2));factor=Vector((.8,.8,2/2.4));label='goal_frame'
            transform=lambda v,a=anchor,f=factor:a+Vector(tuple((v[i]-a[i])*f[i] for i in range(3)))
        elif kind=='Baseball' and len(comp)==8 and abs(size.x-.85)<.001 and abs(size.y-.85)<.001:
            anchor=Vector((center.x,center.y,.2));factor=Vector((.42/.85,.42/.85,.04/.12));label='base'
            transform=lambda v,a=anchor,f=factor:a+Vector(tuple((v[i]-a[i])*f[i] for i in range(3)))
        elif abs(abs(center.x)-6)<.01 and center.y>23 and len(comp)==8 and abs(size.x-3)<.001:
            half=28;anchor=Vector((center.x,half-3,.2));factor=Vector((.8,.6,.84));label='bench'
            transform=lambda v,a=anchor,f=factor:a+Vector(tuple((v[i]-a[i])*f[i] for i in range(3)))
        if transform:
            for i in comp:field.data.vertices[i].co=transform(field.data.vertices[i].co)
            changed.append(label)
    assert changed.count('lawn')==1 and changed.count('bench')==4 and (changed.count('base')==4 and changed.count('baseline')==6 if kind=='Baseball' else changed.count('goal_frame')==16),(kind,changed)
    field.data.update();bm=bmesh.new();bm.from_mesh(field.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(field.data);bm.free()
    if kind not in changes:
        changes[kind]=changed
        e=export(field,f'SM_Building_{kind}Park_A');e.update(asset_id='building.park.'+kind.lower(),mesh_destination='/Game/CitySim/Art/Meshes/'+e['name'])
        statics.append(e)
    rig=bpy.data.objects[record['rig']];ball=bpy.data.objects[record['ball']]
    ratio=.04/.055 if kind=='Baseball' else .11/.13
    for v in ball.data.vertices:v.co*=ratio
    delta=Vector()
    if record['clip']=='SoccerKick':
        s.frame_set(25);update(rig);toe=rig.matrix_world@rig.pose.bones['foot.R'].tail
        dz=rig.location.z+.11-toe.z;point=toe+Vector((math.sqrt(max(0,.11**2-dz**2)),0,dz));delta=point-ball.location
    def keeper_center():
        left=rig.matrix_world@palm(rig,'L');right=rig.matrix_world@palm(rig,'R');span=(left-right)/2
        assert span.length<.11,'Approved keeper hands cannot enclose the ball'
        forward=rig.matrix_world.to_quaternion()@Vector((1,0,0))
        normal=(forward-span.normalized()*forward.dot(span.normalized())).normalized()
        return (left+right)/2+normal*math.sqrt(.11**2-span.length_squared)
    if record['clip']=='GoalkeeperSave':
        s.frame_set(25);update(rig);delta=keeper_center()-ball.location
    for frame in range(1,record['period']+2):
        s.frame_set(frame);update(rig)
        if record['clip']=='SoccerKick':ball.location+=delta
        elif record['clip']=='SoccerRun':ball.location.z-=.02
        elif record['clip']=='GoalkeeperSave':
            if frame<=25:ball.location+=delta*((frame-1)/24)
            elif frame<=40:ball.location=keeper_center()
        if kind=='Soccer':ball.keyframe_insert('location',frame=frame)
    samples=[];old=next(e for e in native['clips'] if e['name']==record['clip'])
    for frame in range(1,record['period']+2):
        s.frame_set(frame);update(rig)
        def pose(obj):
            location,rotation,scale=obj.matrix_world.decompose();return dict(position=list(location*100),quaternion=list(rotation),scale=list(scale))
        body=pose(rig);previous=old['frames'][frame-1]['body']
        assert max(abs(a-b) for key in body for a,b in zip(body[key],previous[key]))<.0001,'Approved body transform changed'
        if record['clip']=='SoccerKick':
            assert ball.location.z>=rig.location.z+.11-1e-5
            if frame==25:assert abs((ball.location-(rig.matrix_world@rig.pose.bones['foot.R'].tail)).length-.11)<.001
        if record['clip']=='SoccerRun':assert abs(ball.location.z-(rig.location.z+.11))<.0001
        samples.append(dict(body=body,props=[pose(bpy.data.objects[n]) for n in record['props']]))
    clips.append(dict(name=record['clip'],frames=samples,period=record['period']))
    if not any(e['kind']==kind for e in props):
        e=export(ball,'SM_ParkNative_'+kind);e.update(kind=kind,mesh_destination='/Game/CitySim/Art/Parks/Meshes/'+e['name']);props.append(e)
    s.frame_set(1)

# A frozen keeper in its actual runtime goal position makes scale review readable.
s=bpy.data.scenes['D88_GoalkeeperSave'];bpy.context.window.scene=s;s.frame_set(25);update(bpy.data.objects[next(e for e in doc['scenes'] if e['clip']=='GoalkeeperSave')['rig']])
def gallery(name,old=False,wide=False):
    scene=bpy.data.scenes.new(name);scene.render.engine='BLENDER_WORKBENCH';scene.render.resolution_x=1200;scene.render.resolution_y=850
    scene.display.shading.color_type='VERTEX';scene.display.shading.light='STUDIO';scene.display.shading.show_shadows=False;scene.display.shading.show_cavity=True
    scene.display.shading.background_type='WORLD';scene.world=bpy.data.worlds.new(name+'_World');scene.world.color=(.065,.085,.115);scene.view_settings.view_transform='Standard'
    field=old_goals if old else next(o for o in s.objects if o.name.endswith('_Park'));scene.collection.objects.link(field)
    e=next(e for e in doc['scenes'] if e['clip']=='GoalkeeperSave')
    hero=bpy.data.objects[e['citizen']];deps=bpy.context.evaluated_depsgraph_get()
    mesh=bpy.data.meshes.new_from_object(hero.evaluated_get(deps));o=bpy.data.objects.new(name+'_Keeper',mesh);scene.collection.objects.link(o);o.matrix_world=hero.matrix_world.copy();o.location.x-=21
    ball=bpy.data.objects[e['ball']].copy();ball.data=ball.data.copy();ball.animation_data_clear();scene.collection.objects.link(ball);ball.location.x-=21
    camera=bpy.data.objects.new(name+'_Camera',bpy.data.cameras.new(name+'_Camera'));scene.collection.objects.link(camera);scene.camera=camera
    aim=Vector((0,0,0)) if wide else Vector((-22,0,1.2));camera.location=aim+Vector((7,-11,7))
    camera.rotation_euler=(aim-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=62 if wide else 8.5
    return scene
for name,old,wide in [('Goal-Before',True,False),('Goal-After',False,False),('Soccer-Field',False,True)]:gallery(name,old,wide)
text=bpy.data.texts.new('D88_SCALE_START_HERE');text.write('Approved D88 control actions retained. Sports dimensions now match citizen scale. Goal-After and Soccer-Field show keeper at actual -21m runtime position. Preserve originals/working copies. See city_citizen_sports_scale.json.\n')
bpy.context.window.scene=bpy.data.scenes['Goal-After'];bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))
assert all(sha(ROOT/p)==digest for p,digest in inputs.items())
hero=bpy.data.objects[doc['scenes'][0]['citizen']];low,high=bounds([v.co for v in hero.data.vertices]);height=(high.z-low.z)*.811
report=dict(task='D88-Scale',source=SOURCE.relative_to(ROOT).as_posix(),source_sha256=sha(SOURCE),source_inputs=inputs,statics=statics,props=props,clips=clips,
    measurements=dict(citizen_height_m=height,goal_before_m=[6,2.4,2],goal_after_m=[4.8,2,1.6],soccer_pitch_m=[44,30],base_before_m=.85,base_after_m=.42,
                      bench_after_m=[2.4,.6,.42],baseball_diameter_m=.08,soccer_ball_diameter_m=.22,bat_length_m=.85,lawn_surface_m=.2,logical_footprint_cells=[7,7]),
    approved_skin_actions_unchanged=True,prop_contact_checks=True,source_frames=293,native_selected=False)
REPORT.write_text(json.dumps(report,indent=2)+'\n')
print('CITYSIM_D88_SCALE_SOURCE_PASS 4 meshes;293 body/prop frames;approved skin actions preserved')

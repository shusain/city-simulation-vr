"""D91 isolated low-home window alternatives; production source/assets are read-only."""
from pathlib import Path
import bpy, json, hashlib, math
from mathutils import Vector
import sys
sys.path.insert(0,str(Path(__file__).absolute().parent))
from CityBuildingStudyIO import checked_fbx,window_faces
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/D91/Blender'; OUT.mkdir(parents=True,exist_ok=True)
EXPORT=ROOT/'ArtSource/Exports/HomeWindowStudy'; EXPORT.mkdir(parents=True,exist_ok=True)
SOURCE=ROOT/'ArtSource/Blender/CityResidentialSurface.blend'
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
scene=bpy.context.scene
scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=1
base=next(o for o in scene.objects if o.type=='MESH')
base.name='SM_HomeWindows_Before'
for m in base.data.materials: m.name=m.name.split('.')[0]
attr=base.data.color_attributes['WindowMask']
windows=[p.index for p in base.data.polygons if all(attr.data[i].color[3]<.01 for i in p.loop_indices)]
assert len(windows)==11
base.data.calc_loop_triangles(); original_triangles=len(base.data.loop_triangles)

# Small authored sky/room suggestion. This is an opaque illustration of glass,
# not a scene-dependent reflection or an interior. No external image input.
tex=bpy.data.images.new('HomeGlass',width=128,height=128,alpha=True)
pixels=[]
for y in range(128):
    v=y/127
    for x in range(128):
        u=x/127; stripe=.055 if .45<u*.65+v<.58 or .84<u*.65+v<.89 else 0
        pixels.extend((.075+.10*v+stripe,.13+.16*v+stripe,.19+.19*v+stripe,1))
tex.pixels=pixels
texture_path=ROOT/'ArtSource/Textures/HomeWindowStudy/HomeGlass.png'
texture_path.parent.mkdir(parents=True,exist_ok=True)
tex.filepath_raw=str(texture_path);tex.file_format='PNG';tex.save();tex.pack()

def assembly_material(name,transparent=False):
    m=bpy.data.materials.new(name);m.use_nodes=True
    n=m.node_tree.nodes;l=m.node_tree.links;p=n.get('Principled BSDF')
    a=n.new('ShaderNodeVertexColor');a.layer_name='WindowMask'
    t=n.new('ShaderNodeTexImage');t.image=tex
    mix=n.new('ShaderNodeMixRGB');mix.inputs[2].default_value=(.76,.73,.64,1)
    l.new(t.outputs['Color'],mix.inputs[1]);l.new(a.outputs['Alpha'],mix.inputs[0]);l.new(mix.outputs[0],p.inputs['Base Color'])
    rough=n.new('ShaderNodeMapRange');rough.inputs['From Min'].default_value=0;rough.inputs['From Max'].default_value=1
    rough.inputs['To Min'].default_value=.18;rough.inputs['To Max'].default_value=.55
    l.new(a.outputs['Alpha'],rough.inputs[0]);l.new(rough.outputs[0],p.inputs['Roughness'])
    p.inputs['IOR'].default_value=1.45
    p.inputs['Transmission Weight'].default_value=0 # Same alpha blend experiment as Unreal, no wall apertures.
    if transparent:
        opacity=n.new('ShaderNodeMapRange');opacity.inputs['To Min'].default_value=.35;opacity.inputs['To Max'].default_value=1
        l.new(a.outputs['Alpha'],opacity.inputs[0]);l.new(opacity.outputs[0],p.inputs['Alpha'])
    night=n.new('ShaderNodeValue');night.name='NightGlow';night.outputs[0].default_value=0
    inv=n.new('ShaderNodeMath');inv.operation='SUBTRACT';inv.inputs[0].default_value=1;l.new(a.outputs['Alpha'],inv.inputs[1])
    amount=n.new('ShaderNodeMath');amount.operation='MULTIPLY';l.new(inv.outputs[0],amount.inputs[0]);l.new(night.outputs[0],amount.inputs[1])
    p.inputs['Emission Color'].default_value=(.8,.46,.16,1);l.new(amount.outputs[0],p.inputs['Emission Strength'])
    return m

def add_frames(obj):
    mesh=obj.data; colors=mesh.color_attributes['WindowMask'];uv=mesh.uv_layers.active
    added_vertices=[];added_faces=[]; specs=[]
    for pi in windows:
        p=mesh.polygons[pi];coords=[mesh.vertices[i].co.copy() for i in p.vertices]
        origin=coords[0];right=(coords[1]-origin).normalized();up=Vector((0,0,1));normal=right.cross(up)
        w=(coords[1]-origin).length;h=coords[3].z-origin.z
        p.material_index=3
        for li in p.loop_indices:
            c=mesh.vertices[mesh.loops[li].vertex_index].co-origin
            uv.data[li].uv=(c.dot(right)/w,c.z/h)
        def quad(points):
            start=len(added_vertices)
            added_vertices.extend([tuple(origin+right*x+up*z+normal*d) for x,z,d in points])
            added_faces.append(tuple(range(start,start+4)))
        # Chamfered outer surround plus recessed inner edge, all inside pane bounds.
        outer=[(0,0),(w,0),(w,h),(0,h)]
        inner=[(.12,.12),(w-.12,.12),(w-.12,h-.12),(.12,h-.12)]
        for i in range(4):
            j=(i+1)%4
            quad([(*outer[i],.015),(*outer[j],.015),(*inner[j],.095),(*inner[i],.095)])
            quad([(*inner[i],.095),(*inner[j],.095),(*inner[j],.008),(*inner[i],.008)])
        # Slim cross bars; four panes remain legible at the house scale.
        for x0,x1,z0,z1 in ((w/2-.035,w/2+.035,.12,h-.12),(.12,w/2-.035,h/2-.035,h/2+.035),(w/2+.035,w-.12,h/2-.035,h/2+.035)):
            quad([(x0,z0,.045),(x1,z0,.045),(x1,z1,.045),(x0,z1,.045)])
        # A projecting sill with top/front/ends (bottom/back are unseen).
        for pts in (((-.04,-.10,.015),(w+.04,-.10,.015),(w+.04,-.10,.18),(-.04,-.10,.18)),
                    ((-.04,-.10,.18),(w+.04,-.10,.18),(w+.04,.02,.18),(-.04,.02,.18)),
                    ((-.04,.02,.18),(w+.04,.02,.18),(w+.04,.02,.015),(-.04,.02,.015)),
                    ((-.04,-.10,.015),(-.04,-.10,.18),(-.04,.02,.18),(-.04,.02,.015)),
                    ((w+.04,-.10,.18),(w+.04,-.10,.015),(w+.04,.02,.015),(w+.04,.02,.18))):quad(pts)
        specs.append(dict(polygon=pi,width_m=w,height_m=h,normal=list(normal)))
    frame_mesh=bpy.data.meshes.new('WindowFrames');frame_mesh.from_pydata(added_vertices,[],added_faces);frame_mesh.update()
    frame_uv=frame_mesh.uv_layers.new(name='UVMap')
    for p in frame_mesh.polygons:
        axis=max(range(3),key=lambda i:abs(p.normal[i]));axes=[i for i in range(3) if i!=axis]
        for li in p.loop_indices:
            c=frame_mesh.vertices[frame_mesh.loops[li].vertex_index].co
            frame_uv.data[li].uv=(c[axes[0]],c[axes[1]])
    frames=bpy.data.objects.new('WindowFrames',frame_mesh);scene.collection.objects.link(frames)
    frame_mesh.materials.append(obj.data.materials[3])
    ca=frame_mesh.color_attributes.new(name='WindowMask',type='BYTE_COLOR',domain='CORNER')
    for c in ca.data:c.color=(1,1,1,1)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);frames.select_set(True);bpy.context.view_layer.objects.active=obj;bpy.ops.object.join()
    return specs

opaque=base.copy();opaque.data=base.data.copy();scene.collection.objects.link(opaque);opaque.name='SM_HomeWindows_Opaque'
opaque.data.materials.append(assembly_material('HomeWindowOpaque'));specs=add_frames(opaque)
trans=opaque.copy();trans.data=opaque.data.copy();scene.collection.objects.link(trans);trans.name='SM_HomeWindows_Translucent'
trans.data.materials[3]=assembly_material('HomeWindowTranslucent',True)
objects=[base,opaque,trans]
assets=[]
for obj in objects:
    path=EXPORT/(obj.name+'.fbx')
    with checked_fbx(obj,path,dimension_tolerance=.001) as check:
        assert window_faces(check)==11
    obj.data.calc_loop_triangles()
    assets.append(dict(name=obj.name,export=path.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        triangles=len(obj.data.loop_triangles),sections=len(obj.data.materials),slots=[m.name for m in obj.data.materials],dimensions_m=list(obj.dimensions),roundtrip_pass=True))

# Render the saved materials on the real current display-house geometry.
scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True
scene.render.resolution_x=1200;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX';scene.world.use_nodes=True
background=scene.world.node_tree.nodes.get('Background');background.inputs['Color'].default_value=(.28,.34,.44,1)
background.inputs['Strength'].default_value=.7
sun_data=bpy.data.lights.new('StudySun','SUN');sun_data.energy=3;sun_data.angle=.12
sun=bpy.data.objects.new('StudySun',sun_data);scene.collection.objects.link(sun);sun.rotation_euler=(.5,-.45,-.6)
cam_data=bpy.data.cameras.new('StudyCamera');cam_data.type='ORTHO'
cam=bpy.data.objects.new('StudyCamera',cam_data);scene.collection.objects.link(cam);scene.camera=cam
views=[]
for obj in objects:
    for o in objects:o.hide_render=o!=obj
    for label,aim,offset,scale in [('Whole',(0,-.5,3.8),(23,-32,19),22),('Window',(3.65,-6.19,3.4),(2,-7,1.6),4.5)]:
        target=Vector(aim);cam.location=target+Vector(offset);cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam_data.ortho_scale=scale
        path=OUT/(obj.name+'-'+label+'.png');scene.render.filepath=str(path);bpy.ops.render.render(write_still=True);views.append(path.relative_to(ROOT).as_posix())
for o in objects:
    o.hide_render=o!=opaque
    o.hide_set(o!=opaque)
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'ArtSource/Blender/CityResidentialWindows.blend'))
manifest=dict(task='D91',prototype_only=True,source=SOURCE.relative_to(ROOT).as_posix(),source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
    assets=assets,window_count=11,frames=specs,views=views,texture=texture_path.relative_to(ROOT).as_posix(),
    original_triangles=original_triangles,notes='Original solid walls, porch, door, roof, walk, bounds and night pane masks retained. New four-pane surrounds/sills. Translucency35% experiment; opaque illustrated glass candidate.')
(ROOT/'ArtSource/Manifests/city_home_window_study.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('CITYSIM_D91_BLENDER_PASS',len(views),'renders; three FBX roundtrips')

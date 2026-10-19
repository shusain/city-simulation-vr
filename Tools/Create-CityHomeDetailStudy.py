"""D91 door/chimney refinement on the user-accepted opaque-window home."""
from pathlib import Path
import bpy,json,hashlib,math
from mathutils import Vector
import sys
sys.path.insert(0,str(Path(__file__).absolute().parent))
from CityBuildingStudyIO import checked_fbx,window_faces
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/D91/Details/Blender';OUT.mkdir(parents=True,exist_ok=True)
EXPORT=ROOT/'ArtSource/Exports/HomeDetailStudy';EXPORT.mkdir(parents=True,exist_ok=True)
SOURCE=ROOT/'ArtSource/Blender/CityResidentialWindows.blend'
bpy.ops.wm.open_mainfile(filepath=str(SOURCE));scene=bpy.context.scene
baseline=bpy.data.objects['SM_HomeWindows_Opaque']
for o in list(scene.objects):
    if o.type=='MESH' and o!=baseline:bpy.data.objects.remove(o,do_unlink=True)
baseline.name='SM_HomeDetails_WindowsOnly';baseline.hide_set(False)
after=baseline.copy();after.data=baseline.data.copy();scene.collection.objects.link(after);after.name='SM_HomeDetails_Refined'
mesh=after.data;ca=mesh.color_attributes['WindowMask']

# One opaque solid-detail surface replaces the existing solid trim section.
# Alpha .5 selects brick; alpha1 selects flat paint/stone/brass color. This
# material has no emission. The original window section/masks stay untouched.
solid=bpy.data.materials.new('HomeSolidDetails');solid.use_nodes=True
n=solid.node_tree.nodes;l=solid.node_tree.links;p=n.get('Principled BSDF')
vertex=n.new('ShaderNodeVertexColor');vertex.layer_name='WindowMask'
image=n.new('ShaderNodeTexImage');image.image=bpy.data.images.load(str(ROOT/'ArtSource/Textures/BuildingSurfaces/CityBrick_BaseColor.png'),check_existing=True)
mul=n.new('ShaderNodeMixRGB');mul.blend_type='MULTIPLY';mul.inputs[0].default_value=1
l.new(vertex.outputs['Color'],mul.inputs[1]);l.new(image.outputs['Color'],mul.inputs[2])
mask=n.new('ShaderNodeMath');mask.operation='SUBTRACT';mask.inputs[0].default_value=1;l.new(vertex.outputs['Alpha'],mask.inputs[1])
double=n.new('ShaderNodeMath');double.operation='MULTIPLY';double.inputs[1].default_value=2;l.new(mask.outputs[0],double.inputs[0])
mix=n.new('ShaderNodeMixRGB');l.new(vertex.outputs['Color'],mix.inputs[1]);l.new(mul.outputs[0],mix.inputs[2]);l.new(double.outputs[0],mix.inputs[0]);l.new(mix.outputs[0],p.inputs['Base Color'])
rough=n.new('ShaderNodeMapRange');rough.inputs['To Min'].default_value=.55;rough.inputs['To Max'].default_value=.8
l.new(double.outputs[0],rough.inputs[0]);l.new(rough.outputs[0],p.inputs['Roughness'])
mesh.materials[1]=solid
trim=(.10,.14,.19,1);paint=(.065,.17,.22,1);brass=(.62,.40,.12,1);stone=(.48,.46,.40,1);metal=(.055,.065,.075,1)
uv=mesh.uv_layers.active
for face in mesh.polygons:
    if face.material_index!=1:continue
    color=paint if 23<=face.index<=28 else trim
    if 11<=face.index<=16:color=(.56,.22,.105,.5)
    for li in face.loop_indices:
        ca.data[li].color=color
        if 11<=face.index<=16:
            c=mesh.vertices[mesh.loops[li].vertex_index].co;axis=max(range(3),key=lambda i:abs(face.normal[i]));axes=[i for i in range(3) if i!=axis]
            uv.data[li].uv=(c[axes[0]]/1.2,c[axes[1]]/1.2)
# Keep the accepted9m high envelope; make room for the collar and vent hood.
chimney_vertices={i for f in list(mesh.polygons)[11:17] for i in f.vertices}
changed=[]
for vi in chimney_vertices:
    if mesh.vertices[vi].co.z>8.99:mesh.vertices[vi].co.z=8.65;changed.append(vi)

verts=[];faces=[];colors=[];parts=[]
def quad(points,color):
    start=len(verts);verts.extend(points);faces.append(tuple(range(start,start+4)));colors.append(color)
def box(name,center,size,color):
    x,y,z=center;a,b,c=[v/2 for v in size]
    points=[(x-a,y-b,z-c),(x+a,y-b,z-c),(x+a,y+b,z-c),(x-a,y+b,z-c),
            (x-a,y-b,z+c),(x+a,y-b,z+c),(x+a,y+b,z+c),(x-a,y+b,z+c)]
    for idx in ((0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)):quad([points[i] for i in idx],color)
    parts.append(dict(name=name,center=list(center),size=list(size)))

# Door dimensions remain2.64x4m. Matching pale frame is assigned the existing
# window section after joining, so paint/stone/metal share the original trim.
frame=(.76,.73,.64,1)
box('DoorLeftJamb',(-1.34,-6.48,1.99),(.16,.15,3.98),frame)
box('DoorRightJamb',(1.34,-6.48,1.99),(.16,.15,3.98),frame)
box('DoorLintel',(0,-6.48,4.06),(2.84,.15,.16),frame)
box('DoorThreshold',(0,-6.55,.025),(2.55,.30,.05),stone)
for x0,x1 in ((-1.12,-.12),(.12,1.12)):
    for z0,z1 in ((.32,1.86),(2.15,3.72)):
        outer=[(x0,z0),(x1,z0),(x1,z1),(x0,z1)]
        inner=[(x0+.075,z0+.075),(x1-.075,z0+.075),(x1-.075,z1-.075),(x0+.075,z1-.075)]
        for i in range(4):
            j=(i+1)%4
            quad([(outer[i][0],-6.435,outer[i][1]),(outer[j][0],-6.435,outer[j][1]),(inner[j][0],-6.49,inner[j][1]),(inner[i][0],-6.49,inner[i][1])],paint)
        quad([(x,-6.49,z) for x,z in inner],(.09,.205,.25,1))
box('DoorKnobBackplate',(.98,-6.515,2.0),(.13,.035,.28),brass)
# Eight-sided round knob; its front disk and side band add28 triangles.
cx,cz=.98,2.;radius=.075
ring=[(cx+radius*math.cos(i*math.tau/8),cz+radius*math.sin(i*math.tau/8)) for i in range(8)]
for i in range(8):
    j=(i+1)%8
    quad([(ring[i][0],-6.535,ring[i][1]),(ring[j][0],-6.535,ring[j][1]),(ring[j][0],-6.635,ring[j][1]),(ring[i][0],-6.635,ring[i][1])],brass)
# Disks are triangulated at export, with correct winding for the -Y front.
start=len(verts);verts.extend((x,-6.635,z) for x,z in ring);faces.append(tuple(range(start,start+8)));colors.append(brass)
box('LetterPlate',(0,-6.455,2.015),(.55,.045,.11),brass)

# Flashing follows the existing pitched-roof surface, rather than floating
# horizontally above it or intersecting its visible upper side.
center=(3.696,2.64)
for label,x0,x1,y0,y1 in (
    ('Front',2.78,4.61,1.70,1.85),('Back',2.78,4.61,3.43,3.58),
    ('Left',2.78,2.904,1.85,3.43),('Right',4.488,4.61,1.85,3.43)):
    points=[(x0,y0),(x1,y0),(x1,y1),(x0,y1)]
    quad([(x,y,8-x*(2/7.04)+.025) for x,y in points],metal)
    parts.append(dict(name='RoofFlashing'+label,roof_clearance_m=.025))
box('ChimneyCollar',(center[0],center[1],8.69),(1.79,1.79,.18),stone)
box('ChimneyFlueInset',(center[0],center[1],8.795),(.70,.70,.03),metal)
for x in (3.16,4.232):
    for y in (2.10,3.18):box('VentCapSupport',(x,y,8.86),(.07,.07,.16),metal)
box('ChimneyVentHood',(center[0],center[1],8.95),(1.44,1.44,.10),metal)

detail_mesh=bpy.data.meshes.new('DoorChimneyDetails');detail_mesh.from_pydata(verts,[],faces);detail_mesh.update()
detail=bpy.data.objects.new('DoorChimneyDetails',detail_mesh);scene.collection.objects.link(detail)
detail_mesh.materials.append(solid)
a=detail_mesh.color_attributes.new(name='WindowMask',type='BYTE_COLOR',domain='CORNER');duv=detail_mesh.uv_layers.new(name='UVMap')
for face,color in zip(detail_mesh.polygons,colors):
    axis=max(range(3),key=lambda i:abs(face.normal[i]));axes=[i for i in range(3) if i!=axis]
    for li in face.loop_indices:
        a.data[li].color=color;c=detail_mesh.vertices[detail_mesh.loops[li].vertex_index].co;duv.data[li].uv=(c[axes[0]],c[axes[1]])
bpy.ops.object.select_all(action='DESELECT');after.select_set(True);detail.select_set(True);bpy.context.view_layer.objects.active=after;bpy.ops.object.join()
assert len(after.data.materials)==4
baseline_vertices=len(baseline.data.vertices);baseline_faces=len(baseline.data.polygons)
# The first18 added quads are the three cream doorway bars.
for f in list(after.data.polygons)[baseline_faces:baseline_faces+18]:f.material_index=3

# Scope check: all accepted window panes/frames and unrelated body/roof/walk
# geometry stay byte-equivalent in the saved Blender mesh data.
checks=[]
for f in baseline.data.polygons:
    if 11<=f.index<=16 or 23<=f.index<=28:continue
    g=after.data.polygons[f.index];assert list(f.vertices)==list(g.vertices)
    assert all((baseline.data.vertices[i].co-after.data.vertices[i].co).length<1e-6 for i in f.vertices)
    assert all((baseline.data.uv_layers.active.data[i].uv-after.data.uv_layers.active.data[i].uv).length<1e-6 for i in f.loop_indices)
    if f.material_index==3:
        assert g.material_index==3
        assert all(tuple(baseline.data.color_attributes['WindowMask'].data[i].color)==tuple(after.data.color_attributes['WindowMask'].data[i].color) for i in f.loop_indices)
    checks.append(f.index)
assert len(changed)==4
assets=[]
for obj in (baseline,after):
    path=EXPORT/(obj.name+'.fbx')
    with checked_fbx(obj,path,dimension_tolerance=.001,expected_sections=4) as readback:
        assert window_faces(readback)==11
    obj.data.calc_loop_triangles()
    entry=dict(name=obj.name,export=path.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),triangles=len(obj.data.loop_triangles),sections=4,slots=[m.name for m in obj.data.materials],dimensions_m=list(obj.dimensions),roundtrip_pass=True)
    assets.append(entry)

scene.cycles.samples=24;scene.camera=bpy.data.objects['StudyCamera']
views=[]
for obj in (baseline,after):
    for o in (baseline,after):o.hide_render=o!=obj;o.hide_set(False)
    for label,aim,offset,scale in [('Whole',(0,-.5,3.8),(23,-32,19),22),('Door',(0,-6.5,2.1),(2,-8,1),5.6),('Chimney',(3.696,2.64,8),(5,-8,4),4.7)]:
        target=Vector(aim);cam=scene.camera;cam.location=target+Vector(offset);cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=scale
        path=OUT/(obj.name+'-'+label+'.png');scene.render.filepath=str(path);bpy.ops.render.render(write_still=True);views.append(path.relative_to(ROOT).as_posix())
for o in (baseline,after):o.hide_render=o!=after;o.hide_set(o!=after)
scene.camera.data.ortho_scale=22;aim=Vector((0,-.5,3.8));scene.camera.location=aim+Vector((23,-32,19));scene.camera.rotation_euler=(aim-scene.camera.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'ArtSource/Blender/CityResidentialDetails.blend'))
doc=dict(task='D91',slice='DoorChimney',prototype_only=True,source=SOURCE.relative_to(ROOT).as_posix(),source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
    assets=assets,views=views,window_count=11,preserved_window_material='HomeWindowOpaque',parts=parts,chimney_vertices_changed=changed,preserved_faces=checks,
    detail_material='HomeSolidDetails',texture='ArtSource/Textures/HomeWindowStudy/HomeGlass.png',notes='Four painted bevel panels, cream door frame, threshold, brass-colored knob/letter plate; terracotta brick chimney, slope-matched flashing, stone collar, dark flue/raised ventilated hood. Four sections; no interiors or transparency.')
(ROOT/'ArtSource/Manifests/city_home_detail_study.json').write_text(json.dumps(doc,indent=2)+'\n')
print('CITYSIM_D91_HOME_DETAILS_BLENDER_PASS 6 renders; 2 FBX roundtrips;',assets[1]['triangles'],'triangles; 4 sections')

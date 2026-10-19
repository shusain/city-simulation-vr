"""D91 citizen-aligned bungalow derivative; preserve accepted detail/source studies."""
from pathlib import Path
import bpy,json,hashlib,math
from mathutils import Vector
import sys
sys.path.insert(0,str(Path(__file__).absolute().parent))
from CityBuildingStudyIO import checked_fbx,window_faces
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D91/Scale';OUT.mkdir(parents=True,exist_ok=True)
PREVIEW=OUT/'Blender';PREVIEW.mkdir(exist_ok=True)
EXPORT=ROOT/'ArtSource/Exports/HomeScaleStudy';EXPORT.mkdir(parents=True,exist_ok=True)
SOURCE=ROOT/'ArtSource/Blender/CityResidentialDetails.blend'
bpy.ops.wm.open_mainfile(filepath=str(SOURCE));scene=bpy.context.scene
before=bpy.data.objects['SM_HomeDetails_Refined'];before.name='SM_HomeScale_Before';before.hide_set(False)
for o in list(scene.objects):
    if o.type=='MESH' and o!=before:bpy.data.objects.remove(o,do_unlink=True)
after=before.copy();after.data=before.data.copy();scene.collection.objects.link(after);after.name='SM_HomeScale_Refined'
mesh=after.data;assert len(mesh.polygons)==333
used=set()
def transform_faces(indices,transform):
    indices=set(i for fi in indices for i in mesh.polygons[fi].vertices)
    assert not (indices&used),'A shared vertex would receive two transforms'
    for i in indices:mesh.vertices[i].co=transform(mesh.vertices[i].co.copy())
    used.update(indices)
xy=.75
transform_faces(range(0,6),lambda c:Vector((c.x*xy,c.y*xy,c.z*.5)))
transform_faces(range(6,11),lambda c:Vector((c.x*xy,c.y*xy,3+(c.z-6)*.8)))
transform_faces(range(11,17),lambda c:Vector((c.x*xy,c.y*xy,3+(c.z-6)*(2.25/2.65))))
# The logical16m lot stays fixed; the narrower walk still reaches its -8m edge.
transform_faces(range(17,23),lambda c:Vector((c.x*(1.3/3),-8+(c.y+8)*(3.38/1.84),c.z)))
def door(c):return Vector((c.x*(1.1/2.64),-4.62+(c.y+6.16)*(.1/.264),c.z*(1.85/4)))
transform_faces(range(23,29),door)
transform_faces(range(40,46),lambda c:Vector((c.x*xy,c.y*xy,c.z*.5)))
for wi in range(11):
    face=before.data.polygons[29+wi]
    coords=[before.data.vertices[i].co.copy() for i in face.vertices]
    center=sum(coords,Vector())/4;right=(coords[1]-coords[0]).normalized();up=Vector((0,0,1));normal=right.cross(up)
    target=Vector((center.x*xy,center.y*xy,1.4))
    def window(c,center=center,target=target,right=right,normal=normal):
        d=c-center
        return target+right*(d.dot(right)*(1.4/1.936))+up*(d.z*(1.3/1.8))+normal*(d.dot(normal)*xy)
    transform_faces([29+wi]+list(range(46+wi*16,46+(wi+1)*16)),window)
transform_faces(range(222,266),door) # jambs/threshold and four panels
def fittings(c):
    d=door(c);d.z-=.075
    return d
transform_faces(range(266,287),fittings) # knob, backplate and letter plate
# Flashing is fitted to the new roof slope at the new chimney location.
transform_faces(range(287,291),lambda c:Vector((c.x*xy,c.y*xy,4.6-(c.x*xy)*(1.6/5.28)+.025)))
transform_faces(range(291,333),lambda c:Vector((c.x*xy,c.y*xy,c.z-3.4)))
assert len(used)==len(mesh.vertices)
mesh.update()
uv=mesh.uv_layers.active
for face in mesh.polygons:
    if face.index in list(range(6))+list(range(11,17)):
        axis=max(range(3),key=lambda i:abs(face.normal[i]));sign=1 if face.normal[axis]>0 else -1
        for li in face.loop_indices:
            c=mesh.vertices[mesh.loops[li].vertex_index].co
            a,b=(sign*c.y,c.z) if axis==0 else (-sign*c.x,c.z) if axis==1 else (sign*c.x,c.y)
            uv.data[li].uv=(a/2.4,b/2.4)
    elif face.material_index==2:
        tangent=face.normal.cross(Vector((1,0,0))).normalized()
        for li in face.loop_indices:
            c=mesh.vertices[mesh.loops[li].vertex_index].co;uv.data[li].uv=(c.x/4.8,c.dot(tangent)/4.8)

# Keep both real meshes at their actual (or chosen new) runtime scales. Feet
# share the studio ground for a proportion comparison, not a fabricated pose.
citizen_inputs=[dict(kind='Street',export='ArtSource/Exports/CitizenPresentation/SM_Citizen_PC_Casual.fbx',
    native_mesh='/Game/CitySim/Art/PC/Meshes/SM_Citizen_PC_Casual',scale_before=.72,scale_after=1.15,x=-1.8),
    dict(kind='Park',export='ArtSource/Exports/HomeScaleStudy/Reference_Park_Field.fbx',
    native_mesh='/Game/CitySim/Art/Parks/Meshes/SM_Park_Field_Casual',scale_before=.811,scale_after=.811,x=1.8)]
figures=[]
for record in citizen_inputs:
    bpy.ops.import_scene.fbx(filepath=str(ROOT/record['export']));obj=next(o for o in bpy.context.selected_objects if o.type=='MESH')
    bpy.context.view_layer.objects.active=obj;bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    obj.name='ScaleReference_'+record['kind'];obj.hide_set(False)
    colors=obj.data.color_attributes['CitizenPalette'];material=bpy.data.materials.new('ScaleCitizen_'+record['kind']);material.use_nodes=True
    a=material.node_tree.nodes.new('ShaderNodeVertexColor');a.layer_name='CitizenPalette';p=material.node_tree.nodes.get('Principled BSDF')
    material.node_tree.links.new(a.outputs['Color'],p.inputs['Base Color']);p.inputs['Roughness'].default_value=.78
    obj.data.materials.clear();obj.data.materials.append(material)
    for f in obj.data.polygons:f.material_index=0
    low=min(v.co.z for v in obj.data.vertices);high=max(v.co.z for v in obj.data.vertices)
    record.update(source_min_z_m=low,source_height_m=high-low,height_before_m=(high-low)*record['scale_before'],height_after_m=(high-low)*record['scale_after'],sha256=hashlib.sha256((ROOT/record['export']).read_bytes()).hexdigest())
    figures.append(obj)
assets=[]
for obj in (before,after):
    path=EXPORT/(obj.name+'.fbx')
    with checked_fbx(obj,path,expected_triangles=668,expected_sections=4) as check:
        assert window_faces(check)==11
    obj.data.calc_loop_triangles()
    e=dict(name=obj.name,export=path.relative_to(ROOT).as_posix(),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),triangles=len(obj.data.loop_triangles),sections=4,slots=[m.name for m in obj.data.materials],dimensions_m=list(obj.dimensions),roundtrip_pass=True)
    assets.append(e)

bpy.ops.mesh.primitive_plane_add(size=100,location=(0,0,-.21))
ground=bpy.context.object;ground.name='ScaleReviewGround'
ground_material=bpy.data.materials.new('ScaleReviewGround');ground_material.use_nodes=True
ground_material.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.16,.18,.22,1)
ground_material.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value=.85
ground.data.materials.append(ground_material)
views=[];cam=scene.camera
for obj in (before,after):
    changed=obj==after
    for o in (before,after):o.hide_render=o!=obj;o.hide_set(False)
    front=-4.62 if changed else -6.16
    for figure,record in zip(figures,citizen_inputs):
        factor=record['scale_after' if changed else 'scale_before'];figure.scale=(factor,)*3
        figure.location=(record['x'],front-.9,-.20-record['source_min_z_m']*factor);figure.rotation_euler.z=math.pi/2
    for label,aim,offset,width in [('Whole',(0,-.5,3),(23,-32,19),22),('Entrance',(0,front,1.65),(2,-10,2),8.7),
        ('Chimney',(2.772,1.98,4.8) if changed else (3.696,2.64,8),(5,-8,4),5.2)]:
        target=Vector(aim);cam.location=target+Vector(offset);cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=width
        path=PREVIEW/(obj.name+'-'+label+'.png');scene.render.filepath=str(path);bpy.ops.render.render(write_still=True);views.append(path.relative_to(ROOT).as_posix())
for o in (before,after):o.hide_render=o!=after;o.hide_set(o!=after)
aim=Vector((0,-.5,3));cam.location=aim+Vector((23,-32,19));cam.rotation_euler=(aim-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=22
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'ArtSource/Blender/CityResidentialScale.blend'))
result=dict(task='D91',slice='CitizenScale',prototype_only=True,source=SOURCE.relative_to(ROOT).as_posix(),source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
    assets=assets,views=views,citizens=citizen_inputs,texture='ArtSource/Textures/HomeWindowStudy/HomeGlass.png',
    dimensions=dict(door_before_m=[2.64,4],door_after_m=[1.1,1.85],knob_height_m=.85,window_before_m=[1.936,1.8],window_after_m=[1.4,1.3],window_sill_m=.75,eaves_m=3,roof_ridge_m=4.6,chimney_top_m=5.6,wall_width_m=9.24,logical_lot_m=[16,16],brick_repeat_m=2.4),
    street_origin_offset_cm=89.625,street_foot_support_cm=104,notes='User accepts door design/chimney geometry; resize house and street citizens coherently. Shared wall/chimney brick2.4m UV repeat. Citizen meshes/park transforms and source studies preserved; no broader building adoption.')
(ROOT/'ArtSource/Manifests/city_home_scale_study.json').write_text(json.dumps(result,indent=2)+'\n')
print('CITYSIM_D91_SCALE_BLENDER_PASS 6 renders; 2 FBX roundtrips; 668 triangles/4 sections')

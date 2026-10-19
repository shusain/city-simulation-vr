"""D66 checkerboard fix: original centered unit foundation with solid lawn top.

Blender --background --factory-startup --python this-file saves a separate
source/manifest and renders the unit and scaled lot. Review before -- --export.
No previous Blender source, FBX, material or house driveway is changed.
"""
from pathlib import Path
import bpy, bmesh, json, sys
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
NAME = 'SM_Building_LotFoundation_A'
SOURCE = ROOT / 'ArtSource/Blender/CityResidentialGround.blend'
PREVIEW = ROOT / 'Artifacts/ArtPreview/D66-ResidentialGround-Blender.png'
MANIFEST = ROOT / 'ArtSource/Manifests/city_residential_ground.json'
SLOTS = {'Lawn':'/Game/CitySim/Art/Materials/MI_CityPark',
         'Concrete':'/Game/CitySim/Art/Materials/MI_CityTraffic'}
COLORS = {'Lawn':(.16,.58,.20),'Concrete':(.88,.88,.86)}
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.unit_settings.system='METRIC'
scene.unit_settings.scale_length=1.0
bpy.ops.mesh.primitive_cube_add(size=1,location=(0,0,0))
mesh=bpy.context.object
mesh.name=NAME
mesh.data.name=NAME
for slot,color in COLORS.items():
    material=bpy.data.materials.new(slot)
    material.diffuse_color=(*color,1)
    material.use_nodes=True
    shader=material.node_tree.nodes['Principled BSDF']
    shader.inputs['Base Color'].default_value=(*color,1)
    shader.inputs['Roughness'].default_value=.85
    mesh.data.materials.append(material)
for face in mesh.data.polygons:
    face.material_index=0 if face.normal.z>.9 else 1
    face.use_smooth=False
mesh.data.calc_loop_triangles()
bm=bmesh.new();bm.from_mesh(mesh.data)
assert not any(edge.is_boundary for edge in bm.edges)
assert not any(face.calc_area()<1e-8 for face in bm.faces)
bm.free()
assert len(mesh.data.loop_triangles)==12 and len(mesh.data.uv_layers)==1
assert sum(face.material_index==0 for face in mesh.data.polygons)==1
assert all(abs(value-1)<1e-6 for value in mesh.dimensions)
entry=dict(name=NAME,asset_id='presentation.lot_foundation',
    source='ArtSource/Blender/CityResidentialGround.blend',
    export='ArtSource/Exports/'+NAME+'.fbx',
    mesh_destination='/Game/CitySim/Art/Meshes/'+NAME,
    dimensions_m=[1,1,1],bounds_min_m=[-.5,-.5,-.5],bounds_max_m=[.5,.5,.5],
    pivot_policy='centered unit cube; same transform convention as Engine/BasicShapes/Cube',
    material_slots=SLOTS,top_normal_authored=[0,0,1],top_triangles=2,side_bottom_triangles=10,
    triangles=12,collision_policy='none; decorative pooled foundation only',
    lod_policy='single twelve-triangle cube; per-building pooled instancing',
    footprint_policy='runtime scales existing authoritative footprint; no new logical cells',
    provenance='Original project Blender unit cube; Tools/Create-CityResidentialGround.py')
MANIFEST.parent.mkdir(parents=True,exist_ok=True)
MANIFEST.write_text(json.dumps(dict(schema_version=1,task='D66',assets=[entry]),indent=2)+'\n',encoding='utf-8')
if '--export' in sys.argv:
    assert PREVIEW.exists(),'Review rendered unit/scaled lot before export'
    bpy.ops.export_scene.fbx(filepath=str(ROOT/entry['export']),use_selection=True,
        object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,
        bake_anim=False,add_leaf_bones=False)
else:
    SOURCE.parent.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE))
    PREVIEW.parent.mkdir(parents=True,exist_ok=True)
    mesh.location=(-9,0,.5)
    lot=mesh.copy();lot.data=mesh.data.copy();scene.collection.objects.link(lot)
    lot.name='Preview16mLot';lot.scale=(16,16,.40);lot.location=(3,0,.20)
    bpy.ops.mesh.primitive_plane_add(size=50,location=(0,0,-.025))
    ground=bpy.context.object
    material=bpy.data.materials.new('PreviewGround');material.use_nodes=True
    material.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.20,.22,.23,1)
    ground.data.materials.append(material)
    scene.render.engine='BLENDER_EEVEE'
    scene.render.resolution_x=1200;scene.render.resolution_y=700;scene.render.resolution_percentage=100
    scene.world.color=(.18,.18,.18);scene.view_settings.view_transform='Standard'
    bpy.ops.object.light_add(type='AREA',location=(0,-12,22))
    bpy.context.object.data.energy=8000;bpy.context.object.data.size=25
    bpy.ops.object.camera_add(location=(19,-27,19))
    camera=bpy.context.object
    camera.rotation_euler=(Vector((0,0,.30))-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.type='ORTHO';camera.data.ortho_scale=30;scene.camera=camera
    scene.render.filepath=str(PREVIEW);bpy.ops.render.render(write_still=True)
print('CITYSIM_RESIDENTIAL_GROUND_ART_READY',json.dumps(entry))

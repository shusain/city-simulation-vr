"""#125: derive one pathless Compact Home, preserving approved source geometry.

Blender --background --factory-startup --python this-file -- --source SOURCE
No approved source/export/native package is overwritten. UVs, corner colors and
semantic slots of every retained polygon are copied verbatim and read back.
"""
from pathlib import Path
import argparse, hashlib, json, sys
import bpy
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'Tools'))
from CityBuildingStudyIO import checked_fbx, load_object

args = argparse.ArgumentParser()
args.add_argument('--source', type=Path, default=ROOT/'ArtSource/Blender/CityLowDensityBuildings.blend')
source = args.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []).source
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(source) == '05e4b0409ac3f8785546feccf9b46443c449295819ef0ec68e7790658fb13d9e'
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
obj = load_object(source, 'SM_LowDensity_Home_A')
old = obj.data
# The legacy walk is a distinct, six-face box below the body pivot. Refuse an
# unexpected source rather than removing arbitrary low door/porch geometry.
removed = [p.index for p in old.polygons if all(old.vertices[v].co.z < -.001 for v in p.vertices)]
assert len(removed) == 6, removed
assert min(old.vertices[v].co.y for i in removed for v in old.polygons[i].vertices) < -7.99
keep = [p for p in old.polygons if p.index not in removed]
used = sorted({v for p in keep for v in p.vertices}); remap = {v:i for i,v in enumerate(used)}
mesh = bpy.data.meshes.new('CompactHomeBodyWithoutWalk')
mesh.from_pydata([old.vertices[i].co for i in used], [], [[remap[i] for i in p.vertices] for p in keep])
for material in old.materials: mesh.materials.append(material)
for layer in old.uv_layers:
    target = mesh.uv_layers.new(name=layer.name)
    for before, after in zip(keep, mesh.polygons):
        for a,b in zip(before.loop_indices, after.loop_indices): target.data[b].uv = layer.data[a].uv
for layer in old.color_attributes:
    assert layer.domain == 'CORNER'
    target = mesh.color_attributes.new(name=layer.name, type=layer.data_type, domain='CORNER')
    for before, after in zip(keep, mesh.polygons):
        for a,b in zip(before.loop_indices, after.loop_indices): target.data[b].color = layer.data[a].color
for before, after in zip(keep, mesh.polygons):
    after.material_index = before.material_index; after.use_smooth = before.use_smooth
obj.data = mesh; obj.name = 'SM_CompactHome_GroundPilot'
obj.location = (0,0,0); bpy.context.view_layer.update()
slots = [m.name.split('.')[0] for m in mesh.materials]
export = ROOT/'ArtSource/Exports/HomeGroundPilot/SM_CompactHome_GroundPilot.fbx'
export.parent.mkdir(parents=True, exist_ok=True)
with checked_fbx(obj, export, slots, verify_alpha=True): pass
blend = ROOT/'ArtSource/Blender/CityHomeGroundPilot.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(blend))
manifest = json.loads((ROOT/'ArtSource/Manifests/city_low_density_buildings.json').read_text())
original = next(e for e in manifest['assets'] if e['name']=='SM_LowDensity_Home_A')
record = dict(issue=125, scope='one A/brick Compact Home; other appearances unchanged',
    source='ArtSource/Blender/CityLowDensityBuildings.blend', source_sha256=sha(source),
    derived_source=str(blend.relative_to(ROOT)).replace('\\','/'), derived_source_sha256=sha(blend),
    export=str(export.relative_to(ROOT)).replace('\\','/'), export_sha256=sha(export),
    mesh_destination='/Game/CitySim/Art/HomeGroundPilot/SM_CompactHome_GroundPilot',
    triangles=len(mesh.loop_triangles), sections=len(slots), material_bindings=original['material_bindings'],
    removed_walk_faces=removed, retained_faces=len(keep), dimensions_m=list(obj.dimensions),
    door_native_cm=[0,467,0], pedestrian_width_cm=130,
    unchanged='retained positions, UVs, corner colors, semantic materials; approved source and original native meshes')
(ROOT/'ArtSource/Manifests/city_home_ground_pilot.json').write_text(json.dumps(record,indent=2)+'\n')
assert sha(source)==record['source_sha256']
print('CITYSIM_HOME_GROUND_SOURCE_PASS', json.dumps(record))

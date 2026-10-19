"""#192 exact approved nuclear v6 -> checked LODs and terrain handoff.

Approved source stays unchanged. No native, catalog or runtime writes.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import sys
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'Tools'))
from CityBuildingStudyIO import checked_fbx, load_object
from Assets.Manifests.CityApprovedSources import verified_export

PIN = 'db7dd8baf2c4844f95bf850b6d9b8114e187f764dfd0d394f106646a40e3f0b5'
SOURCE = ROOT / 'ArtSource/Generated/blender/issue-153/v6'
OUT = ROOT / 'ArtSource/Exports/NuclearFacility'
BLEND = ROOT / 'ArtSource/Blender/CityNuclearFacility-v6.blend'
MANIFEST = ROOT / 'ArtSource/Manifests/city_nuclear_integration.json'

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def relative(path):
    return Path(path).relative_to(ROOT).as_posix()

def split_parts(mesh, components):
    parents = list(range(len(mesh.vertices)))
    def find(i):
        while parents[i] != i:
            parents[i] = parents[parents[i]]
            i = parents[i]
        return i
    for edge in mesh.edges:
        a, b = edge.vertices
        parents[find(a)] = find(b)
    groups = {}
    for v in mesh.vertices:
        groups.setdefault(find(v.index), []).append(v.index)
    remaining = list(components)
    result = []
    for ids in groups.values():
        pts = [mesh.vertices[i].co for i in ids]
        lo = [min(v[k] for v in pts) for k in range(3)]
        hi = [max(v[k] for v in pts) for k in range(3)]
        delta, index = min((max(abs(a-b) for a,b in zip(lo+hi,c['min']+c['max'])),i) for i,c in enumerate(remaining))
        assert delta < .0002, (lo, hi, delta)
        component = remaining.pop(index)
        idset = set(ids)
        polys = [p for p in mesh.polygons if p.vertices[0] in idset]
        result.append(dict(component=component, ids=sorted(ids), polys=polys))
    assert not remaining and len(result) == 162
    return result

def make_lod(source, parts, lod, segments, omitted):
    vertices, faces, uvs, materials, smooth = [], [], [], [], []
    old = source.data
    for part in parts:
        name = part['component']['name']
        if any(name == prefix or name.startswith(prefix + '.') for prefix in omitted):
            continue
        ids = part['ids']
        reduce = name.startswith(('Open cooling shell', 'Recessed intake band'))
        if reduce:
            assert len(ids) % 32 == 0 and segments in (8,16)
            rings = len(ids) // 32
            assert all(len(p.vertices) == 4 for p in part['polys'])
            assert len(part['polys']) == (rings-1)*32
            stride = 32 // segments
            chosen = [ids[j*32+i*stride] for j in range(rings) for i in range(segments)]
            offset = len(vertices)
            vertices += [tuple(old.vertices[i].co) for i in chosen]
            for j in range(rings-1):
                for i in range(segments):
                    a, b = j*segments+i, j*segments+(i+1)%segments
                    face = [offset+a, offset+b, offset+b+segments, offset+a+segments]
                    points = [Vector(vertices[k]) for k in face]
                    normal = (points[1]-points[0]).cross(points[2]-points[0])
                    axis = max(range(3), key=lambda k: abs(normal[k]))
                    axes = [k for k in range(3) if k != axis]
                    faces.append(face)
                    uvs.append([(p[axes[0]],p[axes[1]]) for p in points])
                    materials.append(part['polys'][0].material_index)
                    smooth.append(True)
        else:
            remap = {i:len(vertices)+k for k,i in enumerate(ids)}
            vertices += [tuple(old.vertices[i].co) for i in ids]
            for p in part['polys']:
                faces.append([remap[i] for i in p.vertices])
                uvs.append([tuple(old.uv_layers.active.data[i].uv) for i in p.loop_indices])
                materials.append(p.material_index)
                smooth.append(p.use_smooth)
    name = 'SM_NuclearFacility_v6_LOD' + str(lod)
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    for mat in old.materials:
        mesh.materials.append(mat)
    uv = mesh.uv_layers.new(name='UV0')
    for p, coords, material, shading in zip(mesh.polygons, uvs, materials, smooth):
        p.material_index, p.use_smooth = material, shading
        for index, value in zip(p.loop_indices, coords):
            uv.data[index].uv = value
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj

def measure(obj):
    bpy.context.view_layer.update()
    obj.data.calc_loop_triangles()
    assert all(math.isfinite(x) for v in obj.data.vertices for x in v.co)
    assert all(math.isfinite(x) for uv in obj.data.uv_layers.active.data for x in uv.uv)
    assert all(abs(x)<1e-6 for x in obj.location)
    assert all(abs(x-1)<1e-6 for x in obj.scale)
    for x in (-8, 7.8):
        hit, *_ = obj.ray_cast(Vector((x,10.1,15)), Vector((0,0,-1)), distance=16)
        assert not hit, ('Tower throat closed', obj.name, x)
    return dict(triangles=len(obj.data.loop_triangles), sections=len(obj.data.materials), dimensions_m=list(obj.dimensions))

def main():
    verified_export(SOURCE,153,6,PIN,kind='blender')
    assert not OUT.exists() and not BLEND.exists() and not MANIFEST.exists(), 'Use a fresh preparation destination'
    before = {relative(p):digest(p) for p in SOURCE.rglob('*') if p.is_file()}
    validation = json.loads((SOURCE/'validation.json').read_text())
    anchors = json.loads((SOURCE/'entrance-anchors.json').read_text())
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    # Library inspection identifies the sole asset mesh without executing scripts.
    with bpy.data.libraries.load(str(SOURCE/'asset.blend'), link=False) as (available, _):
        names = [n for n in available.objects if not n.startswith('ReviewGround')]
    objects = []
    for name in names:
        with bpy.data.libraries.load(str(SOURCE/'asset.blend'), link=False) as (available, loaded):
            loaded.objects = [name]
        obj = loaded.objects[0]
        if obj.type == 'MESH':
            bpy.context.collection.objects.link(obj)
            objects.append(obj)
        else:
            bpy.data.objects.remove(obj)
    assert len(objects) == 1
    obj = objects[0]
    assert list(obj.location)==[0,0,0] and all(abs(x)<1e-6 for x in obj.rotation_euler)
    parts = split_parts(obj.data, validation['components'])
    supports = []
    for part in parts:
        c = part['component']
        if c['min'][2] > .002:
            continue
        contact = [list(obj.data.vertices[i].co) for i in part['ids'] if abs(obj.data.vertices[i].co.z) < .002]
        assert contact
        supports.append(dict(component=c['name'],kind='structural_ground_contact',bounds_m=c,
                             support_top_z_m=0,contact_vertices_m=contact))
    # Electrical tanks/radiators intentionally sit above grade; runtime supplies
    # their under-support. They are equipment, never terrain-reaching paving.
    for index, y in enumerate((-4.5,-.7)):
        relevant = [p['component'] for p in parts if p['component']['name'].startswith(('Transformer tank','Radiator block','Radiator rib'))
                    and p['component']['min'][1] >= y-1.051 and p['component']['max'][1] <= y+1.051]
        assert len(relevant)==9
        supports.append(dict(component='Transformer assembly '+str(index),kind='equipment_under_support',
                             min_m=[min(c['min'][k] for c in relevant) for k in range(3)],
                             max_m=[max(c['max'][k] for c in relevant) for k in range(3)],
                             support_top_z_m=.15,parts=[c['name'] for c in relevant]))
    OUT.mkdir(parents=True)
    slots = [m.name for m in obj.data.materials]
    records = [dict(lod=0,export=relative(SOURCE/'asset.fbx'),export_sha256=digest(SOURCE/'asset.fbx'),**measure(obj))]
    obj.name = 'SM_NuclearFacility_v6_LOD0'
    for lod, segments, omitted in [(1,16,()),(2,8,('Window mullion','Side window mullion','Unit louver','Radiator rib','Cabinet latch','Door handle','Shutter seam','Flange','Terminal'))]:
        derived = make_lod(obj, parts, lod, segments, omitted)
        info = measure(derived)
        assert info['triangles'] < records[-1]['triangles']
        assert max(abs(a-b) for a,b in zip(info['dimensions_m'],records[0]['dimensions_m'])) < .002
        path = OUT/(derived.name+'.fbx')
        with checked_fbx(derived,path,slots):
            pass
        records.append(dict(lod=lod,export=relative(path),export_sha256=digest(path),shell_segments=segments,
                            omitted_component_prefixes=list(omitted),roundtrip_pass=True,**info))
        derived.location.x = lod*40  # gallery only; exported ground-centred above
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    assert all(digest(ROOT/name)==value for name,value in before.items())
    manifest = dict(issue=192,source_issue=153,version=6,approval_sha256=PIN,
                    approved_directory=relative(SOURCE),approved_source_sha256=before[relative(SOURCE/'asset.blend')],
                    source=relative(BLEND),source_sha256=digest(BLEND),lods=records,
                    native_target='/Game/CitySim/Art/Utilities/Nuclear/SM_NuclearFacility_v6',
                    source_front='-Y',unit_scale=1,pivot_m=[0,0,0],nominal_grade_m=0,site_cells=[4,4],site_m=[32,32],
                    anchors=anchors['anchors'],support_regions=supports,material_regions=slots,
                    material_contract=json.loads((SOURCE/'material-manifest.json').read_text()),
                    structural_decks=[],site_paving=[],moving_parts=[],
                    terrain_owner_issue=125,native_yaw_mapping='Verify actual imported handedness; source metadata remains authoritative input',
                    collision_policy='Logical occupancy/selection authoritative; no automatic complex collision; native owner chooses required simple proxy',
                    lod_policy='LOD0 approved exact FBX; LOD1 half tower segments; LOD2 quarter segments and small fixtures removed; native screen thresholds require review',
                    original_source_unchanged=True,native_import_run=False,device_appearance='unconfirmed')
    MANIFEST.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print('NUCLEAR_SOURCE_PREPARATION_PASS',[(r['lod'],r['triangles']) for r in records],len(supports),'support regions')

if __name__ == '__main__':
    main()

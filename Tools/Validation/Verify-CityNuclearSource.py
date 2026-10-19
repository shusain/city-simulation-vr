"""Independent retained-surface and actual FBX checks for nuclear #192."""
from pathlib import Path
import hashlib
import json
import math
import sys
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
DOC=json.loads((ROOT/'ArtSource/Manifests/city_nuclear_integration.json').read_text())
OUT=ROOT/'Artifacts/Issue192'

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def parts(mesh):
    adj=[set() for _ in mesh.vertices]
    for e in mesh.edges:
        a,b=e.vertices;adj[a].add(b);adj[b].add(a)
    seen=set();groups=[]
    for start in range(len(adj)):
        if start in seen:continue
        todo=[start];seen.add(start);group=[]
        while todo:
            i=todo.pop();group.append(i)
            for j in adj[i]-seen:seen.add(j);todo.append(j)
        ids=set(group)
        polygons=[p for p in mesh.polygons if p.vertices[0] in ids]
        bounds=tuple(min(mesh.vertices[i].co[k] for i in group) for k in range(3))+tuple(max(mesh.vertices[i].co[k] for i in group) for k in range(3))
        groups.append((bounds,group,polygons))
    return groups

def signature(mesh,polygons):
    faces=[]
    for p in polygons:
        corners=[tuple(round(x,6) for x in mesh.vertices[mesh.loops[i].vertex_index].co)+tuple(round(x,6) for x in mesh.uv_layers.active.data[i].uv) for i in p.loop_indices]
        # Polygon starting corner may change; winding is preserved by rotations.
        variants=[tuple(corners[i:]+corners[:i]) for i in range(len(corners))]
        faces.append((p.material_index,min(variants),p.use_smooth))
    return sorted(faces)

def asset_mesh(path):
    with bpy.data.libraries.load(str(path),link=False) as (available,loaded):
        loaded.objects=[n for n in available.objects if n.startswith('Nuclear')]
    meshes=[o for o in loaded.objects if o.type=='MESH' and not o.name.startswith('ReviewGround')]
    assert len(meshes)==1
    bpy.context.collection.objects.link(meshes[0]);return meshes[0]

def main():
    source=ROOT/DOC['approved_directory']
    originals={p:sha(p) for p in source.rglob('*') if p.is_file()}
    original=asset_mesh(source/'asset.blend');original_mesh=original.data
    original_parts=parts(original_mesh)
    validation=json.loads((source/'validation.json').read_text())
    named={}
    for bounds,ids,polys in original_parts:
        matches=[c for c in validation['components'] if max(abs(a-b) for a,b in zip(bounds,c['min']+c['max']))<.0002]
        assert len(matches)==1
        named[matches[0]['name']]=(bounds,ids,polys)
    assert len(named)==162
    with bpy.data.libraries.load(str(ROOT/DOC['source']),link=False) as (available,loaded):
        loaded.objects=[n for n in available.objects if n.startswith('SM_NuclearFacility_v6_LOD')]
    objects={int(o.name.split('LOD')[-1].split('.')[0]):o for o in loaded.objects}
    assert set(objects)=={0,1,2}
    checks=[]
    for lod,entry in enumerate(DOC['lods']):
        obj=objects[lod];obj.location=(0,0,0);bpy.context.collection.objects.link(obj)
        mesh=obj.data;mesh.calc_loop_triangles();actual_parts=parts(mesh)
        assert len(mesh.loop_triangles)==entry['triangles']
        assert all(math.isfinite(x) for v in mesh.vertices for x in v.co)
        assert all(math.isfinite(x) for uv in mesh.uv_layers.active.data for x in uv.uv)
        removed=entry.get('omitted_component_prefixes',[])
        expected={n for n in named if not any(n==p or n.startswith(p+'.') for p in removed)}
        assert len(actual_parts)==len(expected)
        untouched=0
        for bounds,ids,polys in actual_parts:
            matches=[n for n in expected if max(abs(a-b) for a,b in zip(bounds,named[n][0]))<.0002]
            assert len(matches)==1,(lod,bounds,matches)
            name=matches[0];expected.remove(name)
            if lod==0 or not name.startswith(('Open cooling shell','Recessed intake band')):
                assert signature(mesh,polys)==signature(original_mesh,named[name][2]),(lod,name)
                untouched+=1
            else:
                old_points={tuple(round(x,6) for x in original_mesh.vertices[i].co) for i in named[name][1]}
                assert all(tuple(round(x,6) for x in mesh.vertices[i].co) in old_points for i in ids)
                assert len(polys)==len(named[name][2])//(2 if lod==1 else 4)
        assert not expected
        for x in (-8,7.8):
            hit,*_=obj.ray_cast(Vector((x,10.1,15)),Vector((0,0,-1)),distance=16)
            assert not hit,(lod,'closed throat')
        for anchor in DOC['anchors']:
            corridor=anchor['reserved_free_corridor'];lo,hi=corridor['min'],corridor['max']
            assert not any(all(b[k+3]>lo[k] and b[k]<hi[k] for k in range(3)) for b,_,_ in actual_parts)
        before=set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(ROOT/entry['export']))
        imported=[o for o in set(bpy.data.objects)-before if o.type=='MESH'];assert len(imported)==1
        check=imported[0];check.data.calc_loop_triangles()
        assert len(check.data.loop_triangles)==entry['triangles']
        assert {m.name.split('.')[0] for m in check.data.materials}==set(DOC['material_regions'])
        assert max(abs(a-b) for a,b in zip(check.dimensions,entry['dimensions_m']))<.002
        assert all(math.isfinite(x) for uv in check.data.uv_layers.active.data for x in uv.uv)
        checks.append(dict(lod=lod,triangles=entry['triangles'],components=len(actual_parts),unchanged_components=untouched,
                           full_retained_face_uv_material_winding_check=True,open_throats=2,clear_corridors=2,actual_fbx_readback=True))
        for o in set(bpy.data.objects)-before:bpy.data.objects.remove(o,do_unlink=True)
    assert len(DOC['support_regions'])==35
    grounded={c['name'] for c in validation['components'] if c['min'][2]<.002}
    supplied={s['component'] for s in DOC['support_regions'] if s['kind']=='structural_ground_contact'}
    assert grounded==supplied
    assert all(sha(p)==value for p,value in originals.items())
    result=dict(issue=192,lods=checks,ground_contact_regions=len(grounded),equipment_under_support_regions=2,
                original_approved_files_unchanged=True,native_unreal_run=False,headset_appearance='unconfirmed')
    (OUT/'IndependentSourceVerification.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('NUCLEAR_INDEPENDENT_SOURCE_PASS',json.dumps(result))

if __name__=='__main__':main()

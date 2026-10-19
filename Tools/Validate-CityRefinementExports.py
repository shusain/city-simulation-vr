"""Round-trip D79 staged FBX and D71 prototype FBX in Blender, without Unreal."""
from pathlib import Path
import bpy, bmesh, json, hashlib

root=Path(__file__).resolve().parents[1]
draft=json.loads((root/'Artifacts/D79/draft.json').read_text(encoding='utf-8'))
citizens=json.loads((root/'ArtSource/Manifests/city_citizen_prototype.json').read_text(encoding='utf-8'))
report=[]
for e in draft['assets']+citizens['assets']:
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    prototype=e.get('prototype_only',False)
    path=root/e['export'] if prototype else root/'Artifacts/D79/Exports'/Path(e['export']).name
    bpy.ops.import_scene.fbx(filepath=str(path))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    assert len(meshes)==1, path
    obj=meshes[0]
    bpy.context.view_layer.objects.active=obj
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    obj.data.calc_loop_triangles()
    triangles=len(obj.data.loop_triangles)
    slots={m.name.split('.')[0] for m in obj.data.materials}
    expected=set(e['material_slots'])
    assert slots==expected and len(obj.data.materials)==len(expected), (path,slots,expected)
    vertices=[obj.matrix_world@v.co for v in obj.data.vertices]
    assert abs(min(v.z for v in vertices))<1e-5, path
    bm=bmesh.new();bm.from_mesh(obj.data)
    assert not any(f.calc_area()<1e-8 for f in bm.faces),path
    boundaries=sum(edge.is_boundary for edge in bm.edges);bm.free()
    if prototype:
        assert boundaries==0 and triangles==e['triangles']
        assert all(abs(min(v[i] for v in vertices)-e['bounds_min_m'][i])<1e-5
                   and abs(max(v[i] for v in vertices)-e['bounds_max_m'][i])<1e-5 for i in range(3))
        attributes=list(obj.data.color_attributes)
        assert len(attributes)==1
        palette={tuple(round(c,2) for c in value.color[:3]) for value in attributes[0].data}
        assert len(palette)>=5, (path,'Vertex palette lost',palette)
    else:
        measured=next(m for m in draft['measurements'] if m['asset_id']==e['asset_id'])
        assert triangles==measured['triangles'] and boundaries==measured['boundary_edges']
        assert all(abs(a-b)<1e-4 for a,b in zip(obj.dimensions,e['dimensions_m']))
        if e['asset_id']=='building.residential.high.a':
            colors=list(obj.data.color_attributes)[0]
            assert any(c.color[3]<.01 for c in colors.data), 'Window mask lost'
            assert all(colors.data[i].color[3]>.99 for p in obj.data.polygons
                       if min(obj.data.vertices[j].co.z for j in p.vertices)>=36
                       for i in p.loop_indices), 'Rooftop equipment is marked as emissive windows'
            bottoms={round(v.co.z,3) for v in obj.data.vertices if v.co.y < -6.6 and abs(v.co.x)>2}
            assert {9.025,17.025,25.025,33.025,13.75,29.75}.issubset(bottoms)
            # Test the actual disconnected slab components after FBX import.
            bm=bmesh.new();bm.from_mesh(obj.data)
            remaining=set(bm.verts);slabs=[];roof_equipment=[]
            while remaining:
                pending=[remaining.pop()];component=[]
                while pending:
                    v=pending.pop();component.append(v)
                    for edge in v.link_edges:
                        other=edge.other_vert(v)
                        if other in remaining:remaining.remove(other);pending.append(other)
                low=[min(v.co[i] for v in component) for i in range(3)]
                high=[max(v.co[i] for v in component) for i in range(3)]
                if high[1]<-6.49 and low[2]>7 and high[2]-low[2]<.6:
                    slabs.append((low,high))
                if low[2]>=35.999: roof_equipment.append((low,high))
            bm.free()
            assert len(slabs)==10,slabs
            for i,(a,b) in enumerate(slabs):
                for c,d in slabs[i+1:]:
                    assert not all(min(b[k],d[k])-max(a[k],c[k])>1e-5 for k in range(3)), 'Facade slabs intersect'
            assert len(roof_equipment)==22
            assert all(abs(v)<7 for a,b in roof_equipment for v in a[:2]+b[:2]), 'Equipment exceeds roof edge'
            housing=[(a,b) for a,b in roof_equipment if all(abs(b[i]-a[i]-size)<1e-4
                     for i,size in enumerate((5.4,4.4,4.0)))]
            assert len(housing)==1
            a,b=housing[0]
            assert abs((a[0]+b[0])*.5-2.4)<1e-4 and abs((a[1]+b[1])*.5-1.8)<1e-4
            assert abs(max(b[2] for a,b in roof_equipment)-41)<1e-4
    report.append(dict(name=obj.name,triangles=triangles,slots=len(slots),boundary_edges=boundaries,
        sha256=hashlib.sha256(path.read_bytes()).hexdigest(),roundtrip_pass=True))
(root/'Artifacts/D79/roundtrip.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('CITYSIM_REFINEMENT_ROUNDTRIP_PASS',len(report))

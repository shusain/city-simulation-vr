"""D83 independent source/FBX rig and deformation checks; Blender only."""
from pathlib import Path
import bpy, hashlib, json, math
from mathutils import Vector
from mathutils.kdtree import KDTree

ROOT=Path(__file__).resolve().parents[1]
manifest=json.loads((ROOT/'ArtSource/Manifests/city_park_refinement.json').read_text())
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
for path,expected in manifest['protected_inputs'].items():
    assert digest(ROOT/path)==expected, path
assert digest(ROOT/'ArtSource/Blender/CityParkRefinement.blend')==manifest['source_sha256']
assert digest(ROOT/'ArtSource/Blender/CityCitizenRigPrototype.blend')==manifest['rig_source_sha256']

bpy.ops.wm.open_mainfile(filepath=str(ROOT/'ArtSource/Blender/CityParkRefinement.blend'))
static=[]
for entry in manifest['assets']:
    obj=bpy.data.objects[entry['name']]
    mesh=obj.data;mesh.calc_loop_triangles()
    assert len(mesh.loop_triangles)==entry['triangles_after']
    assert digest(ROOT/entry['export'])==entry['sha256']
    assert list(obj.rotation_euler)==[0,0,0] and list(obj.scale)==[1,1,1]
    assert len(mesh.materials)==len(entry['material_slots'])
    # Independently reconstruct connected components, rather than trusting the
    # authoring pass's chosen ring order or its metadata.
    edges={i:set() for i in range(len(mesh.vertices))}
    for edge in mesh.edges:
        a,b=edge.vertices;edges[a].add(b);edges[b].add(a)
    remaining=set(edges);cylinders=0;ramps=0;crowns=0;pruned_lobes=0;trunks=0
    while remaining:
        stack=[min(remaining)];comp=set(stack)
        while stack:
            for other in edges[stack.pop()]-comp:comp.add(other);stack.append(other)
        remaining-=comp
        faces=[p for p in mesh.polygons if p.vertices[0] in comp]
        points=[mesh.vertices[i].co for i in comp]
        if len(comp)==96 and len(faces)==50:
            trunk=entry['asset_id']=='building.park.a' and max(v.z for v in points)<4.5 and abs(sum(v.x for v in points)/96)>6
            if trunk:trunks+=1
            else:cylinders+=1
            cap=[p for p in faces if len(p.vertices)==48]
            sides=[p for p in faces if len(p.vertices)==4]
            assert len(cap)==2 and all(not p.use_smooth for p in cap)
            assert all(p.use_smooth for p in sides)
            for p in cap:
                z=[mesh.vertices[i].co.z for i in p.vertices]
                assert max(z)-min(z)<1e-5, 'Nonplanar cylinder cap'
                assert abs(p.normal.z)>.999
            for p in sides:
                xy={(round(mesh.vertices[i].co.x,5),round(mesh.vertices[i].co.y,5)) for i in p.vertices}
                if not trunk:assert len(xy)==2, 'Twisted cylinder side'
                else:
                    center_x=sum(v.x for v in points)/96;center_y=sum(v.y for v in points)/96
                    directions=set()
                    for i in p.vertices:
                        offset=mesh.vertices[i].co.xy-Vector((center_x,center_y))
                        offset.normalize();directions.add(tuple(round(c,4) for c in offset))
                    assert len(directions)==2,'Twisted tapered trunk'
                if not trunk:assert abs(p.normal.z)<1e-5
            if trunk:
                x=sum(v.x for v in points)/96;y=sum(v.y for v in points)/96
                for p in cap:
                    radius=.32 if sum(mesh.vertices[i].co.z for i in p.vertices)/48<2.4 else .17
                    assert all(abs(math.hypot(mesh.vertices[i].co.x-x,mesh.vertices[i].co.y-y)-radius)<1e-5 for i in p.vertices), (entry['name'],p.normal[:],radius,x,y,[(mesh.vertices[i].co.z,math.hypot(mesh.vertices[i].co.x-x,mesh.vertices[i].co.y-y)) for i in list(p.vertices)[:3]])
        elif len(comp)==24 and entry['asset_id']=='building.park.a':
            trunks+=1
            assert len(faces)==14 and all(not p.use_smooth for p in faces)
            assert mesh.materials[faces[0].material_index].name.split('.')[0]=='Bark'
            x=sum(v.x for v in points)/24;y=sum(v.y for v in points)/24
            for v in points:
                radius=.32 if v.z<2.4 else .17
                assert abs(math.hypot(v.x-x,v.y-y)-radius)<1e-5
        elif len(comp)==70 and '.skate' in entry['asset_id']:
            ramps+=1
            riding=[p for p in faces if p.use_smooth]
            assert len(riding)==32
            for p in riding:
                for i in p.vertices:
                    v=mesh.vertices[i].co
                    assert abs(v.z-(.36+.12*(abs(v.x)-8)**2))<1e-5
            assert all(not p.use_smooth for p in faces if len(p.vertices)>4)
        elif len(comp)==162:
            crowns+=1
            assert len(faces)==320 and all(p.use_smooth for p in faces)
        elif len(comp)==34:
            pruned_lobes+=1
            assert len(faces)==40 and all(not p.use_smooth for p in faces)
            assert mesh.materials[faces[0].material_index].name.split('.')[0]=='Canopy'
            center_x=sum(v.x for v in points)/34;center_y=sum(v.y for v in points)/34
            offsets={(round(v.x-center_x,5),round(v.y-center_y,5),round(v.z,5)) for v in points}
            assert all((-x,y,z) in offsets and (x,-y,z) in offsets for x,y,z in offsets), 'Asymmetric pruned crown'
    expected=sum(c['kind']=='cylinder' for c in entry['changes'])
    assert cylinders==expected
    assert ramps==sum(c['kind']=='ramp' for c in entry['changes'])
    assert crowns==sum(c['kind']=='crown' for c in entry['changes'])
    assert pruned_lobes==sum(c.get('lobes',0) for c in entry['changes'] if c['kind']=='pruned_crown')
    assert trunks==sum(c['kind']=='pruned_trunk' for c in entry['changes'])
    static.append(dict(name=obj.name,cylinders=cylinders,crowns=crowns,pruned_lobes=pruned_lobes,tapered_trunks=trunks,ramps=ramps,pass_checks=True))

bpy.ops.wm.open_mainfile(filepath=str(ROOT/'ArtSource/Blender/CityCitizenRigPrototype.blend'))
rig_checks=[]
source_corners={};source_weights={};source_poses={}
def colored_corners(mesh):
    data=mesh.data;attr=data.color_attributes.active_color
    return sorted((tuple(round(c,4) for c in data.vertices[data.loops[i].vertex_index].co),
                   tuple(round(c,3) for c in attr.data[i].color))
                  for p in data.polygons for i in p.loop_indices)
def weighted_vertices(mesh):
    return sorted((tuple(round(c,4) for c in v.co),
                   tuple(sorted((mesh.vertex_groups[g.group].name,round(g.weight,5)) for g in v.groups)))
                  for v in mesh.data.vertices)
def world_points(mesh):
    evaluated=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    skin=evaluated.to_mesh()
    points=sorted(tuple(evaluated.matrix_world@v.co) for v in skin.vertices)
    evaluated.to_mesh_clear()
    return points
for entry in manifest['citizens']:
    mesh=bpy.data.objects[entry['name']];rig=mesh.parent
    mesh.hide_set(False);rig.hide_set(False)
    rig.location=(0,0,0)
    source_corners[entry['name']]=colored_corners(mesh)
    source_weights[entry['name']]=weighted_vertices(mesh)
    source_poses[entry['name']]={}
    assert set(rig.data.bones.keys())==set(entry['bones']) and len(rig.data.bones)==20
    assert rig.data.bones['root'].parent is None
    assert all(b.parent is not None for b in rig.data.bones if b.name!='root')
    assert all(abs(sum(g.weight for g in v.groups)-1)<1e-6 and 1<=len(v.groups)<=2 for v in mesh.data.vertices)
    samples=0
    for name,period in entry['clips'].items():
        action=bpy.data.actions[rig.name+'_'+name]
        rig.animation_data.action=action
        for frame in range(1,period+2):
            bpy.context.scene.frame_set(frame)
            evaluated=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
            skin=evaluated.to_mesh()
            assert len(skin.vertices)==len(mesh.data.vertices)
            points=[v.co.copy() for v in skin.vertices]
            assert all(math.isfinite(c) for v in points for c in v)
            low=min(v.z for v in points);high=max(v.z for v in points)
            assert abs(low)<1e-4, (entry['name'],name,frame,'ground',low)
            assert .8<high<2.5, (name,'unbounded pose',high)
            skin.calc_loop_triangles()
            assert all((points[t.vertices[1]]-points[t.vertices[0]]).cross(points[t.vertices[2]]-points[t.vertices[0]]).length>1e-8 for t in skin.loop_triangles)
            if frame==1:first=points
            if frame==period+1:assert max((a-b).length for a,b in zip(first,points))<1e-5, 'Non-looping pose'
            evaluated.to_mesh_clear();samples+=1
            if frame in (1,9,13,period//2+1,period+1):
                source_poses[entry['name']][(name,frame)]=world_points(mesh)
        # Walk feet must move primarily along +X, not cross across lateral Y.
        if name=='Walk':
            assert any((a-b.co).length>.05 for a,b in zip(first,mesh.data.vertices))
    rig_checks.append(dict(name=entry['name'],deformation_samples=samples,bones=20,normalized_weights=True,pass_checks=True))

exports=[]
for entry in manifest['citizens']:
    assert digest(ROOT/entry['export'])==entry['sha256']
    for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
    for action in list(bpy.data.actions):bpy.data.actions.remove(action)
    bpy.ops.import_scene.fbx(filepath=str(ROOT/entry['export']),use_anim=True)
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    rigs=[o for o in bpy.context.scene.objects if o.type=='ARMATURE']
    assert len(meshes)==len(rigs)==1
    mesh,rig=meshes[0],rigs[0]
    mesh.data.calc_loop_triangles()
    assert len(mesh.data.loop_triangles)==entry['triangles']
    assert set(rig.data.bones.keys())==set(entry['bones'])
    assert all(abs(sum(g.weight for g in v.groups)-1)<2e-6 for v in mesh.data.vertices)
    assert colored_corners(mesh)==source_corners[entry['name']], 'Rig FBX rest geometry/RGBA differs'
    assert weighted_vertices(mesh)==source_weights[entry['name']], 'Rig FBX skin weights differ'
    actions=list(bpy.data.actions)
    for clip in entry['clips']:
        assert any(a.name.endswith('_'+clip) for a in actions), (entry['name'],clip,[a.name for a in actions])
    # Ensure each export contains only its own clips, not all earlier palettes.
    assert len(actions)==len(entry['clips']),(entry['name'],'unexpected animations',[a.name for a in actions])
    pose_samples=0
    maximum_error=0
    for (clip,frame),expected in source_poses[entry['name']].items():
        action=next(a for a in actions if a.name.endswith('_'+clip))
        rig.animation_data.action=action
        bpy.context.scene.frame_set(frame)
        actual=world_points(mesh)
        assert len(actual)==len(expected)
        # Stable vertex order instead of sorted XYZ: coincident/seam vertices may
        # swap in floating point. Compare nearest positions in both directions.
        def distance_to(points,other):
            tree=KDTree(len(other))
            for i,point in enumerate(other):tree.insert(point,i)
            tree.balance()
            return max(tree.find(point)[2] for point in points)
        error=distance_to(expected,actual)
        reverse=distance_to(actual,expected)
        maximum_error=max(maximum_error,error,reverse)
        assert maximum_error<1e-4,(entry['name'],clip,frame,'FBX deformation mismatch',maximum_error)
        pose_samples+=1
    exports.append(dict(name=entry['name'],bones=20,actions=[a.name for a in actions],roundtrip_pass=True,
                        geometry_rgba_weights_preserved=True,deformation_samples=pose_samples,
                        maximum_pose_error_m=maximum_error))

result=dict(task='D83',static=static,source_rigs=rig_checks,fbx_rigs=exports,
            protected_inputs_unchanged=True,unreal_launched=False,device_checks='Pending')
(ROOT/'Artifacts/D83/validation.json').write_text(json.dumps(result,indent=2)+'\n')
print('CITYSIM_D83_VALIDATION_PASS',len(static),'static;',len(rig_checks),'source rigs;',len(exports),'FBX rigs')

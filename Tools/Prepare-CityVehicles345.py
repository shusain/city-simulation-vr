"""Derive native fleet inputs from the six immutable approved Blender exports."""
from pathlib import Path
from collections import defaultdict, Counter
import hashlib, json, sys
import bpy,bmesh
from mathutils import Matrix

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'Tools'))
from CityBuildingStudyIO import checked_fbx
from Assets.Manifests.CityApprovedSources import verified_export
OUT = ROOT/'ArtSource/Exports/Vehicle345'
BLENDS = ROOT/'ArtSource/Blender/Vehicle345'

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p, d): p.write_text(json.dumps(d, indent=2)+'\n', encoding='utf-8', newline='\n')
def bounds(vs): return {side:[fn(v[k] for v in vs) for k in range(3)] for side,fn in [('min',min),('max',max)]}

def components(mesh):
    parent=list(range(len(mesh.vertices)))
    def find(i):
        while parent[i]!=i: parent[i]=parent[parent[i]]; i=parent[i]
        return i
    for e in mesh.edges: parent[find(e.vertices[0])]=find(e.vertices[1])
    result=defaultdict(list)
    for v in mesh.vertices: result[find(v.index)].append(v.index)
    return list(result.values())

def main():
    OUT.mkdir(parents=True, exist_ok=True); BLENDS.mkdir(parents=True, exist_ok=True)
    inputs=json.loads((ROOT/'ArtSource/Manifests/city_vehicle_inputs345.json').read_bytes()); rows=[]
    for a in inputs['assets']:
        source=ROOT/a['source']; verified_export(source,a['issue'],a['version'],a['approval_sha256'],'blender')
        before={str(p):sha(p) for p in source.iterdir() if p.is_file()}
        bpy.ops.wm.open_mainfile(filepath=str(source/'asset.blend'))
        obj=next(o for o in bpy.context.scene.objects if o.type=='MESH' and not o.name.startswith('ReviewGround'))
        # Source joins can retain the active part's transform. Bake world space,
        # preserving reviewed dimensions and actual tire contacts at z=0.
        obj.data.transform(obj.matrix_world); obj.matrix_world=Matrix.Identity(4)
        mesh=obj.data; fixes=[]
        if a['key']=='GarbageTruck':
            moved=[]
            for ids in components(mesh):
                b=bounds([mesh.vertices[i].co for i in ids]); span=[b['max'][k]-b['min'][k] for k in range(3)]
                if abs(span[0]-.68)<.003 and abs(span[1]-.30)<.003 and abs(span[2]-.12)<.003 and 1.5<b['min'][0]<1.6:
                    for i in ids: mesh.vertices[i].co.x-=.30
                    moved.append(dict(before=b,after=bounds([mesh.vertices[i].co for i in ids])))
            assert len(moved)==2, ('Expected both cab steps',moved)
            # Front tire begins x=2.07. Corrected step ends x=1.92: .15m gap.
            assert all(2.07-r['after']['max'][0]>.149 for r in moved)
            fixes.append(dict(change='Move both cab steps rearward .30m for .15m tire clearance',steps=moved))
        # Make triangulation deterministic for concave boolean wheel arches.
        # FBX import otherwise silently drops zero-area tessellation triangles.
        bm=bmesh.new();bm.from_mesh(mesh)
        bmesh.ops.triangulate(bm,faces=list(bm.faces),quad_method='FIXED',ngon_method='EAR_CLIP')
        degenerate=[f for f in bm.faces if f.calc_area()<1e-8]
        if degenerate:
            fixes.append(dict(change='Remove zero-area export triangles',count=len(degenerate)))
            bmesh.ops.delete(bm,geom=degenerate,context='FACES_ONLY')
        bm.to_mesh(mesh);bm.free();mesh.update()
        materials=[]
        for mat in mesh.materials:
            p=mat.node_tree.nodes.get('Principled BSDF') if mat.use_nodes else None
            materials.append(dict(slot=mat.name,color=list(p.inputs['Base Color'].default_value) if p else list(mat.diffuse_color),roughness=float(p.inputs['Roughness'].default_value) if p else .7,metallic=float(p.inputs['Metallic'].default_value) if p else 0.))
        lamps=[]; lamp_faces={}
        for ids in components(mesh):
            b=bounds([mesh.vertices[i].co for i in ids]); idset=set(ids)
            polys=[p for p in mesh.polygons if p.vertices[0] in idset]
            names={mesh.materials[p.material_index].name for p in polys}; color=None
            if a['key']=='PoliceCar' and names <= {'EmergencyRed','EmergencyBlue'}:
                color=(1,.015,.01,0) if 'EmergencyRed' in names else (.01,.12,1,0)
            elif a['key']=='Ambulance' and names <= {'RedLens','BlueLens'}:
                color=(1,.015,.01,0) if 'RedLens' in names else (.01,.12,1,0)
            elif a['key']=='FireEngine' and b['min'][2]>3.09 and b['max'][2]<3.30 and b['min'][0]>2.1 and names <= {'Taillight','Headlight'}:
                color=(1,.015,.01,0) if 'Taillight' in names else (1,.75,.55,0)
            if color:
                for p in polys: lamp_faces[p.index]=color
                lamps.append(dict(bounds_m=b,color=list(color),triangles=sum(len(p.vertices)-2 for p in polys)))
        if a['key'] in ('PoliceCar','FireEngine','Ambulance'):
            assert len(lamps)>=2 and any(c['color'][2]>.5 for c in lamps) and any(c['color'][2]<.1 for c in lamps),(a['key'],lamps)
            roof=bpy.data.materials.new('Roof'); roof.diffuse_color=(.10,.14,.19,1);mesh.materials.append(roof);roof_index=len(mesh.materials)-1
            # Unreal imports the first FBX color layer. The runner's original
            # all-white layer must not precede the civic beacon mask.
            for old in list(mesh.color_attributes):
                if old.name!='WindowMask':mesh.color_attributes.remove(old)
            attr=mesh.color_attributes.get('WindowMask') or mesh.color_attributes.new(name='WindowMask',type='BYTE_COLOR',domain='CORNER')
            for value in attr.data: value.color=(1,1,1,1)
            for p in mesh.polygons:
                if p.index in lamp_faces:
                    p.material_index=roof_index
                    for li in p.loop_indices: attr.data[li].color=lamp_faces[p.index]
            mesh.color_attributes.active_color=attr
            mesh.color_attributes.render_color_index=0
        # Strip unused authoring/boolean slots, preserve polygon indices exactly.
        used=sorted({p.material_index for p in mesh.polygons});indices=[p.material_index for p in mesh.polygons]
        selected=[mesh.materials[i] for i in used];remap={old:i for i,old in enumerate(used)}
        mesh.materials.clear()
        for mat in selected:mesh.materials.append(mat)
        for p,i in zip(mesh.polygons,indices):p.material_index=remap[i]
        slots=[m.name for m in mesh.materials]
        if a['key']=='GarbageTruck': slots[slots.index('BodyPaint')]='Service'
        mesh.update();mesh.calc_loop_triangles();b=bounds([v.co for v in mesh.vertices])
        assert abs(b['min'][2])<.003,(a['key'],b)
        name='SM_Traffic_'+a['key']+'_A';obj.name=name
        fbx=OUT/(name+'.fbx')
        with checked_fbx(obj,fbx,slots,verify_alpha=True):pass
        blend=BLENDS/(name+'.blend');bpy.ops.wm.save_as_mainfile(filepath=str(blend))
        bindings={}
        for old,slot in zip(used,slots):
            if slot=='Roof':continue
            original=materials[old].copy();original['slot']=slot;bindings[slot]=original
        row=dict(**a,export=fbx.relative_to(ROOT).as_posix(),export_sha256=sha(fbx),blend=blend.relative_to(ROOT).as_posix(),blend_sha256=sha(blend),slots=slots,materials=bindings,bounds_m=b,triangles=len(mesh.loop_triangles),emergency_lamps=lamps,derivative_changes=fixes,source_world_transform_baked=True)
        rows.append(row)
        assert all(sha(Path(p))==pin for p,pin in before.items())
    write(ROOT/'ArtSource/Manifests/city_vehicle_native345.json',dict(issue=345,assets=rows,source_unchanged=True,coordinates='metres; +X forward, ground-center pivot; native FBX x,-y,z in cm',parked_policy='Current approved civic replacements have no baked vehicles; fleet meshes remain reusable and defaults non-emissive'))
    print('VEHICLE345_PREPARATION_PASS',len(rows))

if __name__=='__main__': main()

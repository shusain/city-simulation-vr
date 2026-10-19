"""Prepare the exact fifteen approved #157 sources, without touching originals."""
from pathlib import Path
from collections import Counter
import bpy, hashlib, importlib.util, json, math, re, sys

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools'))
from Assets.Manifests.CityApprovedSources import verified_export
def module(name,file):
    spec=importlib.util.spec_from_file_location(name,ROOT/file)
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
p=module('utility_prepare','Tools/Assets/Blender/Prepare-CityUtilityIntegration.py')
inspect=module('utility_inspect','Tools/Assets/Blender/Inspect-CityUtilityNativeRecipe.py')
OUT=ROOT/'ArtSource/Exports/ApprovedCivicBatch'
SCOPE='/Game/CitySim/Art/ApprovedCivicBatch'
SITES={'primary':[4,4],'secondary':[5,5],'college':[6,5],
       'police':[5,4],'fire':[5,4],'hospital':[5,5],'wastedepot':[6,5],'recycling':[6,5]}

def stable_parts(obj,components):
    # Metadata identifies the pre-join connected solids by exact measured bounds.
    return p.parts_of(obj,components)

def supports(obj,parts):
    result=[]
    for part in parts:
        b=part['bounds'];name=part['name'];compact=re.sub('[^a-z0-9]','',name.lower())
        if abs(b['min'][2])>p.EPS:continue
        if any(t in compact for t in ('dumpster','bin','door','leaf','jamb','baysection','trucksection','flag','bollard','wheel','lid','handle')):continue
        if b['max'][2]<.3:continue
        contact=p.contours(obj.data,part,b['min'][2])
        area=(b['max'][0]-b['min'][0])*(b['max'][1]-b['min'][1])
        # These civic enclosures/posts have rectangular contacts. Never turn
        # a curved/hollow shape into a filled bounding rectangle.
        if abs(contact['contact_area_m2']-area)>max(.0001,area*.00001):
            raise AssertionError(('Nonrectangular structural contact requires contour support',name))
        result.append(dict(component=name,bounds_m=b,top_m=b['min'][2],contact=contact))
    assert result
    return result

def portals(parts,source):
    lookup={x['name']:x for x in parts};anchors=p.read(source/'entrance-anchors.json')['anchors']
    door=next(a for a in anchors if a['role']=='pedestrian');records=[]
    # Source anchors can be at the centre of a multipart door. Use the actual
    # leaf/front and sill, preserving authored framing/canopy geometry.
    for a in anchors:
        target=lookup[a['target']];b=target['bounds']
        records.append(dict(anchor=a,actual_bounds_m=b,entry_m=[a['xyz_m'][0],b['min'][1],b['min'][2]],
                            width_m=a['width_m'],role=a['role']))
    bayfile=source/'vehicle-bays.json'
    return records,p.read(bayfile) if bayfile.exists() else None

def source_outward(obj,parts):
    obj.data.calc_loop_triangles();rows=[]
    for part in parts:
        ids=set(part['ids']);edges=Counter(tuple(sorted(e)) for poly in part['polys'] for e in poly.edge_keys)
        closed=all(v==2 for v in edges.values())
        volume=sum(obj.data.vertices[t.vertices[0]].co.dot(obj.data.vertices[t.vertices[1]].co.cross(obj.data.vertices[t.vertices[2]].co))/6
                   for t in obj.data.loop_triangles if t.vertices[0] in ids)
        rows.append(dict(component=part['name'],closed=closed,signed_volume_m3=volume))
    return rows

def meshfacts(obj):
    # Material access can invalidate Blender's lazy tessellation cache. Capture
    # labels before acquiring loop-triangle views, then copy all indices first.
    m=obj.data;m.update();names=[mat.name for mat in m.materials]
    m.calc_loop_triangles();indices=[int(t.material_index) for t in m.loop_triangles]
    assert all(0<=i<len(names) for i in indices),(obj.name,indices[:10])
    counts=Counter(names[i] for i in indices);samples={}
    for index,name in enumerate(names):
        corners=[]
        normals=[list(n.vector) for n in m.corner_normals]
        for poly in m.polygons:
            if poly.material_index!=index:continue
            for vi,li in zip(poly.vertices,poly.loop_indices):
                corners.append(dict(position_m=list(m.vertices[vi].co),normal=normals[li],uv0=list(m.uv_layers[0].data[li].uv)))
        if corners:samples[name]=[corners[i] for i in sorted(set(round(i*(len(corners)-1)/15) for i in range(16)))]
    return dict(triangles_by_material=dict(counts),material_slots=names,uv_channels=len(m.uv_layers),uv_names=[u.name for u in m.uv_layers],
        render_uv_channel=next(i for i,u in enumerate(m.uv_layers) if u.active_render),bounds_m=p.measure(obj)['bounds_m'],samples=samples)

def main():
    assert not (ROOT/'ArtSource/Manifests/city_civic_batch.json').exists(),'Frozen manifest already exists'
    OUT.mkdir(parents=True,exist_ok=True)
    inputs=p.read(ROOT/'ArtSource/Manifests/city_civic_batch_inputs.json');records=[];protected={}
    key=sys.argv[sys.argv.index('--key')+1] if '--key' in sys.argv else None
    for a in inputs['assets']:
        if key and a['key']!=key:continue
        source=ROOT/a['export_directory'];verified_export(source,a['issue'],a['version'],a['approval_sha256'],kind='blender')
        protected.update({p.relative(f):p.digest(f) for f in source.rglob('*') if f.is_file()})
        obj=p.load_source(source/'asset.blend')
        valid=p.read(source/'validation.json');parts=stable_parts(obj,valid['components']);facts=p.measure(obj)
        assert facts['triangles']==valid['triangles']
        contacts=supports(obj,parts);entries,bays=portals(parts,source)
        outward=source_outward(obj,parts)
        reversed_parts=[r['component'] for r in outward if r['closed'] and r['signed_volume_m3']<0]
        if reversed_parts:
            # A winding-only derivative preserves each corner's UV, material,
            # vertex position and source file. No two-sided material workaround.
            import bmesh
            bm=bmesh.new();bm.from_mesh(obj.data);bm.faces.ensure_lookup_table()
            faceids={poly.index for part in parts if part['name'] in reversed_parts for poly in part['polys']}
            bmesh.ops.reverse_faces(bm,faces=[bm.faces[i] for i in faceids]);bm.to_mesh(obj.data);bm.free();obj.data.update()
            parts=stable_parts(obj,valid['components'])
            assert all(not r['closed'] or r['signed_volume_m3']>0 for r in source_outward(obj,parts))
        before=p.snapshot(obj)
        family=a['key'].split('_')[0];site=SITES[family]
        assert all(facts['bounds_m']['min'][k]>=-site[k]*4+.15 and facts['bounds_m']['max'][k]<=site[k]*4-.15 for k in (0,1)),(a['key'],facts['bounds_m'],site)
        textures=p.texture_materials(obj,source)['textures']
        graphs=[inspect.graph(m) for m in obj.data.materials]
        name='SM_Approved_'+a['key']+'_v'+str(a['version']);obj.name=name
        slots=[m.name for m in obj.data.materials];lods=[];folder=OUT/a['key'];folder.mkdir(exist_ok=True)
        for lod in range(3):
            if lod==0:
                model=obj;path=source/'asset.fbx';omitted=[]
                if reversed_parts:
                    path=folder/(name+'_LOD0_Outward.fbx')
                    with p.checked_fbx(model,path,slots):pass
            else:
                # FBX export/readback evaluates the source mesh and may replace
                # RNA polygon storage; reacquire component views for each LOD.
                parts=stable_parts(obj,valid['components'])
                tokens=('handle','latch','seam','ventslat','louver','louvre') if lod==1 else ('handle','latch','seam','ventslat','louver','louvre','roofedge')
                omitted=[x['name'] for x in parts if x['bounds']['min'][2]>.15 and any(t in re.sub('[^a-z0-9]','',x['name'].lower()) for t in tokens)
                    and not any(abs(x['bounds'][side][axis]-facts['bounds_m'][side][axis])<p.EPS for side in ('min','max') for axis in range(3))]
                model=p.subset(obj,[x for x in parts if x['name'] not in omitted],name+'_LOD'+str(lod));path=folder/(model.name+'.fbx')
                with p.checked_fbx(model,path,slots):pass
            info=p.measure(model)
            lods.append(dict(lod=lod,export=p.relative(path),export_sha256=p.digest(path),triangles=info['triangles'],facts=meshfacts(model),omitted_components=omitted))
            if lod:model.hide_set(True);model.hide_render=True
        assert p.snapshot(obj)==before
        records.append(dict(**a,source_head=a['head'],site_cells=site,source_front='-Y',presentation_quarter_turns=2,
            target=SCOPE+'/Meshes/'+name,damaged_target=SCOPE+'/Meshes/'+name+'_Damaged',source_pivot_m=[0,0,0],material_slots=slots,
            lods=lods,graphs=graphs,textures=textures,supports=contacts,portals=entries,vehicle_bays=bays,source_outward=outward,
            corrected_inward_components=reversed_parts,components=[dict(name=x['name'],bounds_m=x['bounds']) for x in parts]))
        print('CIVIC_PREPARED',a['key'],site,[x['triangles'] for x in lods],len(contacts),'supports',flush=True)
        p.write(folder/'Preparation.json',dict(record=records[-1],protected=protected))
    assert all(p.digest(ROOT/f)==pin for f,pin in protected.items())
    if key:return
    assert len(records)==15
    manifest=dict(issue=157,scope=SCOPE,source_base=inputs['source_base'],assets=records,protected_sources=protected,
        lod_policy='Named fixture omission only; intact structural contacts, entrances, symbols, panes and silhouette extrema. No source reauthoring.',
        native_basis='source metres x,y,z -> imported centimetres 100x,-100y,100z; presentation yaw2 applied separately',
        headset_observations='unconfirmed')
    p.write(ROOT/'ArtSource/Manifests/city_civic_batch.json',manifest)
    print('CIVIC_BATCH_SOURCE_PASS',len(records),flush=True)

if __name__=='__main__':main()

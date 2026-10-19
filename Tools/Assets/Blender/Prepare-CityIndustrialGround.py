"""#125 compact industrial bodies with independent personnel/freight approaches."""
from pathlib import Path
import hashlib,json,sys
import bpy
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingStudyIO import load_object,checked_fbx
from CityBuildingEntrances import freight_door,personnel_door
import importlib.util
spec=importlib.util.spec_from_file_location('IndustrialBuilder',ROOT/'Tools/Create-CityLowDensityVariants.py')
builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def components(mesh):
    # Weld coincident corners for analysis only; preserve actual UV seams.
    adj={};vf={}
    for p in mesh.polygons:
        keys=[tuple(round(float(x),4) for x in mesh.vertices[v].co) for v in p.vertices]
        for k in keys:adj.setdefault(k,set()).update(keys);vf.setdefault(k,set()).add(p.index)
    unseen=set(adj);result=[]
    while unseen:
        todo=[unseen.pop()];seen=set(todo)
        while todo:
            for k in adj[todo.pop()]&unseen:unseen.remove(k);seen.add(k);todo.append(k)
        result.append(sorted(set().union(*(vf[k] for k in seen))))
    return result

def main():
    inputs=json.loads((ROOT/'ArtSource/Manifests/city_industrial_ground_sources.json').read_text())
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    records=[]
    for entry in inputs['assets']:
        for key in ('source','export'):assert digest(ROOT/entry[key])==entry[key+'_sha256']
        obj=load_object(ROOT/entry['source'],entry['object']);before=obj.data
        removed=set();move={};dock=entry.get('entrance',{}).get('dock')
        for fs in components(before):
            vs={v for f in fs for v in before.polygons[f].vertices}
            lo=[min(before.vertices[v].co[a] for v in vs) for a in range(3)]
            hi=[max(before.vertices[v].co[a] for v in vs) for a in range(3)]
            # Exactly the personnel/legacy ground box, never the threshold.
            iswalk=abs(lo[1]+10.4)<.001 and hi[2]<.161
            islegacy=not entry['appearance'] and lo[1]<-10 and hi[2]<=4.001
            oldbay=not entry['appearance'] and lo[2]<.001 and hi[2]>=4.9 and hi[0]-lo[0]<8.1 and lo[1]<-7.4
            oldposts=not entry['appearance'] and abs(abs(lo[0]+hi[0])*.5-4.8)<.001 and hi[2]<2.001 and lo[1]<-7.6
            docksteps=dock and lo[0]<-4.9 and hi[0]<-3.19 and hi[2]<1.051
            if iswalk or islegacy or oldbay or oldposts or docksteps:
                removed.update(fs);continue
            center=(lo[1]+hi[1])*.5
            shift=1.5-3/14*max(-7,min(7,center))
            # Shorten the structural hall/roof in depth, while translating
            # small parts intact (doors, windows, round flues and hardware).
            for v in vs:
                x,y,z=before.vertices[v].co
                new_y=y+(1.5-3/14*max(-7,min(7,y)) if hi[1]-lo[1]>8 else shift)
                if dock and lo[1]<-9.4 and hi[2]<1.081:
                    #1m-deep loading ledge; bumper remains on its front face.
                    new_y+=min(1.4,max(0,(-y-7.05)/2.42*1.4)) if hi[1]>-7.1 else 1.4
                # Existing weathered medium/high roof corners overhang the
                #16m reservation by14.9cm; trim those outer roof vertices.
                move[v]=(max(-8,min(8,x)),new_y,z)
        keep=[p for p in before.polygons if p.index not in removed]
        used=sorted({v for p in keep for v in p.vertices});remap={v:i for i,v in enumerate(used)}
        vertices=[move[v] for v in used];faces=[[remap[v] for v in p.vertices] for p in keep]
        slots=list(entry['slots']);bindings=dict(entry['bindings']);extra=[];b=None
        if not entry['appearance']:
            # Replace ambiguous legacy blocks with an actual service door
            # and freight shutter, using established industrial recipes.
            b=builder.Builder(obj);freight_door(b,0,5.65,.08,4.55,roller=True)
            person=personnel_door(b,1)
            b.faces=b.faces[:-6];b.slots=b.slots[:-6];b.colors=b.colors[:-6];b.uvs=b.uvs[:-6]
            # Builder stores four independent corners per box face. Remove
            # unused path vertices too; Unreal prunes them from native bounds.
            b.vertices=b.vertices[:-24]
            assert max(v for f in b.faces for v in f)<len(b.vertices)
            offset=len(vertices);vertices.extend((x,y+3,z) for x,y,z in b.vertices)
            faces.extend([[offset+v for v in f] for f in b.faces])
            # Native/source existing materials only; no new material assets.
            steel='IndustrialMechanical' if 'IndustrialMechanical' in slots else 'CharredMechanical'
            trim='Roof' if 'Roof' in slots else 'CharredTrim'
            extra=[slots.index(steel if s==1 else trim) for s in b.slots]
        mesh=bpy.data.meshes.new(entry['target_name']);mesh.from_pydata(vertices,[],faces)
        for m in before.materials:mesh.materials.append(m)
        for layer in before.uv_layers:
            target=mesh.uv_layers.new(name=layer.name)
            for old,new in zip(keep,mesh.polygons):
                # Keep physical brick/metal repeat on shortened hall walls
                # and roofs. Small translated parts retain exact original UVs.
                slope=None
                for a in old.loop_indices:
                    pa=before.vertices[before.loops[a].vertex_index].co
                    for b0 in old.loop_indices:
                        pb=before.vertices[before.loops[b0].vertex_index].co
                        if abs(pa.x-pb.x)<.0001 and abs(pa.z-pb.z)<.0001 and abs(pa.y-pb.y)>8:
                            slope=(layer.data[b0].uv-layer.data[a].uv)/(pb.y-pa.y)
                for a,c in zip(old.loop_indices,new.loop_indices):
                    uv=layer.data[a].uv.copy()
                    if slope is not None:
                        v=before.loops[a].vertex_index
                        uv+=slope*(move[v][1]-before.vertices[v].co.y)
                    target.data[c].uv=uv
            if b:
                for p,uvs in zip(list(mesh.polygons)[len(keep):],b.uvs):
                    for li,uv in zip(p.loop_indices,uvs):target.data[li].uv=uv
        for layer in before.color_attributes:
            assert layer.domain=='CORNER'
            target=mesh.color_attributes.new(name=layer.name,type=layer.data_type,domain='CORNER')
            for old,new in zip(keep,mesh.polygons):
                for a,c in zip(old.loop_indices,new.loop_indices):target.data[c].color=layer.data[a].color
            for p in list(mesh.polygons)[len(keep):]:
                for li in p.loop_indices:target.data[li].color=(1,1,1,1)
        for old,new in zip(keep,mesh.polygons):new.material_index,new.use_smooth=old.material_index,old.use_smooth
        for p,slot in zip(list(mesh.polygons)[len(keep):],extra):p.material_index=slot
        mesh.update()
        if before.has_custom_normals:
            mesh.normals_split_custom_set([before.corner_normals[i].vector for p in keep for i in p.loop_indices]+[p.normal.copy() for p in list(mesh.polygons)[len(keep):] for _ in p.loop_indices])
        obj.data,obj.name=mesh,entry['target_name'];bpy.context.view_layer.update();mesh.calc_loop_triangles()
        assert all(abs(v.co.x)<=8.001 and abs(v.co.y)<=8.001 for v in mesh.vertices),entry['name']
        export=ROOT/'ArtSource/Exports/IndustrialGround'/(obj.name+'.fbx');export.parent.mkdir(parents=True,exist_ok=True)
        with checked_fbx(obj,export,slots,verify_alpha=True):pass
        # The outer sill is the actual private paving start, not door centre.
        portal=[520,444,20] if entry['appearance'] else [520,444,20]
        freight=[]
        original=entry.get('entrance',{}).get('freight',[dict(center_x_m=0,width_m=5.65,floor_m=.08)])
        for f in original:
            freight.append(dict(entry_native_cm=[f['center_x_m']*100,507 if dock else 437,8 if dock else 13],width_cm=f['width_m']*100,raised_dock=bool(dock)))
        records.append(dict(entry,derived_export=export.relative_to(ROOT).as_posix(),derived_export_sha256=digest(export),
            target='/Game/CitySim/Art/IndustrialGround/'+obj.name,triangles=len(mesh.loop_triangles),dimensions_m=list(obj.dimensions),
            entry_native_cm=portal,landing_cm=10,ground_reserve_cm=60,path_width_cm=130,freight=freight,
            removed_faces=sorted(removed),retained_faces=len(keep),added_faces=len(extra)))
        print('INDUSTRIAL_GROUND_DERIVED',obj.name,len(mesh.loop_triangles))
    blend=ROOT/'ArtSource/Blender/CityIndustrialGround.blend';bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    manifest=dict(issue=125,source=blend.relative_to(ROOT).as_posix(),source_sha256=digest(blend),assets=records,
        contract='Unchanged2x2 plots. Hall depth14m to11m,3m forecourt; roof/detail translation preserves original small-part dimensions/UV/materials. C loading ledge shortened to1m, old ground paths/dock stairs removed. Personnel approach separate from freight; raised dock vehicle pad meets grade below loading ledge.')
    (ROOT/'ArtSource/Manifests/city_industrial_ground.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('INDUSTRIAL_GROUND_SOURCE_PASS',len(records))
if __name__=='__main__':main()

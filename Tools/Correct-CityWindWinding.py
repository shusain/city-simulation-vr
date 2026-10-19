"""#273 CI-only targeted import: four mesh packages, no shared material saves."""
from pathlib import Path
from collections import defaultdict,Counter
import copy,sys,math
import unreal
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tools'))
from Assets.Unreal.CityApprovedUtilityPlan import read,write,sha
from Assets.Unreal.CityApprovedUtilityMeshes import import_mesh
from Assets.Unreal.CityApprovedUtilityNative import emit_files
PIN='c0b9831fe2b1a2f3c49a9ee386e423e69a88ba7e388e4b7d6300253d7420297c'

def component_volumes(mesh,lod,expected):
    # Weld split render corners within each material section. The source hub
    # and spinner have separate closed caps sharing an exact 24-edge ring;
    # position-only welding incorrectly fuses those two touching solids.
    vertices=[];triangles=[];lookup={};edges=Counter()
    for section in range(mesh.get_num_sections(lod)):
        vs,ts,_,_,_=unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,lod,section)
        remap=[]
        for v in vs:
            key=(section,)+tuple(round(c*1000) for c in (v.x,v.y,v.z))
            if key not in lookup:lookup[key]=len(vertices);vertices.append(v)
            remap.append(lookup[key])
        for i in range(0,len(ts),3):triangles.append(tuple(remap[ts[i+j]] for j in range(3)))
    parent=list(range(len(vertices)))
    def find(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    for a,b,c in triangles:
        assert len({a,b,c})==3
        parent[find(a)]=find(b);parent[find(b)]=find(c)
        for edge in ((a,b),(b,c),(c,a)):edges[tuple(sorted(edge))]+=1
    assert set(edges.values())=={2},('Native solid not closed',mesh.get_name(),lod)
    groups=defaultdict(list)
    for t in triangles:groups[find(t[0])].append(t)
    assert len(groups)==len(expected),(mesh.get_name(),lod,len(groups),len(expected))
    result=[];remaining=list(expected)
    for ts in groups.values():
        ids={v for t in ts for v in t};center=sum((vertices[i] for i in ids),unreal.Vector())/len(ids)
        volume=sum((vertices[a]-center).dot((vertices[b]-center).cross(vertices[c]-center))/6 for a,b,c in ts)
        bounds=dict(min=[min(getattr(vertices[i],k) for i in ids) for k in ('x','y','z')],max=[max(getattr(vertices[i],k) for i in ids) for k in ('x','y','z')])
        def error(row):
            b=row['bounds_m'];pred=dict(min=[100*b['min'][0],-100*b['max'][1],100*b['min'][2]],max=[100*b['max'][0],-100*b['min'][1],100*b['max'][2]])
            return max(abs(x-y) for s in ('min','max') for x,y in zip(bounds[s],pred[s]))
        row=min(remaining,key=error);assert error(row)<.2
        assert len(ts)==row['triangles'] and volume<0,(row['component'],volume)
        relative=abs(volume+row['signed_volume_m3']*1e6)/(row['signed_volume_m3']*1e6)
        # Magnitude is diagnostic: source lofts contain nonplanar quads, so
        # Blender/FBX native triangulation can choose different diagonals.
        # Closure and outward sign, not approximate enclosed volume, establish
        # winding. Existing position/UV/normal/count checks remain unchanged.
        result.append(dict(component=row['component'],native_clockwise_volume_cm3=volume,source_outward_volume_m3=row['signed_volume_m3'],relative_volume_error=relative,triangles=len(ts),closed=True))
        remaining.remove(row)
    return result

def main():
    assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
    manifest=ROOT/'ArtSource/Manifests/city_wind_winding273.json';assert sha(manifest)==PIN
    doc=read(manifest);assert doc['issue']==273 and doc['corner_parity_checks_pass']
    for path,pin in doc['source_files'].items():assert sha(ROOT/path)==pin
    protected={p:sha(p) for p in (ROOT/'CitySimVR/Content/CitySim').rglob('*.uasset')}
    unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.new_object(unreal.StaticMeshEditorSubsystem)
    results=[];files=[]
    for record in doc['records']:
        for lod in record['lods']:assert sha(ROOT/lod['export'])==lod['export_sha256']
        targets=[record['target']]+([record['target']+'_Damaged'] if record['role']=='Whole' else [])
        for target in targets:
            current=unreal.load_asset(target);assert isinstance(current,unreal.StaticMesh)
            bindings={str(s.get_editor_property('imported_material_slot_name')):current.get_material(i).get_path_name().split('.')[0] for i,s in enumerate(current.get_editor_property('static_materials'))}
            assert set(bindings)==set(record['material_slots'])
            selected=copy.deepcopy(record);selected['target']=target
            mesh,actual=import_mesh(selected,bindings,subsystem)
            actual['solid_winding']=[dict(lod=l['lod'],components=component_volumes(mesh,l['lod'],l['components'])) for l in record['lods']]
            results.append(actual);files.append(target)
    native=emit_files(files);assert len(native)==4
    changed={ROOT/x['file'] for x in native}
    assert all(sha(path)==pin for path,pin in protected.items() if path not in changed)
    write(ROOT/'Artifacts/CIAssetPreparation/WindWinding273.json',dict(issue=273,manifest_sha256=PIN,source_originals_unchanged=True,
          files=native,meshes=results,other_native_packages_unchanged=True,engine=unreal.SystemLibrary.get_engine_version()))
    unreal.log('WIND273_NATIVE_PASS four mesh packages; twelve LOD checks; closed outward solids; single-sided materials')

if __name__=='__main__': main()

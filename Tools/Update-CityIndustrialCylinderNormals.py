"""Focused production-copy shading update; geometry, maps and study inputs stay fixed."""
from pathlib import Path
import hashlib,json,sys
import bpy
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingStudyIO import snapshot,checked_fbx
from CityIndustrialCylinderNormals import smooth_cylinders
OUT=ROOT/'Artifacts/D104/CylinderShading'
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    path=ROOT/'ArtSource/Manifests/city_low_density_buildings.json';doc=json.loads(path.read_text())
    assert digest(ROOT/doc['source'])==doc['source_sha256']
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/doc['source']));rows=[]
    for e in doc['assets']:
        if e['category']!='Factory':continue
        o=bpy.data.objects[e['name']];before=snapshot(o);location=o.location.copy();o.location=(0,0,0)
        shading=smooth_cylinders(o);assert snapshot(o)==before
        expected={}
        for p in o.data.polygons:
            if p.material_index!=2:continue
            for li in p.loop_indices:
                v=o.data.vertices[o.data.loops[li].vertex_index].co;n=o.data.corner_normals[li].vector
                key=tuple(round(float(x),5) for x in v)
                expected.setdefault(key,[]).append(tuple(float(x) for x in n))
        with checked_fbx(o,ROOT/e['export'],e['slots'],verify_alpha=True) as check:
            assert check.data.has_custom_normals
            # Imported corners may be reordered. Every mechanical corner must
            # retain a source normal at the same position, including hard rims.
            count=0
            for p in check.data.polygons:
                if p.material_index!=2:continue
                for li in p.loop_indices:
                    v=check.data.vertices[check.data.loops[li].vertex_index].co;n=check.data.corner_normals[li].vector
                    key=tuple(round(float(x),5) for x in v)
                    candidates=expected.get(key)
                    if not candidates:candidates=expected[min(expected,key=lambda k:sum((k[a]-v[a])**2 for a in range(3)))]
                    assert max(n.dot(__import__('mathutils').Vector(x)) for x in candidates)>.9999,(o.name,tuple(v),tuple(n))
                    count+=1
        e['export_sha256']=digest(ROOT/e['export']);e['cylinder_shading']=shading
        rows.append(dict(name=e['name'],**shading,roundtrip_normal_corners=count,expected_mechanical_normals=[dict(position_m=list(k),normals=v) for k,v in expected.items()]))
        o.location=location
    bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/doc['source']))
    doc['source_sha256']=digest(ROOT/doc['source']);doc['cylinder_shading']='Custom averaged curved-wall corner normals; caps/rims and other architecture hard; geometry and materials unchanged.'
    path.write_text(json.dumps(doc,indent=2)+'\n');(OUT/'Source.json').write_text(json.dumps(dict(source=doc['source'],source_sha256=doc['source_sha256'],meshes=rows),indent=2)+'\n')
    print('CITYSIM_D104_CYLINDER_SOURCE_PASS eight meshes; unchanged geometry/UV/RGBA; FBX custom normals retained')
if __name__=='__main__':main()

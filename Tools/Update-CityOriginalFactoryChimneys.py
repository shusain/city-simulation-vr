"""Open original factory flues on the production copy, retaining reviewed exterior."""
from pathlib import Path
import hashlib,json,math,sys
import bpy,bmesh
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityIndustrialCylinderNormals import hollow_original_chimneys,smooth_cylinders
from CityBuildingStudyIO import checked_fbx
OUT=ROOT/'Artifacts/D104/DynamicReflections'
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    path=ROOT/'ArtSource/Manifests/city_low_density_buildings.json';doc=json.loads(path.read_text());assert digest(ROOT/doc['source'])==doc['source_sha256']
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/doc['source']));healthy=bpy.data.objects['SM_LowDensity_Factory_A'];damaged=bpy.data.objects['SM_LowDensity_Factory_A_Damaged']
    healthy_bounds=list(healthy.dimensions);old_points={tuple(v.co) for v in healthy.data.vertices};assert hollow_original_chimneys(healthy)==2
    assert old_points.issubset({tuple(v.co) for v in healthy.data.vertices}) and all(abs(a-b)<1e-5 for a,b in zip(healthy.dimensions,healthy_bounds))
    materials=list(damaged.data.materials);old=damaged.data;damaged.data=healthy.data.copy()
    for i,m in enumerate(materials):damaged.data.materials[i]=m
    height=max(v.co.z for v in damaged.data.vertices)
    for v in damaged.data.vertices:
        if v.co.z>height*.45:v.co.z*=.72+.14*math.sin(v.co.x*.55+v.co.y*.43+8)
    bm=bmesh.new();bm.from_mesh(damaged.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(damaged.data);bm.free()
    for colors in damaged.data.color_attributes:
        for c in colors.data:c.color=(1,1,1,1)
    if not old.users:bpy.data.meshes.remove(old)
    rows=[]
    for e in doc['assets']:
        if e['name'] not in (healthy.name,damaged.name):continue
        o=bpy.data.objects[e['name']];location=o.location.copy();o.location=(0,0,0);shading=smooth_cylinders(o);o.data.calc_loop_triangles()
        with checked_fbx(o,ROOT/e['export'],e['slots'],verify_alpha=True) as check:assert check.data.has_custom_normals
        e.update(export_sha256=digest(ROOT/e['export']),triangles=len(o.data.loop_triangles),cylinder_shading=shading,hollow_original_chimneys=2)
        assert all(abs(a-b)<.002 for a,b in zip(o.dimensions,e['dimensions_m']))
        rows.append(dict(name=o.name,triangles=e['triangles'],hollow_chimneys=2,exterior_dimensions_unchanged=True));o.location=location
    bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/doc['source']));doc['source_sha256']=digest(ROOT/doc['source']);doc['original_factory_chimneys']='Two hollow flues with 20% wall thickness, smooth inner/outer walls and hard annular rims; matched damage.'
    path.write_text(json.dumps(doc,indent=2)+'\n');(OUT/'Chimneys.json').write_text(json.dumps(dict(source_sha256=doc['source_sha256'],meshes=rows),indent=2)+'\n')
    print('CITYSIM_D104_ORIGINAL_HOLLOW_PASS four paired chimneys; outside dimensions preserved')
if __name__=='__main__':main()

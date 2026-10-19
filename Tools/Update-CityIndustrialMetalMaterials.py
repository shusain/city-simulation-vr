"""Rebind factory doors/hardware on the production copy, preserving mesh data."""
from pathlib import Path
import hashlib,json,sys,bpy
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Tools'))
from CityIndustrialMetalMaterials import apply_healthy,METALS,STEEL_SLOT
from CityBuildingStudyIO import checked_fbx,load_object
OUT=ROOT/'Artifacts/D104/IndustrialMetal'
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def geometry(obj):
    m=obj.data
    return (tuple(tuple(v.co) for v in m.vertices),tuple(tuple(p.vertices) for p in m.polygons),
            tuple(tuple(x.uv) for layer in m.uv_layers for x in layer.data),
            tuple(tuple(x.color) for layer in m.color_attributes for x in layer.data))
def main():
    path=ROOT/'ArtSource/Manifests/city_low_density_buildings.json';doc=json.loads(path.read_text(encoding='utf-8'))
    assert digest(ROOT/doc['source'])==doc['source_sha256']
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/doc['source']));rows=[]
    for letter in 'ABCD':
        name='SM_LowDensity_Factory_'+letter;entry=next(e for e in doc['assets'] if e['name']==name)
        obj=bpy.data.objects[name];damaged=bpy.data.objects[name+'_Damaged'];before=[geometry(o) for o in (obj,damaged)]
        reference=load_object(ROOT/doc['approved_source'],entry['source_object'])
        selected=apply_healthy(obj,reference,entry['entrance'],entry['slots'],entry['material_bindings'])
        bpy.data.objects.remove(reference,do_unlink=True)
        damage=next(e for e in doc['assets'] if e['name']==damaged.name)
        assert len(obj.data.polygons)==len(damaged.data.polygons)
        index=len(damage['slots']);damage['slots'].append(STEEL_SLOT)
        charred=damaged.data.materials[damage['slots'].index('IndustrialMechanical')].copy()
        charred.name='D104_CharredDoorMetal';damaged.data.materials.append(charred)
        for i in selected:damaged.data.polygons[i].material_index=index
        damage['material_bindings'][STEEL_SLOT]='/Game/CitySim/Art/Materials/MI_CharredMechanical'
        for o,e,g in zip((obj,damaged),(entry,damage),before):
            assert geometry(o)==g;location=o.location.copy();o.location=(0,0,0)
            with checked_fbx(o,ROOT/e['export'],e['slots'],verify_alpha=True,expected_triangles=e['triangles']) as check:
                assert check.data.has_custom_normals
            assert geometry(o)==g;o.location=location
            e.update(export_sha256=digest(ROOT/e['export']),sections=len(e['slots']),industrial_metal_faces=len(selected))
            rows.append(dict(name=o.name,faces_rebound=len(selected),triangles=e['triangles'],geometry_uv_rgba_unchanged=True,damaged=e['damaged']))
    bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/doc['source']))
    doc['source_sha256']=digest(ROOT/doc['source']);doc['industrial_metals']=METALS
    doc['material_refinement']='Bare factory corrugated steel ~0.144 roughness; exposed door steel0.22/hardware0.18 metallic1/HQ reflections. Painted walls stay nonmetallic; damage stays rough charred.'
    raw=(json.dumps(doc,indent=2)+'\n').encode('utf-8');tmp=path.with_suffix('.d104-tmp');tmp.write_bytes(raw);tmp.replace(path)
    (OUT/'Source.json').write_text(json.dumps(dict(source_sha256=doc['source_sha256'],materials=METALS,meshes=rows),indent=2)+'\n',encoding='utf-8')
    print('CITYSIM_INDUSTRIAL_METAL_SOURCE_PASS eight paired meshes; unchanged geometry/UV/RGBA')
if __name__=='__main__':main()

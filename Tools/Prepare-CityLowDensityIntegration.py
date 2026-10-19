"""Author approved D103 geometry with D104 surfaces and paired damage exports."""
from pathlib import Path
import hashlib, json, math, sys, importlib.util
import bpy, bmesh
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tools'))
from CityBuildingStudyIO import checked_fbx, snapshot
from CityIndustrialCylinderNormals import smooth_cylinders, hollow_original_chimneys
from CityIndustrialMetalMaterials import apply_healthy, METALS
spec=importlib.util.spec_from_file_location('CityMaterialStudySource',ROOT/'Tools/Create-CityMaterialStudy.py')
study=importlib.util.module_from_spec(spec);spec.loader.exec_module(study)
material=study.material;native_winding=study.native_winding

def digest(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    approved=json.loads((ROOT/'ArtSource/Manifests/city_low_density_variants_study.json').read_text())
    assert digest(ROOT/approved['source'])==approved['source_sha256']
    textures=json.loads((ROOT/'ArtSource/Textures/MaterialStudy/Materials.json').read_text())
    out=ROOT/'Artifacts/LowDensity/Integration';out.mkdir(parents=True,exist_ok=True)
    exports=ROOT/'ArtSource/Exports/LowDensity';exports.mkdir(parents=True,exist_ok=True)
    source=ROOT/'ArtSource/Blender/CityLowDensityBuildings.blend'
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/approved['source']))
    original_objects=list(bpy.data.objects)
    options={};records=[];variants=[]
    charred={}
    for category in ('Home','Shop','Factory'):
        for surface,label,tint,metal in textures['options'][category]:
            name=category+'_'+surface
            m=material(name,textures['surfaces'][surface],tint,metal)
            scale=.4 if metal else 1
            if scale!=1:
                shader=m.node_tree.nodes.get('Principled BSDF');link=shader.inputs['Roughness'].links[0]
                input_socket=link.from_socket;m.node_tree.links.remove(link)
                n=m.node_tree.nodes.new('ShaderNodeMath');n.operation='MULTIPLY';n.inputs[1].default_value=scale
                m.node_tree.links.new(input_socket,n.inputs[0]);m.node_tree.links.new(n.outputs[0],shader.inputs['Roughness'])
            options[name]=dict(name=name,surface=surface,label=label,tint=tint,metallic=metal,roughness_scale=scale,
                hq_forward_reflections=bool(metal),native_material='/Game/CitySim/Art/LowDensity/Materials/M_'+name)
    for index,record in enumerate(approved['assets']):
        original=bpy.data.objects[record['name']];before=snapshot(original)
        obj=original.copy();obj.data=original.data.copy();bpy.context.scene.collection.objects.link(obj);obj.location=(0,0,0)
        category=record['category'];letter=record['variant'][0]
        surface={'A':'Brick','B':'Timber','C':'Clapboard','D':'Corrugated'}[letter]
        name='SM_LowDensity_'+category+'_'+letter;obj.name=name
        slots=list(record['slots']);bindings=dict(approved['material_contracts'][category])
        if surface!='Brick':
            wall={'Home':'ResidentialBrick','Shop':'CommercialBrick','Factory':'IndustrialBrick'}[category]
            material_name=category+'_'+surface
            m=next(m for m in bpy.data.materials if m.name==material_name)
            for i,slot in enumerate(slots):
                if slot==wall:obj.data.materials[i]=m
            bindings[wall]=options[material_name]['native_material']
        assert snapshot(obj)==before
        metal_faces=apply_healthy(obj,original,record['entrance'],slots,bindings) if category=='Factory' else []
        winding=native_winding(obj,record['name'])
        if category=='Factory' and letter=='A':hollow_original_chimneys(obj)
        base=dict(category=category,definition_id={'Home':'Home','Shop':'Shop','Factory':'Factory'}[category],
            appearance_id=letter+'_'+surface,source_object=record['name'],slots=slots,
            native_winding_corrected_faces=winding,entrance=record['entrance'])
        if category=='Factory':base['industrial_metal_faces']=len(metal_faces)
        for damaged in (False,True):
            current=obj
            mapping=dict(bindings)
            if damaged:
                current=obj.copy();current.data=obj.data.copy();bpy.context.scene.collection.objects.link(current)
                current.name=name+'_Damaged';height=max(v.co.z for v in current.data.vertices)
                for v in current.data.vertices:
                    if v.co.z>height*.45:v.co.z*=.72+.14*math.sin(v.co.x*.55+v.co.y*.43+index)
                bm=bmesh.new();bm.from_mesh(current.data);boundaries=sum(e.is_boundary for e in bm.edges)
                bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
                assert all(f.calc_area()>1e-8 for f in bm.faces)
                assert sum(e.is_boundary for e in bm.edges)==boundaries
                bm.to_mesh(current.data);bm.free();current.data.update()
                for colors in current.data.color_attributes:
                    for c in colors.data:c.color=(1,1,1,1)
                for slot in slots:
                    suffix='Mechanical' if slot=='IndustrialDoorMetal' else 'Brick' if 'Brick' in slot else 'Mechanical' if 'Mechanical' in slot else slot if slot.startswith('Roof') and slot!='Roof' else 'Trim'
                    mapping[slot]='/Game/CitySim/Art/Materials/MI_Charred'+suffix
                    if slot not in charred:
                        m=bpy.data.materials.new('D104_Charred'+slot);m.use_nodes=True
                        p=m.node_tree.nodes.get('Principled BSDF')
                        p.inputs['Base Color'].default_value=(.19,.15,.13,1) if suffix=='Brick' else (.065,.06,.055,1)
                        p.inputs['Roughness'].default_value=.9;charred[slot]=m
                    current.data.materials[slots.index(slot)]=charred[slot]
            elif category=='Shop':mapping['StoreSignTall']='/Game/CitySim/Art/LowDensity/Materials/MI_Sign_market'
            current.location=(0,0,0)
            if category=='Factory':smooth_cylinders(current)
            bpy.context.view_layer.update();current.data.calc_loop_triangles()
            path=exports/(current.name+'.fbx')
            with checked_fbx(current,path,slots,verify_alpha=True):pass
            coords=[v.co for v in current.data.vertices]
            records.append(dict(base,name=current.name,damaged=damaged,material_bindings=mapping,
                export=path.relative_to(ROOT).as_posix(),export_sha256=digest(path),triangles=len(current.data.loop_triangles),
                sections=len(slots),dimensions_m=list(current.dimensions),bounds_min_m=[min(v[a] for v in coords) for a in range(3)],
                mesh_destination='/Game/CitySim/Art/LowDensity/Meshes/'+current.name))
            current.location=(index%4*26,-(index//4)*34-(110 if damaged else 0),0)
        variants.append(dict(definition_id=base['definition_id'],appearance_id=base['appearance_id'],
            presentation_mesh='/Game/CitySim/Art/LowDensity/Meshes/'+name,
            damaged_mesh='/Game/CitySim/Art/LowDensity/Meshes/'+name+'_Damaged'))
        assert snapshot(original)==before
    for o in original_objects:bpy.data.objects.remove(o,do_unlink=True)
    bpy.context.preferences.filepaths.save_version=0;bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(source))
    assert digest(ROOT/approved['source'])==approved['source_sha256']
    doc=dict(task='D104',production_integration_authorized=True,source=source.relative_to(ROOT).as_posix(),source_sha256=digest(source),
        approved_source=approved['source'],approved_source_sha256=approved['source_sha256'],assets=records,variants=variants,
        materials=list(options.values()),industrial_metals=METALS,textures=textures['surfaces'],citizen_height_m=1.43175,
        policy='Four coordinated appearances per family: A brick, B timber, C siding, D corrugated. Stable entity ID; paired damage. Gameplay definitions unchanged.',
        material_refinement='Bare factory steel ~0.144 roughness; exposed doors0.22/hardware0.18 metallic1/HQ reflections; painted walls remain nonmetallic; damage stays rough charred.',
        native_winding='Six veranda faces corrected on production copy; approved study stays unchanged.')
    (ROOT/'ArtSource/Manifests/city_low_density_buildings.json').write_text(json.dumps(doc,indent=2)+'\n')
    (out/'Source.json').write_text(json.dumps(doc,indent=2)+'\n')
    print('CITYSIM_D104_PRODUCTION_SOURCE_PASS 12 healthy +12 matched damage FBX roundtrips')

if __name__=='__main__':main()

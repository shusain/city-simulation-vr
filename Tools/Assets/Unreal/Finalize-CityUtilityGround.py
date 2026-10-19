"""#125/#192 nuclear damage presentation and scoped4x4 catalog adoption.

Requires the exact healthy import. Keeps original source assets and all other
catalog fields/definitions intact; writes a matching charred presentation.
"""
from pathlib import Path
import hashlib
import json
import sys
import unreal

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'Tools'))
from CityBuildingAdoptionMaterials import node, color, link, prop, finish


def main():
    assert Path(unreal.Paths.project_dir()).resolve() == (ROOT/'CitySimVR').resolve()
    scope = '/Game/CitySim/Art/Utilities/Nuclear'
    mesh = unreal.load_asset(scope+'/SM_NuclearFacility_v6')
    assert mesh and mesh.get_num_triangles(0) == 4616
    lib = unreal.MaterialEditingLibrary
    # One reusable tint parameter keeps the exact source maps for healthy and
    # damaged equipment while damage remains clearly visible on the same body.
    pbr = unreal.load_asset(scope+'/Materials/M_UtilityPBR')
    sample = lib.get_material_property_input_node(pbr, unreal.MaterialProperty.MP_BASE_COLOR)
    assert sample
    tint = node(pbr, unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property('parameter_name','SurfaceTint')
    tint.set_editor_property('default_value',unreal.LinearColor(1,1,1,1))
    multiply = node(pbr, unreal.MaterialExpressionMultiply)
    link(sample,multiply,'A');link(tint,multiply,'B')
    prop(multiply,unreal.MaterialProperty.MP_BASE_COLOR)
    finish(pbr)
    destination = scope+'/SM_NuclearFacility_v6_Damaged'
    damaged = unreal.EditorAssetLibrary.duplicate_asset(mesh.get_path_name().split('.')[0],destination)
    assert damaged
    files = ['CitySimVR/Content/'+p.removeprefix('/Game/')+'.uasset' for p in [destination,scope+'/Materials/M_UtilityPBR']]
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        region = str(slot.get_editor_property('imported_material_slot_name'))
        material = mesh.get_material(index)
        target = scope+'/Materials/MI_Nuclear_'+region+'_Damaged'
        instance = unreal.EditorAssetLibrary.duplicate_asset(material.get_path_name().split('.')[0],target)
        assert instance
        if region in ('RoofService','EquipmentMetal'):
            lib.set_material_instance_vector_parameter_value(instance,'SurfaceTint',unreal.LinearColor(.16,.13,.11,1))
        else:
            lib.set_material_instance_vector_parameter_value(instance,'BaseColor',unreal.LinearColor(.065,.055,.045,1))
            lib.set_material_instance_scalar_parameter_value(instance,'Roughness',.95)
        assert unreal.EditorAssetLibrary.save_loaded_asset(instance)
        damaged.set_material(index,instance)
        files.append('CitySimVR/Content/'+target.removeprefix('/Game/')+'.uasset')
    assert unreal.EditorAssetLibrary.save_loaded_asset(damaged)
    catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog')
    definitions=list(catalog.get_editor_property('definitions'))
    matches=0
    for definition in definitions:
        if str(definition.get_editor_property('definition_id')) != 'NuclearPower': continue
        footprint=definition.get_editor_property('footprint')
        assert footprint.get_editor_property('width') in (3,4) and footprint.get_editor_property('height') in (3,4)
        footprint.set_editor_property('width',4);footprint.set_editor_property('height',4)
        definition.set_editor_property('footprint',footprint)
        # Retain the stable original presentation key; Board maps both healthy
        # and damaged keys to the new exact native bodies. No missing suffix.
        assert definition.get_editor_property('presentation_mesh').get_name() == 'SM_Building_NuclearPower_A'
        definition.set_editor_property('logical_height_cm',1185.)
        construction=unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A')
        assert construction
        definition.set_editor_property('construction_mesh',construction)
        matches+=1
    assert matches==1
    catalog.set_editor_property('definitions',definitions)
    assert unreal.EditorAssetLibrary.save_loaded_asset(catalog)
    files.append('CitySimVR/Content/CitySim/Data/DA_CityBuildingCatalog.uasset')
    output=ROOT/'Artifacts/CIAssetPreparation';output.mkdir(parents=True,exist_ok=True)
    previous=(output/'Files.txt').read_text().splitlines() if (output/'Files.txt').exists() else []
    (output/'Files.txt').write_text('\n'.join(dict.fromkeys(previous+files))+'\n')
    receipt=dict(issue=125,integration_issue=192,plot_change='Nuclear3x3 ->4x4; existing saves are preserved, no migration promised',
                 damage_policy='same approved shape/LODs with five dark rough material variants',
                 files=[dict(file=p,sha256=hashlib.sha256((ROOT/p).read_bytes()).hexdigest(),bytes=(ROOT/p).stat().st_size)for p in files])
    (output/'UtilityGround.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('UTILITY_GROUND_FINALIZE_PASS',len(files),'packages')


if __name__=='__main__': main()

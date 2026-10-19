"""Adopt measured specialized plots; retain existing economy and stable mesh keys."""
import unreal
from Assets.Unreal.CityApprovedCivicPlan import ROOT,PLAN,preflight,sha,read,write
FAMILIES={'School':('primary',),'PrimarySchool':('primary',),'SecondarySchool':('secondary',),'College':('college',),
 'Police':('police_base','police_medium','police_high'),'Fire':('fire_base','fire_medium','fire_high'),
 'Hospital':('hospital_base','hospital_medium'),'WasteDepot':('wastedepot_base','wastedepot_medium'),
 'Recycling':('recycling_base','recycling_medium')}
PRESERVED=('cost','jobs_provided','monthly_upkeep','utility_capacity','minimum_staff_education','pollution_emission','construction_steps','entrance_sides','player_purchased_levels')
def main():
    plan=preflight();out=ROOT/'Artifacts/CIAssetPreparation';out.mkdir(parents=True,exist_ok=True)
    receipt=read(ROOT/'ArtSource/Manifests/city_civic_batch_native.json')
    assert receipt['plan_sha256']==sha(PLAN) and receipt['shared_inputs_unchanged']
    for f in receipt['files']:assert sha(ROOT/f['file'])==f['sha256']
    catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog');assert catalog
    definitions=list(catalog.get_editor_property('definitions'));records=[]
    construction=unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A');assert construction
    for definition in definitions:
        identity=str(definition.get_editor_property('definition_id'))
        if identity not in FAMILIES:continue
        assets=[next(a for a in plan['assets'] if a['key']==key) for key in FAMILIES[identity]]
        sites={tuple(a['site_cells']) for a in assets};assert len(sites)==1
        width,height=sites.pop();before={n:definition.get_editor_property(n) for n in PRESERVED}
        levels=[l.export_text() for l in definition.get_editor_property('density_levels')]
        mesh=definition.get_editor_property('presentation_mesh').get_path_name();fp=definition.get_editor_property('footprint');old=[fp.width,fp.height]
        fp.width=width;fp.height=height;definition.set_editor_property('footprint',fp)
        definition.set_editor_property('presentation_yaw_offset_quarter_turns',2)
        definition.set_editor_property('logical_height_cm',max(a['lods'][0]['facts']['bounds_m']['max'][2]*100 for a in assets)+25.)
        definition.set_editor_property('construction_mesh',construction)
        assert {n:definition.get_editor_property(n) for n in PRESERVED}==before
        assert levels==[l.export_text() for l in definition.get_editor_property('density_levels')]
        assert mesh==definition.get_editor_property('presentation_mesh').get_path_name()
        records.append(dict(definition=identity,old_site=old,site=[width,height],economy=before,upgrade_records_unchanged=True))
    assert len(records)>=8
    catalog.set_editor_property('definitions',definitions);assert unreal.EditorAssetLibrary.save_loaded_asset(catalog)
    relative='CitySimVR/Content/CitySim/Data/DA_CityBuildingCatalog.uasset'
    listing=out/'Files.txt';listing.write_text((listing.read_text(encoding='utf-8') if listing.exists() else '')+relative+'\n',encoding='utf-8')
    write(out/'ApprovedCivicCatalog.json',dict(issue=157,plan_sha256=sha(PLAN),definitions=records,files=[dict(file=relative,sha256=sha(ROOT/relative),bytes=(ROOT/relative).stat().st_size)]))
    unreal.log('APPROVED_CIVIC_CATALOG_PASS specialized plots; economy,progression,upgrades unchanged')
if __name__=='__main__':main()

"""Adopt the measured utility reservations after one successful native batch.

Retains stable presentation keys, all authored upgrades and economic values.
The runtime maps those keys to the exact approved native wholes/damaged roles.
"""
from pathlib import Path
import json
import unreal
from Assets.Unreal.CityApprovedUtilityPlan import ROOT,PLAN,read,write,sha,preflight
from Assets.Unreal.CityApprovedUtilitySolar import apply_solar_foundation
from Assets.Unreal.CityApprovedUtilityRender import apply_native_render_expectations

FAMILIES={
    'CoalPower':('coal',), 'OilPower':('oil',),
    'Power':('power_base','power_medium','power_high'),
    'Service':('water_base','water_medium','water_high'),
    'WaterTower':('water_tower',), 'WindPower':('wind_turbine',), 'SolarPower':('solar_farm',)}
PRESERVED=('cost','jobs_provided','monthly_upkeep','utility_capacity','utility_minimum_output_percent',
    'minimum_staff_education','pollution_emission','construction_steps','entrance_sides','player_purchased_levels')


def scalar_values(definition):
    return {name:definition.get_editor_property(name) for name in PRESERVED}


def main():
    assert Path(unreal.Paths.project_dir()).resolve()==(ROOT/'CitySimVR').resolve()
    plan=apply_native_render_expectations(apply_solar_foundation(preflight()));output=ROOT/'Artifacts/CIAssetPreparation'
    native=read(output/'ApprovedUtilityMeshes.json')
    assert native['plan_sha256']==sha(PLAN) and native['actual_native_mesh_checks_run']
    assert native['native_import_overrides']==plan['native_import_overrides']
    assert len(native['files'])==37 and native['shared_inputs_unchanged']
    for item in native['files']:assert sha(ROOT/item['file'])==item['sha256']
    sources=read(ROOT/'ArtSource/Manifests/city_utility_integration.json')
    catalog=unreal.load_asset('/Game/CitySim/Data/DA_CityBuildingCatalog');assert catalog
    construction=unreal.load_asset('/Game/CitySim/Art/Meshes/SM_Building_Residential_Construction_A');assert construction
    definitions=list(catalog.get_editor_property('definitions'));records=[]
    for definition in definitions:
        identity=str(definition.get_editor_property('definition_id'))
        if identity not in FAMILIES:continue
        assets=[next(a for a in sources['assets'] if a['key']==key) for key in FAMILIES[identity]]
        sites={tuple(a['site_cells']) for a in assets};assert len(sites)==1
        width,height=sites.pop()
        before=scalar_values(definition)
        original_mesh=definition.get_editor_property('presentation_mesh').get_path_name()
        levels=[l.export_text() for l in definition.get_editor_property('density_levels')]
        footprint=definition.get_editor_property('footprint')
        old_site=[footprint.get_editor_property('width'),footprint.get_editor_property('height')]
        footprint.set_editor_property('width',width);footprint.set_editor_property('height',height)
        definition.set_editor_property('footprint',footprint)
        definition.set_editor_property('presentation_yaw_offset_quarter_turns',2)
        definition.set_editor_property('logical_height_cm',max(a['lods'][0]['bounds_m']['max'][2]*100 for a in assets)+25.)
        definition.set_editor_property('construction_mesh',construction)
        assert scalar_values(definition)==before
        assert definition.get_editor_property('presentation_mesh').get_path_name()==original_mesh
        assert [l.export_text() for l in definition.get_editor_property('density_levels')]==levels
        records.append(dict(definition=identity,assets=list(FAMILIES[identity]),previous_site=old_site,site=[width,height],
            logical_height_cm=definition.get_editor_property('logical_height_cm'),preserved_economy=before,
            presentation_mesh=original_mesh,upgrade_records_unchanged=True))
    assert len(records)==7
    catalog.set_editor_property('definitions',definitions)
    assert unreal.EditorAssetLibrary.save_loaded_asset(catalog)
    relative='CitySimVR/Content/CitySim/Data/DA_CityBuildingCatalog.uasset'
    listing=output/'Files.txt';files=listing.read_text(encoding='utf-8').splitlines()
    listing.write_text('\n'.join(dict.fromkeys(files+[relative]))+'\n',encoding='utf-8',newline='\n')
    write(output/'ApprovedUtilityCatalog.json',dict(issue=125,plan_sha256=sha(PLAN),
        native_import_overrides=plan['native_import_overrides'],
        native_receipt_sha256=sha(output/'ApprovedUtilityMeshes.json'),definitions=records,
        files=[dict(file=relative,sha256=sha(ROOT/relative),bytes=(ROOT/relative).stat().st_size)],
        old_save_limit='Old utilities with changed reservations fail current footprint validation; player files remain untouched.',
        gameplay_acceptance='Pending integrated runtime, focused native terrain views and Windows playtest'))
    unreal.log('APPROVED_UTILITY_CATALOG_PASS 7 definitions;11 approved versions; economy and upgrades retained')


if __name__=='__main__':main()

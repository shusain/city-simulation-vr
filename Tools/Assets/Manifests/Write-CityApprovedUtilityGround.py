"""Generate native entrance/foundation metadata from the pinned utility batch.

Does not import assets or change approved sources. Run after the final native
plan is selected; generated C++ and metadata are reviewed with runtime adoption.
"""
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCES = ROOT / 'ArtSource/Manifests/city_utility_integration.json'
PLAN = ROOT / 'ArtSource/Manifests/city_utility_native_import_plan.json'
MESH_KEYS = dict(coal='CoalPower_A', oil='OilPower_A', power_base='Power_A',
                 power_medium='Power_Medium_A', power_high='Power_High_A',
                 water_base='Service_A', water_medium='Service_Medium_A',
                 water_high='Service_High_A', water_tower='WaterTower_A',
                 wind_turbine='WindPower_A', solar_farm='SolarPower_A')


def native(point):
    return [point[0]*100, -point[1]*100, point[2]*100]


def numbers(values):
    return ','.join(f'{v:.6f}' for v in values)


def cpp_data(records, plan):
    folder=ROOT/'CitySimVR/Source/CitySimVR/Private/Presentation'
    entrances=['// Generated from measured source portals by Write-CityApprovedUtilityGround.py.']
    bays=['// Generated from measured service leaves by Write-CityApprovedUtilityGround.py.']
    foundations=['// Generated from pinned closed contour units; source XY retained.']
    for record in records:
        key=record['key'];support=key+'_Support'
        variants=[(record['original'],record['healthy']), (record['original']+'_Damaged',record['damaged'])]
        if key=='wind_turbine':
            stationary=next(m for m in plan['wind_role_meshes'] if 'Stationary' in m['key'])
            variants.append((stationary['target'].rsplit('/',1)[1],stationary['target']))
        for original,target in variants:
            entrances.append('    {TEXT("'+original+'"),TEXT("'+target+'"),TEXT("'+support+'"),FVector('+numbers(record['entry_cm'])+'),20,60,'+
                numbers([record['width_cm']])+','+str(record['frontage_flight']).lower()+','+numbers([record['door_width_cm'],record['frontage_half_span_cm']])+'},')
        if record['bay']:
            b=record['bay']
            bays.extend(['    if(FString(Profile->SupportMesh)==TEXT("'+support+'")) {',
                '        static const FCityIndustrialBay Entries[]={{FVector('+numbers(b['entry_cm'])+'),'+numbers([b['width_cm']])+',false,false}};',
                '        return MakeArrayView(Entries);', '    }'])
        for group in record['foundations']:
            foundations.append('    {TEXT("'+support+'"),TEXT("'+group['mesh']+'"),'+numbers([group['top_cm']])+',FVector2D('+numbers(group['min_cm'])+'),FVector2D('+numbers(group['max_cm'])+')},')
    for filename,rows in [('CityApprovedUtilityEntranceData.inl',entrances),('CityApprovedUtilityBayData.inl',bays),('CityApprovedUtilityFoundationData.inl',foundations)]:
        (folder/filename).write_text('\n'.join(rows)+'\n',encoding='utf-8',newline='\n')
    checks=['// Exact expected render LOD counts after pinned native policy; all37 packages.']
    for mesh in plan['whole_meshes']+plan['wind_role_meshes']+plan['foundation_meshes']:
        targets=[mesh['target']]+([mesh['damaged_target']] if mesh['kind']=='whole' else [])
        counts=[lod['triangles'] for lod in mesh['lods']]
        for target in targets:
            checks.append('    {TEXT("'+target+'"),'+str(len(counts))+',{'+','.join(str(c) for c in counts+[0]*(3-len(counts)))+'}},')
    (ROOT/'CitySimVR/Source/CitySimVR/Private/Tests/CityApprovedUtilityMeshChecks.inl').write_text('\n'.join(checks)+'\n',encoding='utf-8',newline='\n')


def main():
    source = json.loads(SOURCES.read_bytes())
    sys.path.insert(0,str(ROOT/'Tools'))
    from Assets.Unreal.CityApprovedUtilityPlan import preflight
    from Assets.Unreal.CityApprovedUtilitySolar import apply_solar_foundation
    from Assets.Unreal.CityApprovedUtilityRender import apply_native_render_expectations
    plan = apply_native_render_expectations(apply_solar_foundation(preflight()))
    assert hashlib.sha256(SOURCES.read_bytes()).hexdigest() == 'ee3b1272d3266199480262c05e45c6d0b07173174521b407c56ba808cc0c77df'
    assert set(MESH_KEYS) == {a['key'] for a in source['assets']}
    records = []
    for asset in source['assets']:
        key = asset['key']
        mesh = next(m for m in plan['whole_meshes'] if m['key'] == key)
        portals = asset['portals']
        door = next(p for p in portals if p['kind'] != 'service_bay')
        bounds = door['actual_portal_bounds_m']
        anchor = door['source_anchor']
        entry = native([anchor['xyz_m'][0], bounds['min'][1], door['actual_sill_z_m']])
        ladder = door['kind'] == 'ladder_access'
        if ladder:
            # The authored ladder consists of two rails; the walkway approaches
            # their centre rather than pretending either rail is a door leaf.
            entry[0] = anchor['xyz_m'][0]*100
        width = anchor['boundary_connector']['width_m']*100
        door_width = min(anchor['width_m']*100, (bounds['max'][0]-bounds['min'][0])*100) if not ladder else width
        half_site_x = asset['site_cells'][0]*400
        span = min(760, half_site_x-abs(entry[0])-30)
        assert span > width*.5
        bay = next((p for p in portals if p['kind'] == 'service_bay'), None)
        bay_data = None
        if bay:
            bay_data = dict(entry_cm=native([bay['source_anchor']['xyz_m'][0], bay['actual_portal_bounds_m']['min'][1], bay['actual_sill_z_m']]),
                            width_cm=bay['source_anchor']['width_m']*100)
            assert abs(entry[0]-bay_data['entry_cm'][0]) > (width+bay_data['width_cm'])*.5
        foundation_meshes = [m for m in plan['foundation_meshes'] if m['issue'] == asset['issue']]
        assert foundation_meshes
        groups = []
        for foundation in foundation_meshes:
            b = foundation['lods'][0]['facts']['bounds_m']
            groups.append(dict(mesh=foundation['target'], top_cm=foundation['source_support_top_z_m']*100,
                               min_cm=native([b['min'][0], b['max'][1], 0])[:2],
                               max_cm=native([b['max'][0], b['min'][1], 0])[:2]))
        records.append(dict(key=key, issue=asset['issue'], version=asset['version'],
                            original='SM_Building_'+MESH_KEYS[key], healthy=mesh['target'], damaged=mesh['damaged_target'],
                            site_cells=asset['site_cells'], entry_cm=entry, width_cm=width, door_width_cm=door_width,
                            frontage_flight=key in ('water_tower', 'solar_farm'), frontage_half_span_cm=span,
                            ladder_access=ladder, bay=bay_data, foundations=groups))
    output = dict(source_manifest_sha256=hashlib.sha256(SOURCES.read_bytes()).hexdigest(),
                  native_plan_sha256=hashlib.sha256(PLAN.read_bytes()).hexdigest(),
                  native_import_overrides=plan['native_import_overrides'],
                  source_to_native='[x,-y,z]*100; entity presentation yaw offset2; wind nacelle has separate weather yaw',
                  portal_policy='Actual leaf front/sill; ladder centred approach; nominal portal anchors are not substituted for measured leaf bounds',
                  assets=records)
    target = ROOT/'ArtSource/Manifests/city_approved_utility_ground.json'
    target.write_text(json.dumps(output, indent=2)+'\n', encoding='utf-8', newline='\n')
    cpp_data(records,plan)
    print('APPROVED_UTILITY_GROUND_METADATA_READY', len(records), 'assets;', sum(len(r['foundations']) for r in records), 'foundation groups; no native execution')


if __name__ == '__main__':
    main()

"""Generate measured runtime entrances/supports/checks for #157 approved batch."""
from pathlib import Path
import json,sys
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'Tools'))
from Assets.Unreal.CityApprovedCivicPlan import preflight,write
KEYS={'primary':'PrimarySchool_A','secondary':'SecondarySchool_A','college':'College_A',
      'police_base':'Police_A','police_medium':'Police_Medium_A','police_high':'Police_High_A',
      'fire_base':'Fire_A','fire_medium':'Fire_Medium_A','fire_high':'Fire_High_A',
      'hospital_base':'Hospital_A','hospital_medium':'Hospital_Medium_A',
      'wastedepot_base':'WasteDepot_A','wastedepot_medium':'WasteDepot_Medium_A',
      'recycling_base':'Recycling_A','recycling_medium':'Recycling_Medium_A'}
def numbers(v):return ','.join(f'{x:.6f}' for x in v)
def native(v):return [v[0]*100,-v[1]*100,v[2]*100]
def main():
    plan=preflight();entrances=[];bays=[];supports=[];plots=[];checks=[];records=[]
    for a in plan['assets']:
        key=a['key'];support=key+'_ApprovedSupport';old='SM_Building_'+KEYS[key]
        damage=old[:-2]+'_Damaged_A' if key.startswith(('hospital','wastedepot','recycling')) else old+'_Damaged'
        aliases=[(old,a['target']),(damage,a['damaged_target'])]
        if key=='primary':aliases += [('SM_Building_School_A',a['target']),('SM_Building_School_A_Damaged',a['damaged_target'])]
        door=next(p for p in a['portals'] if p['role']=='pedestrian');entry=native(door['entry_m']);width=door['width_m']*100
        for original,target in aliases:
            entrances.append('        {TEXT("'+original+'"),TEXT("'+target+'"),TEXT("'+support+'"),FVector('+numbers(entry)+'),20,60,'+numbers([width])+',false,'+numbers([width, min(760,a['site_cells'][0]*400-abs(entry[0])-30)])+'},')
        regions=[]
        for s in a['supports']:
            b=s['bounds_m'];low=[b['min'][0]*100,-b['max'][1]*100];high=[b['max'][0]*100,-b['min'][1]*100]
            regions.append('            {'+numbers(low+high+[s['top_m']*100])+'},')
        supports += ['    if(Name==TEXT("'+support+'")) {','        static const FCityBuildingSupportRegion Regions[]={']+regions+['        };','        return MakeArrayView(Regions);','    }']
        plots.append('    if(FString(Entry->SupportMesh)==TEXT("'+support+'")) return FIntPoint('+','.join(str(x) for x in a['site_cells'])+');')
        lookup={c['name']:c['bounds_m'] for c in a['components']};lanes=[]
        for b in (a['vehicle_bays'] or {}).get('bays',[]):
            actual=lookup[b['target']];point=[b['xyz_m'][0],actual['min'][1],actual['min'][2]]
            lanes.append(dict(entry_cm=native(point),width_cm=b['width_m']*100))
        # Staff/service doors of education are secondary narrow approaches.
        if not a['vehicle_bays']:
            lanes=[dict(entry_cm=native(p['entry_m']),width_cm=p['width_m']*100) for p in a['portals'] if p['role']=='service']
        bays += ['    if(FString(Profile->SupportMesh)==TEXT("'+support+'")) {','        static const FCityIndustrialBay Entries[]={']
        bays += ['            {FVector('+numbers(b['entry_cm'])+'),'+numbers([b['width_cm']])+',false,false},' for b in lanes]
        bays += ['        };','        return MakeArrayView(Entries);','    }']
        for target in (a['target'],a['damaged_target']):
            checks.append('    {TEXT("'+target+'"),3,{'+','.join(str(l['triangles']) for l in a['lods'])+'}},')
        records.append(dict(key=key,original=old,damaged_original=damage,healthy=a['target'],damaged=a['damaged_target'],site=a['site_cells'],entry_cm=entry,width_cm=width,bays=lanes,support_key=support))
    folder=ROOT/'CitySimVR/Source/CitySimVR/Private/Presentation'
    (folder/'CityCivicEntrances.cpp').write_text('''#include "Presentation/CityCivicEntrances.h"
#include "Misc/Paths.h"
bool CityCivicEntrances::IsDefinition(FName Id)
{
    return Id==TEXT("Police") || Id==TEXT("Fire") || Id==TEXT("School") || Id==TEXT("PrimarySchool") ||
        Id==TEXT("SecondarySchool") || Id==TEXT("College") || Id==TEXT("Hospital") ||
        Id==TEXT("WasteDepot") || Id==TEXT("Recycling");
}
TConstArrayView<FCityResidentialEntrance> CityCivicEntrances::All()
{
    static const FCityResidentialEntrance Entries[]={
#include "CityApprovedCivicEntranceData.inl"
    };
    return MakeArrayView(Entries);
}
const FCityResidentialEntrance* CityCivicEntrances::Find(const FString& MeshName)
{
    for(const auto& Entry:All())
        if(MeshName==Entry.OriginalMesh || MeshName==FPaths::GetBaseFilename(Entry.TargetMesh)) return &Entry;
    return nullptr;
}
TConstArrayView<FCityIndustrialBay> CityCivicEntrances::Bays(const FString& MeshName)
{
    const auto* Profile=Find(MeshName);
    if(!Profile) return {};
#include "CityApprovedCivicBayData.inl"
    return {};
}
FIntPoint CityCivicEntrances::Footprint(const FString& MeshName)
{
    const auto* Entry=Find(MeshName);
    if(!Entry) return FIntPoint(0,0);
#include "CityApprovedCivicPlotData.inl"
    return FIntPoint(0,0);
}
''',encoding='utf-8',newline='\n')
    for name,rows in [('CityApprovedCivicEntranceData.inl',entrances),('CityApprovedCivicBayData.inl',bays),('CityApprovedCivicSupportData.inl',supports),('CityApprovedCivicPlotData.inl',plots)]:
        (folder/name).write_text('// Generated from exact #157 source measurements.\n'+'\n'.join(rows)+'\n',encoding='utf-8',newline='\n')
    (folder.parent/'Tests/CityApprovedCivicMeshChecks.inl').write_text('// Exact batch render counts for all cooked variants.\n'+'\n'.join(checks)+'\n',encoding='utf-8',newline='\n')
    ids={'primary':'PrimarySchool','secondary':'SecondarySchool','college':'College','police':'Police','fire':'Fire','hospital':'Hospital','wastedepot':'WasteDepot','recycling':'Recycling'}
    cases=[]
    for a in plan['assets']:
        level=2 if a['key'].endswith('_high') else 1 if a['key'].endswith('_medium') else 0
        cases.append('    {TEXT("'+a['key']+'"),TEXT("'+ids[a['key'].split('_')[0]]+'"),TEXT("'+a['target'].rsplit('/',1)[1]+'"),'+str(level)+',0},')
    cases += ['    {TEXT("police-high-damaged"),TEXT("Police"),TEXT("SM_Approved_police_high_v4_Damaged"),2,1},',
              '    {TEXT("waste-construction"),TEXT("WasteDepot"),TEXT("SM_ResidentialGround_Construction"),0,2},']
    (folder.parent/'Tests/CityApprovedCivicStudyCases.inl').write_text('\n'.join(cases)+'\n',encoding='utf-8',newline='\n')
    write(ROOT/'ArtSource/Manifests/city_civic_batch_runtime.json',dict(issue=157,assets=records))
    print('CIVIC_RUNTIME_METADATA_PASS',len(records),'sources',sum(len(a['supports']) for a in plan['assets']),'structural regions')
if __name__=='__main__':main()

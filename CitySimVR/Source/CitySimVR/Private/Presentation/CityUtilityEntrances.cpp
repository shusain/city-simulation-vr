#include "Presentation/CityUtilityEntrances.h"
#include "Misc/Paths.h"
bool CityUtilityEntrances::IsDefinition(FName Id)
{
    return Id==TEXT("NuclearPower") || Id==TEXT("CoalPower") || Id==TEXT("OilPower") ||
        Id==TEXT("Power") || Id==TEXT("Service") || Id==TEXT("WaterTower") ||
        Id==TEXT("WindPower") || Id==TEXT("SolarPower");
}
TConstArrayView<FCityResidentialEntrance> CityUtilityEntrances::All()
{
    static const FCityResidentialEntrance Entries[]={
        {TEXT("SM_Building_NuclearPower_A"),TEXT("/Game/CitySim/Art/Utilities/Nuclear/SM_NuclearFacility_v6"),TEXT("Nuclear_v6_Support"),FVector(-900,1176,0),20,60,180,false,120},
        {TEXT("SM_Building_NuclearPower_A_Damaged"),TEXT("/Game/CitySim/Art/Utilities/Nuclear/SM_NuclearFacility_v6_Damaged"),TEXT("Nuclear_v6_Support"),FVector(-900,1176,0),20,60,180,false,120},
#include "CityApprovedUtilityEntranceData.inl"
    };
    return MakeArrayView(Entries);
}
const FCityResidentialEntrance* CityUtilityEntrances::Find(const FString& MeshName)
{
    for(const auto& Entry:All())
        if(MeshName==Entry.OriginalMesh || MeshName==FPaths::GetBaseFilename(Entry.TargetMesh)) return &Entry;
    return nullptr;
}
TConstArrayView<FCityIndustrialBay> CityUtilityEntrances::Bays(const FString& MeshName)
{
    const auto* Profile=Find(MeshName);
    if(!Profile) return {};
#include "CityApprovedUtilityBayData.inl"
    if(FString(Profile->SupportMesh)!=TEXT("Nuclear_v6_Support")) return {};
    static const FCityIndustrialBay Entries[]={{FVector(800,880,0),450,false,false}};
    return MakeArrayView(Entries);
}

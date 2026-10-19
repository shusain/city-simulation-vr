#include "Presentation/CityCivicEntrances.h"
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

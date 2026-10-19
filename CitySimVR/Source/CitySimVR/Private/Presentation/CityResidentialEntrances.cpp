#include "Presentation/CityResidentialEntrances.h"
#include "Misc/Paths.h"

TConstArrayView<FCityResidentialEntrance> CityResidentialEntrances::All()
{
	static const FCityResidentialEntrance Entries[] = {
		{TEXT("SM_Building_Residential_Medium_A"),TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_Medium"),TEXT("SM_Building_Residential_Medium_A"),FVector(0,730,0),10,80,100,true,300},
		{TEXT("SM_Building_Residential_Medium_A_Damaged"),TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_Medium_Damaged"),TEXT("SM_Building_Residential_Medium_A"),FVector(0,730,0),10,80,100,true,300},
		{TEXT("SM_Building_Residential_High_A"),TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_High"),TEXT("SM_Building_Residential_High_A"),FVector(0,680,0),10,60,130,true,300},
		{TEXT("SM_Building_Residential_High_A_Damaged"),TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_High_Damaged"),TEXT("SM_Building_Residential_High_A"),FVector(0,680,0),10,60,130,true,300},
		{TEXT("SM_Building_Residential_Construction_A"),TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_Construction"),TEXT("SM_Building_Residential_Construction_A"),FVector(0,750,80),10,80,80,true,300},
		{TEXT("SM_Building_Residential_Construction_A_Damaged"),TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_Construction_Damaged"),TEXT("SM_Building_Residential_Construction_A"),FVector(0,750,80),10,80,80,true,300},
		{TEXT("SM_LowDensity_Home_A"), TEXT("/Game/CitySim/Art/HomeGroundPilot/SM_CompactHome_GroundPilot"), TEXT("SM_LowDensity_Home_A"), FVector(0,467,0), 50,100},
		{TEXT("SM_Building_Residential_A"), TEXT("/Game/CitySim/Art/HomeGroundPilot/SM_CompactHome_GroundPilot"), TEXT("SM_LowDensity_Home_A"), FVector(0,467,0), 50,100},
		{TEXT("SM_Building_Residential_SeasonalSpace_A"), TEXT("/Game/CitySim/Art/HomeGroundPilot/SM_CompactHome_GroundPilot"), TEXT("SM_LowDensity_Home_A"), FVector(0,467,0), 50,100},
		{TEXT("SM_LowDensity_Home_A_Damaged"), TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_Home_A_Damaged"), TEXT("SM_LowDensity_Home_A"), FVector(0,467,0), 50,100},
		{TEXT("SM_LowDensity_Home_B"), TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_Home_B"), TEXT("SM_LowDensity_Home_B"), FVector(0,615,0), 10,40},
		{TEXT("SM_LowDensity_Home_B_Damaged"), TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_Home_B_Damaged"), TEXT("SM_LowDensity_Home_B"), FVector(0,615,0), 10,40},
		{TEXT("SM_LowDensity_Home_C"), TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_Home_C"), TEXT("SM_LowDensity_Home_C"), FVector(0,603,0), 10,40},
		{TEXT("SM_LowDensity_Home_C_Damaged"), TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_Home_C_Damaged"), TEXT("SM_LowDensity_Home_C"), FVector(0,603,0), 10,40},
		{TEXT("SM_LowDensity_Home_D"), TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_Home_D"), TEXT("SM_LowDensity_Home_D"), FVector(0,629,0), 10,40},
		{TEXT("SM_LowDensity_Home_D_Damaged"), TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_Home_D_Damaged"), TEXT("SM_LowDensity_Home_D"), FVector(0,629,0), 10,40},
		{TEXT("SM_Building_Residential_A_Damaged"), TEXT("/Game/CitySim/Art/ResidentialGround/SM_ResidentialGround_Home_Legacy_Damaged"), TEXT("SM_Building_Residential_A"), FVector(0,467,0), 50,100}
	};
	return MakeArrayView(Entries);
}

const FCityResidentialEntrance* CityResidentialEntrances::Find(const FString& MeshName)
{
	for (const auto& Entry : All())
		if (MeshName == Entry.OriginalMesh || MeshName == FPaths::GetBaseFilename(Entry.TargetMesh))
			return &Entry;
	return nullptr;
}

FString CityResidentialEntrances::ObjectPath(const FCityResidentialEntrance& Entry)
{
	return FString(Entry.TargetMesh) + TEXT(".") + FPaths::GetBaseFilename(Entry.TargetMesh);
}

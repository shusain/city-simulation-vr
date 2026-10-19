#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityGrowthSimulation.h"

bool FCityBuildingDefinition::PrerequisitesMet(const FCityServiceSnapshot* S) const
{
	if (RequiredOperatingFacilities.IsEmpty() && !bRequiresAlternativePower)
		return true;
	if (!S || (bRequiresAlternativePower && S->OperatingAlternativePower <= 0))
		return false;
	for (const FName Required : RequiredOperatingFacilities)
		if (S->OperatingFacilities.FindRef(Required) <= 0)
			return false;
	return true;
}
FString FCityBuildingDefinition::DescribePrerequisites(const FCityServiceSnapshot& S,
                                                       const UCityBuildingCatalog* Catalog) const
{
	FString Text;
	for (const FName Required : RequiredOperatingFacilities)
	{
		const auto* D = Catalog ? Catalog->FindDefinition(Required) : nullptr;
		if (!Text.IsEmpty())
			Text += TEXT("\n");
		Text += FString::Printf(TEXT("%s %s operating"),
		                        S.OperatingFacilities.FindRef(Required) > 0 ? TEXT("READY") : TEXT("NEED"),
		                        D ? *D->DisplayName : *Required.ToString());
	}
	if (bRequiresAlternativePower)
	{
		if (!Text.IsEmpty())
			Text += TEXT("\n");
		Text += FString::Printf(TEXT("%s Alternative power producing"),
		                        S.OperatingAlternativePower > 0 ? TEXT("READY") : TEXT("NEED"));
	}
	return Text;
}

bool FCityBuildingLevelDefinition::IsValid() const
{
	return UtilitySupplyPercent >= 100 && UtilitySupplyPercent <= 300 && ResidentsProvided >= 0 && JobsProvided >= 0 &&
	       EconomicOutput >= 0 && PowerDemand >= 0 && WaterDemand >= 0 && PollutionEmission >= 0 &&
	       MonthlyUpkeep >= 0 && UpgradeCost >= 0 && MinimumOperationalSteps >= 0 && CivicConstructionSteps >= 0 &&
	       CivicConstructionSteps <= 3650 && CivicRoadRange >= 0 && CivicRoadRange <= 128 &&
	       CivicProtectionCapacity >= 0 && CivicProtectionCapacity <= 10000 && CivicResponseVehicles >= 0 &&
	       CivicResponseVehicles <= 3 && MinimumDesirability >= 0 && MinimumDesirability <= 100 &&
	       MinimumDemand >= -100 && MinimumDemand <= 100;
}

bool FCityBuildingDefinition::IsValid() const
{
	constexpr int32 ValidEntranceMask = static_cast<int32>(ECityBuildingEntranceSide::NegativeY) |
	                                    static_cast<int32>(ECityBuildingEntranceSide::PositiveX) |
	                                    static_cast<int32>(ECityBuildingEntranceSide::PositiveY) |
	                                    static_cast<int32>(ECityBuildingEntranceSide::NegativeX);
	TSet<ECityDensityLevel> UniqueLevels;
	TSet<FName> UniquePrerequisites;
	for (const FName Required : RequiredOperatingFacilities)
	{
		if (Required.IsNone() || Required == DefinitionId || UniquePrerequisites.Contains(Required))
			return false;
		UniquePrerequisites.Add(Required);
	}
	for (const FCityBuildingLevelDefinition& Level : DensityLevels)
	{
		if (!Level.IsValid() || UniqueLevels.Contains(Level.Level))
		{
			return false;
		}
		UniqueLevels.Add(Level.Level);
		if (bPlayerPurchasedLevels &&
		    (DefinitionId == TEXT("Police") || DefinitionId == TEXT("Fire") || DefinitionId == TEXT("Hospital") ||
		     DefinitionId == TEXT("WasteDepot") || DefinitionId == TEXT("Recycling")) &&
		    (Level.CivicRoadRange <= 0 || Level.CivicProtectionCapacity <= 0 || Level.CivicResponseVehicles <= 0 ||
		     Level.JobsProvided <= 0 ||
		     (Level.Level != ECityDensityLevel::Low && (Level.UpgradeCost <= 0 || Level.CivicConstructionSteps <= 0))))
			return false;
	}

	if (bPlayerPurchasedLevels && (DefinitionId == TEXT("Power") || DefinitionId == TEXT("Service")))
	{
		for (const auto& Level : DensityLevels)
			if (Level.JobsProvided <= 0 || (Level.Level != ECityDensityLevel::Low &&
			                                (Level.UpgradeCost <= 0 || Level.CivicConstructionSteps <= 0)))
				return false;
	}
	const bool WasteLevels = (DefinitionId == TEXT("WasteDepot") || DefinitionId == TEXT("Recycling")) &&
	                         UniqueLevels.Num() == 2 && UniqueLevels.Contains(ECityDensityLevel::Low) &&
	                         UniqueLevels.Contains(ECityDensityLevel::Medium);
	const bool HospitalLevels = DefinitionId == TEXT("Hospital") && UniqueLevels.Num() == 2 &&
	                            UniqueLevels.Contains(ECityDensityLevel::Low) &&
	                            UniqueLevels.Contains(ECityDensityLevel::Medium) && MinimumStaffEducation >= 2;
	return (!bPlayerPurchasedLevels ||
	        ((WasteLevels || HospitalLevels ||
	          ((DefinitionId == TEXT("Police") || DefinitionId == TEXT("Fire") || DefinitionId == TEXT("Power") ||
	            DefinitionId == TEXT("Service")) &&
	           UniqueLevels.Num() == 3 && UniqueLevels.Contains(ECityDensityLevel::Low) &&
	           UniqueLevels.Contains(ECityDensityLevel::Medium) && UniqueLevels.Contains(ECityDensityLevel::High))) &&
	         DevelopmentDemandType == ECityDevelopmentDemandType::None)) &&
	       !DefinitionId.IsNone() && !DisplayName.IsEmpty() && Footprint.Width > 0 && Footprint.Height > 0 &&
	       Cost >= 0 && LogicalHeightCm > 0.0 && (EntranceSides & ValidEntranceMask) != 0 &&
	       (EntranceSides & ~ValidEntranceMask) == 0 && PresentationYawOffsetQuarterTurns >= 0 &&
	       PresentationYawOffsetQuarterTurns <= 3 && UtilityCapacity >= 0 && UtilityCapacity <= 5000 &&
	       UtilityMinimumOutputPercent >= 0 && UtilityMinimumOutputPercent <= 100 && MinimumStaffEducation >= 0 &&
	       MinimumStaffEducation <= 3 && JobsProvided >= 0 && EconomicOutput >= 0 && PowerDemand >= 0 &&
	       WaterDemand >= 0 && PollutionEmission >= 0 && MonthlyUpkeep >= 0 && ConstructionSteps >= 0 &&
	       ParkRadiusCells >= 0 && ParkRadiusCells <= 64 && ParkDesirabilityBonus >= 0 && ParkDesirabilityBonus <= 100;
}

const FCityBuildingLevelDefinition* FCityBuildingDefinition::FindDensityLevel(const ECityDensityLevel Level) const
{
	return DensityLevels.FindByPredicate([Level](const FCityBuildingLevelDefinition& Candidate)
	                                     { return Candidate.Level == Level; });
}

const FCityBuildingDefinition* UCityBuildingCatalog::FindDefinition(const FName DefinitionId) const
{
	return Definitions.FindByPredicate([DefinitionId](const FCityBuildingDefinition& Definition)
	                                   { return Definition.DefinitionId == DefinitionId; });
}

bool FCityBuildingDefinition::IsPowerFacility(FName Id)
{
	return Id == TEXT("Power") || Id == TEXT("CoalPower") || Id == TEXT("OilPower") || Id == TEXT("WindPower") ||
	       Id == TEXT("SolarPower") || Id == TEXT("NuclearPower");
}

TSoftObjectPtr<UStaticMesh> UCityBuildingCatalog::FindCommercialPresentation(uint64 EntityId,
                                                                             const ECityDensityLevel Density,
                                                                             const bool bDamaged,
                                                                             FName& OutBusinessId) const
{
	OutBusinessId = NAME_None;
	if (EntityId == 0 || CommercialPresentationVariants.IsEmpty())
		return {};
	// Explicit integer mixing stays identical across platforms and save/load;
	// density, damage, time and rendering order never enter the choice.
	uint64 Key = EntityId + 0x9e3779b97f4a7c15ULL;
	Key = (Key ^ (Key >> 30)) * 0xbf58476d1ce4e5b9ULL;
	Key = (Key ^ (Key >> 27)) * 0x94d049bb133111ebULL;
	Key ^= Key >> 31;
	const auto& Variant = CommercialPresentationVariants[Key % CommercialPresentationVariants.Num()];
	const auto& Meshes = bDamaged ? Variant.DamagedDensityMeshes : Variant.DensityMeshes;
	const int32 Index = static_cast<int32>(Density);
	if (Variant.BusinessId.IsNone() || !Meshes.IsValidIndex(Index) || Meshes[Index].IsNull())
		return {};
	OutBusinessId = Variant.BusinessId;
	return Meshes[Index];
}
bool FCityBuildingDefinition::IsWaterFacility(FName Id)
{
	return Id == TEXT("Service") || Id == TEXT("WaterTower");
}

TSoftObjectPtr<UStaticMesh> UCityBuildingCatalog::FindLowDensityPresentation(
    const uint64 EntityId, const FName DefinitionId, const ECityDensityLevel Density,
    const bool bDamaged, FName& OutAppearanceId) const
{
	OutAppearanceId = NAME_None;
	if (EntityId == 0 || Density != ECityDensityLevel::Low)
		return {};
	int32 Count = 0;
	for (const auto& Variant : LowDensityPresentationVariants)
		if (Variant.DefinitionId == DefinitionId)
			++Count;
	if (Count == 0)
		return {};
	// Separate mixing from business choice avoids correlating a shop sign with
	// one building shape. No density, damage, clock or render order enters it.
	uint64 Key = (EntityId ^ 0xd104b19d38a764efULL) + 0x9e3779b97f4a7c15ULL;
	Key = (Key ^ (Key >> 30)) * 0xbf58476d1ce4e5b9ULL;
	Key = (Key ^ (Key >> 27)) * 0x94d049bb133111ebULL;
	Key ^= Key >> 31;
	int32 Index = static_cast<int32>(Key % Count);
	for (const auto& Variant : LowDensityPresentationVariants)
	{
		if (Variant.DefinitionId != DefinitionId || Index-- != 0)
			continue;
		auto Mesh = bDamaged ? Variant.DamagedMesh : Variant.PresentationMesh;
		if (!bDamaged && DefinitionId == TEXT("Shop"))
		{
			FName Business;
			FindCommercialPresentation(EntityId, Density, false, Business);
			if (const auto* SignedMesh = Variant.BusinessMeshes.Find(Business))
				Mesh = *SignedMesh;
		}
		if (!Variant.AppearanceId.IsNone() && !Mesh.IsNull())
			OutAppearanceId = Variant.AppearanceId;
		return Mesh;
	}
	return {};
}
bool FCityBuildingDefinition::IsAlternativeUtility(FName Id)
{
	return (IsPowerFacility(Id) || IsWaterFacility(Id)) && Id != TEXT("Power") && Id != TEXT("Service");
}

bool FCityBuildingDefinition::IsSchoolFacility(FName Id)
{
	return Id == TEXT("School") || Id == TEXT("PrimarySchool") || Id == TEXT("SecondarySchool") ||
	       Id == TEXT("College");
}
bool FCityBuildingDefinition::CanTeachEducation(FName Id, int32 Education)
{
	if (Id == TEXT("School"))
		return Education < 2; // Preserve existing combined-school progress.
	if (Id == TEXT("PrimarySchool"))
		return Education == 0;
	if (Id == TEXT("SecondarySchool"))
		return Education == 1;
	return Id == TEXT("College") && Education == 2;
}

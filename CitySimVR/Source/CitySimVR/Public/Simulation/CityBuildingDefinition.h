#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Simulation/CityGrid.h"
#include "CityBuildingDefinition.generated.h"

class UStaticMesh;
class UCityBuildingCatalog;
struct FCityServiceSnapshot;

UENUM(BlueprintType)
enum class ECityDensityLevel : uint8
{
	Low,
	Medium,
	High
};

UENUM(BlueprintType)
enum class ECityDevelopmentDemandType : uint8
{
	None,
	Residential,
	Commercial,
	Industrial
};

USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityBuildingLevelDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development")
	ECityDensityLevel Level = ECityDensityLevel::Low;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Presentation")
	TSoftObjectPtr<UStaticMesh> PresentationMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Simulation", meta = (ClampMin = "0"))
	int32 ResidentsProvided = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Simulation", meta = (ClampMin = "0"))
	int32 JobsProvided = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Simulation", meta = (ClampMin = "0"))
	int32 EconomicOutput = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Simulation", meta = (ClampMin = "0"))
	int32 PowerDemand = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Simulation", meta = (ClampMin = "0"))
	int32 WaterDemand = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Simulation", meta = (ClampMin = "0"))
	int32 PollutionEmission = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Simulation", meta = (ClampMin = "0"))
	int64 MonthlyUpkeep = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Upgrade", meta = (ClampMin = "0"))
	int64 UpgradeCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Upgrade", meta = (ClampMin = "0"))
	int64 MinimumOperationalSteps = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Upgrade",
	          meta = (ClampMin = "0", ClampMax = "100"))
	int32 MinimumDesirability = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Upgrade",
	          meta = (ClampMin = "-100", ClampMax = "100"))
	int32 MinimumDemand = 0;

	// Multiplies the existing utility tuning; starter output remains unchanged.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Utility",
	          meta = (ClampMin = "100", ClampMax = "300"))
	int32 UtilitySupplyPercent = 100;

	bool IsValid() const;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Civic",
	          meta = (ClampMin = "0", ClampMax = "3650"))
	int32 CivicConstructionSteps = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Civic",
	          meta = (ClampMin = "0", ClampMax = "128"))
	int32 CivicRoadRange = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Civic",
	          meta = (ClampMin = "0", ClampMax = "10000"))
	int32 CivicProtectionCapacity = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Development|Civic", meta = (ClampMin = "0", ClampMax = "3"))
	int32 CivicResponseVehicles = 0;
};

UENUM(BlueprintType, meta = (Bitflags))
enum class ECityBuildingEntranceSide : uint8
{
	None = 0 UMETA(Hidden),
	NegativeY = 1 << 0 UMETA(DisplayName = "Local -Y"),
	PositiveX = 1 << 1 UMETA(DisplayName = "Local +X"),
	PositiveY = 1 << 2 UMETA(DisplayName = "Local +Y"),
	NegativeX = 1 << 3 UMETA(DisplayName = "Local -X")
};
ENUM_CLASS_FLAGS(ECityBuildingEntranceSide);

USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityBuildingDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	FName DefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building")
	FCityCellFootprint Footprint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building", meta = (ClampMin = "0"))
	int64 Cost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building", meta = (ClampMin = "1.0"))
	double LogicalHeightCm = 800.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Presentation")
	TSoftObjectPtr<UStaticMesh> PresentationMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Development")
	TSoftObjectPtr<UStaticMesh> ConstructionMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Development", meta = (ClampMin = "0"))
	int64 ConstructionSteps = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Development")
	TArray<FCityBuildingLevelDefinition> DensityLevels;

	// Explicit demand channel for upgrades. Future commercial and industrial
	// density families use the same lifecycle without relying on mesh names.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Development")
	ECityDevelopmentDemandType DevelopmentDemandType = ECityDevelopmentDemandType::None;
	// Reuse saved development levels, but purchase explicitly instead of density growth.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Development")
	bool bPlayerPurchasedLevels = false;

	// Gameplay-local footprint edges that can connect to a road. Bit order is
	// -Y, +X, +Y, -X so quarter-turn rotation advances through the same order.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Access",
	          meta = (Bitmask, BitmaskEnum = "/Script/CitySimVR.ECityBuildingEntranceSide"))
	int32 EntranceSides = static_cast<int32>(ECityBuildingEntranceSide::NegativeY);

	// Aligns an imported presentation mesh to gameplay-local axes without
	// changing saved logical rotation or the declared entrance sides.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Presentation",
	          meta = (ClampMin = "0", ClampMax = "3"))
	int32 PresentationYawOffsetQuarterTurns = 0;

	// Authoritative operating values for specialized content. Existing prototype
	// home/shop tuning remains compatible while new industrial families source
	// these values from the catalog.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Simulation", meta = (ClampMin = "0"))
	int32 JobsProvided = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Simulation", meta = (ClampMin = "0"))
	int32 EconomicOutput = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Simulation", meta = (ClampMin = "0"))
	int32 PowerDemand = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Simulation", meta = (ClampMin = "0"))
	int32 WaterDemand = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Simulation", meta = (ClampMin = "0"))
	int32 PollutionEmission = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Simulation", meta = (ClampMin = "0"))
	int64 MonthlyUpkeep = 0;

	bool IsValid() const;
	// Distinct facilities use explicit supply; zero preserves the original tuning.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Utility",
	          meta = (ClampMin = "0", ClampMax = "5000"))
	int32 UtilityCapacity = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Utility", meta = (ClampMin = "0", ClampMax = "100"))
	int32 UtilityMinimumOutputPercent = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Staff", meta = (ClampMin = "0", ClampMax = "3"))
	int32 MinimumStaffEducation = 1;
	// Shared construction locks for ordinary and late-game facilities. Each listed
	// type needs at least one currently operating provider. Empty means unlocked.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Progression")
	TArray<FName> RequiredOperatingFacilities;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Progression")
	bool bRequiresAlternativePower = false;
	bool PrerequisitesMet(const FCityServiceSnapshot* Services) const;
	FString DescribePrerequisites(const FCityServiceSnapshot& Services,
	                              const UCityBuildingCatalog* Catalog) const;
	static bool IsSchoolFacility(FName Id);
	static bool CanTeachEducation(FName Id, int32 Education);
	static bool IsPowerFacility(FName Id);
	static bool IsWaterFacility(FName Id);
	static bool IsAlternativeUtility(FName Id);
	const FCityBuildingLevelDefinition* FindDensityLevel(ECityDensityLevel Level) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Park", meta = (ClampMin = "0", ClampMax = "64"))
	int32 ParkRadiusCells = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building|Park", meta = (ClampMin = "0", ClampMax = "100"))
	int32 ParkDesirabilityBonus = 0;
};

USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityCommercialPresentationVariant
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FName BusinessId;
	// Fixed low/medium/high order, shared across normal and damaged assets.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TArray<TSoftObjectPtr<UStaticMesh>> DensityMeshes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TArray<TSoftObjectPtr<UStaticMesh>> DamagedDensityMeshes;
};

USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityLowDensityPresentationVariant
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FName DefinitionId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FName AppearanceId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UStaticMesh> PresentationMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UStaticMesh> DamagedMesh;
	// Shop signs retain the independently selected, saved business identity.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TMap<FName, TSoftObjectPtr<UStaticMesh>> BusinessMeshes;
};

UCLASS(BlueprintType)
class CITYSIMVR_API UCityBuildingCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buildings")
	TArray<FCityBuildingDefinition> Definitions;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TArray<FCityCommercialPresentationVariant> CommercialPresentationVariants;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TArray<FCityLowDensityPresentationVariant> LowDensityPresentationVariants;

	const FCityBuildingDefinition* FindDefinition(FName DefinitionId) const;
	TSoftObjectPtr<UStaticMesh> FindCommercialPresentation(uint64 EntityId, ECityDensityLevel Density, bool bDamaged,
	                                                       FName& OutBusinessId) const;
	TSoftObjectPtr<UStaticMesh> FindLowDensityPresentation(uint64 EntityId, FName DefinitionId,
	                                                      ECityDensityLevel Density, bool bDamaged,
	                                                      FName& OutAppearanceId) const;
};

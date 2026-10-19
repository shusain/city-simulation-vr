#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityGrid.h"
#include "CityDiagnosticOverlay.generated.h"

class FCityPlacementModel;
struct FCityBuildingDefinition;
struct FCityEconomyTuning;
struct FCityEnvironmentSnapshot;
struct FCityNeedsSnapshot;
struct FCityServiceSnapshot;
struct FCityServiceReach;
struct FCityTrafficSnapshot;
struct FCityDevelopmentSnapshot;
struct FCityWasteSnapshot;
struct FCityHealthSnapshot;
struct FCityPopulationSnapshot;
struct FCityRoadNetworkSnapshot;
enum class ECityOperationFailure : uint8;

UENUM(BlueprintType)
enum class ECityDiagnosticOverlay : uint8
{
	None,
	Service,
	Power,
	ResidentialDemand,
	CommercialDemand,
	IndustrialDemand,
	Pollution,
	Traffic,
	Crime,
	Fire,
	Education,
	Parks,
	Health,
	Waste,
	Happiness,
    CommercialAi
};

struct CITYSIMVR_API FCityDiagnosticOverlayResult
{
	FCityGridDimensions Dimensions;
	ECityDiagnosticOverlay Overlay = ECityDiagnosticOverlay::None;
	TArray<FCityCellCoord> RoadCells;
	TArray<FCityCellCoord> PositiveCells;
	TArray<FCityCellCoord> WarningCells;
	FString Description;
	TArray<int32> Values; // Exact unsmoothed source values, indexed by cell.
	TArray<uint8> Status; // 0: no data, 1: covered/eligible, 2: blocked, 3: building upgrade pressure.
	int32 DisplayMaximum = 100;
	FString Legend;
	// Exact evaluator-backed descriptions, including independent raised pipes.
	TMap<FCityCellCoord, FString> CellDetails;
	FString DescribeCell(const FCityCellCoord& Cell) const;
	FLinearColor ColorAt(int32 Index) const;
};

class CITYSIMVR_API FCityDiagnosticOverlayEvaluator
{
public:
	static void BuildEducationReach(const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
	                                const FCityServiceReach& Reach, const FString& Label, int32 Range,
	                                FCityDiagnosticOverlayResult& OutResult);
	static void Build(const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
	                  const FCityNeedsSnapshot& Needs, const FCityEconomyTuning& Tuning,
	                  const FCityBuildingDefinition* HomeDefinition, const FCityBuildingDefinition* ShopDefinition,
	                  const FCityBuildingDefinition* IndustryDefinition, const FCityEnvironmentSnapshot& Environment,
	                  ECityDiagnosticOverlay Overlay, uint64 FocusEntityId, FCityDiagnosticOverlayResult& OutResult,
	                  const FCityTrafficSnapshot* Traffic = nullptr, const FCityRoadNetworkSnapshot* Network = nullptr,
	                  const FCityDevelopmentSnapshot* Development = nullptr,
	                  const FCityHealthSnapshot* Health = nullptr, const FCityPopulationSnapshot* People = nullptr,
	                  const FCityWasteSnapshot* Waste = nullptr);
	static ECityDiagnosticOverlay OverlayForOperationFailure(ECityOperationFailure OperationFailure);
	static FString DisplayName(ECityDiagnosticOverlay Overlay);
};

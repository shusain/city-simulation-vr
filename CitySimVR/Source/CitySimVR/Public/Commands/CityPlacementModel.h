#pragma once

#include "CoreMinimal.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CitySpatialCache.h"

class FCityTerrainHeightfield;
struct FCityServiceSnapshot;

enum class ECityConstructionFunding : uint8
{
	CityTreasury,
	PrivateDeveloper
};

enum class ECityPlacementFailure : uint8
{
	None,
	NoActivePreview,
	InvalidDefinition,
	StalePreview,
	OutOfBounds,
	Occupied,
	WaterOrShore,
	FoundationRelief,
	InsufficientFunds,
	TechnologyPrerequisite,
	FullRoadFrontage
};

enum class ECityRoadFailure : uint8
{
	None,
	NoActivePreview,
	StalePreview,
	OutOfBounds,
	Blocked,
	UnsupportedGrade,
	InsufficientFunds,
	InvalidStructure
};

enum class ECityRoadProfile : uint8
{
	LegacyAuto,
	Ground,
	Bridge,
	Elevated
};

struct CITYSIMVR_API FCityRoadStructure
{
	FName DefinitionId = TEXT("LocalRoad");
	ECityRoadProfile Profile = ECityRoadProfile::Elevated;
	FCityCellCoord Start, End;
	int32 DeckHeightCm = 0, StartHeightCm = 0, EndHeightCm = 0;
	int32 StartRampCells = 0, EndRampCells = 0;
	int32 ElevationStep = 0;
	int64 RecordedCost = 0;
	bool operator==(const FCityRoadStructure& Other) const = default;
};

enum class ECityRoadTerrainClass : uint8
{
	Ordinary,
	Supported,
	Bridge,
	Invalid
};

struct CITYSIMVR_API FCityRoadTerrainEvaluation
{
	ECityRoadTerrainClass TerrainClass = ECityRoadTerrainClass::Ordinary;
	int64 Cost = 0;
};

enum class ECityDemolitionFailure : uint8
{
	None,
	EntityNotFound
};

enum class ECityZoneType : uint8
{
	None,
	Residential,
	Commercial,
	Industrial
};

struct CITYSIMVR_API FCityPlacementPreview
{
	FName DefinitionId;
	FCityCellCoord Anchor;
	int32 QuarterTurnsClockwise = 0;
	uint64 ExpectedRevision = 0;
	bool bActive = false;
	bool bRotationOverridden = false;
};

struct CITYSIMVR_API FCityPlacedEntity
{
	uint64 EntityId = FCityGrid::EmptyEntityId;
	FName DefinitionId;
	FCityCellCoord Anchor;
	int32 QuarterTurnsClockwise = 0;
	FIntPoint FootprintSize = FIntPoint::ZeroValue;
	// Actual city spending eligible for demolition refunds; private development records zero.
	// Retained verbatim in older saves: no retroactive reimbursement or reclassification.
	int64 RecordedPlacementCost = 0;
};

struct CITYSIMVR_API FCityPlacementResult
{
	ECityPlacementFailure Failure = ECityPlacementFailure::None;
	uint64 EntityId = FCityGrid::EmptyEntityId;
	TArray<FCityCellCoord> ChangedCells;

	bool IsSuccess() const
	{
		return Failure == ECityPlacementFailure::None;
	}
};

struct CITYSIMVR_API FCityRoadPreview
{
	FCityCellCoord Start;
	FCityCellCoord End;
	FName DefinitionId = TEXT("LocalRoad");
	ECityRoadProfile Profile = ECityRoadProfile::LegacyAuto;
	int32 ElevationStep = 0;
	int64 PlacementCostPerCell = 10;
	uint64 ExpectedRevision = 0;
	bool bActive = false;
};

struct CITYSIMVR_API FCityRoadResult
{
	ECityRoadFailure Failure = ECityRoadFailure::None;
	TArray<FCityCellCoord> PathCells;
	TArray<FCityCellCoord> ChangedCells;
	TArray<FCityCellCoord> UpgradedCells;
	TArray<FCityCellCoord> SupportedCells;
	TArray<FCityCellCoord> BridgeCells;
	FCityRoadStructure Structure;
	FString Detail;
	int64 Cost = 0;

	bool IsSuccess() const
	{
		return Failure == ECityRoadFailure::None;
	}
};

struct CITYSIMVR_API FCityRoadCellRecord
{
	FCityCellCoord Cell;
	FName DefinitionId = TEXT("LocalRoad");
};

struct CITYSIMVR_API FCityDemolitionResult
{
	ECityDemolitionFailure Failure = ECityDemolitionFailure::None;
	uint64 EntityId = FCityGrid::EmptyEntityId;
	TArray<FCityCellCoord> ChangedCells;
	int64 Refund = 0;

	bool IsSuccess() const
	{
		return Failure == ECityDemolitionFailure::None;
	}
};

struct CITYSIMVR_API FCityPlacementModelState
{
	FCityGridDimensions Dimensions;
	int64 Funds = 0;
	uint64 Revision = 0;
	uint64 NextEntityId = 1;
	TArray<FCityPlacedEntity> Entities;
	TArray<FCityCellCoord> RoadCells;
	TArray<FCityRoadCellRecord> RoadRecords;
	TArray<FCityRoadStructure> RoadStructures;
	TArray<ECityZoneType> Zones;
};

class CITYSIMVR_API FCityPlacementModel
{
public:
	static constexpr uint64 RoadEntityId = MAX_uint64;
	static constexpr int64 RoadCellCost = 10;
	static constexpr int64 SupportedGradeSurcharge = 20;
	static constexpr int64 BridgeSurcharge = 60;
	static constexpr int32 MaximumSupportedRoadReliefCm = 600;
	static constexpr int32 MaximumFoundationReliefCm = 320;
	static constexpr int32 DemolitionRefundPercent = 50;
	static const FCityCellCoord EntranceCell;

	explicit FCityPlacementModel(int64 StartingFunds = 10000, FCityGridDimensions Dimensions = FCityGridDimensions());
	bool SetTerrain(const FCityTerrainHeightfield* InTerrain, FString* OutError = nullptr);
	const FCityTerrainHeightfield* GetTerrain() const
	{
		return Terrain;
	}
	FCityCellCoord GetEntranceCell() const;
	FCityRoadTerrainEvaluation EvaluateRoadTerrain(const FCityCellCoord& Cell) const;

	FCityPlacementPreview BeginPreview(const FCityBuildingDefinition& Definition, const FCityCellCoord& Anchor,
	                                   int32 QuarterTurnsClockwise = 0) const;
	FCityPlacementPreview BeginRoadFacingPreview(const FCityBuildingDefinition& Definition,
	                                             const FCityCellCoord& Anchor) const;
	void UpdatePreviewAnchor(FCityPlacementPreview& Preview, const FCityCellCoord& Anchor) const;
	void UpdateRoadFacingPreviewAnchor(FCityPlacementPreview& Preview, const FCityBuildingDefinition& Definition,
	                                   const FCityCellCoord& Anchor) const;
	void RotatePreviewClockwise(FCityPlacementPreview& Preview) const;
	void CancelPreview(FCityPlacementPreview& Preview) const;

	FCityPlacementResult ValidatePlacement(
	    const FCityPlacementPreview& Preview, const FCityBuildingDefinition& Definition,
	    ECityConstructionFunding Funding = ECityConstructionFunding::CityTreasury,
	    const FCityServiceSnapshot* Services = nullptr) const;
	FCityPlacementResult CommitPlacement(
	    FCityPlacementPreview& Preview, const FCityBuildingDefinition& Definition,
	    ECityConstructionFunding Funding = ECityConstructionFunding::CityTreasury,
	    const FCityServiceSnapshot* Services = nullptr);

	FCityRoadPreview BeginRoadPreview(const FCityCellCoord& Start, FName DefinitionId = TEXT("LocalRoad"),
	                                  int64 PlacementCostPerCell = RoadCellCost) const;
	void UpdateRoadPreviewEnd(FCityRoadPreview& Preview, const FCityCellCoord& End) const;
	ECityRoadProfile ResolveRoadDrawingProfile(const FCityCellCoord& Start, const FCityCellCoord& End,
	                                           bool bRaised) const;
	void CancelRoadPreview(FCityRoadPreview& Preview) const;
	FCityRoadResult ValidateRoad(const FCityRoadPreview& Preview) const;
	FCityRoadResult CommitRoad(FCityRoadPreview& Preview);
	bool RemoveRoad(const FCityCellCoord& Cell);
	const FCityRoadStructure* FindRoadStructure(const FCityCellCoord& Cell, bool bIncludeApproaches = true) const;
	bool CanRoadsConnect(const FCityCellCoord& A, const FCityCellCoord& B) const;
	bool HasRoadSupport(const FCityCellCoord& Cell) const;
	const FCityRoadStructure* FindRoadCorridor(const FCityCellCoord& Cell) const;
	void GetConnectedRoadNeighbors(const FCityCellCoord& Cell, TArray<FCityCellCoord>& Out) const;
	FCityCellCoord GetRoadNeighbor(const FCityCellCoord& Cell, int32 DX, int32 DY) const;
	FName ResolveRaisedRoadClass(const FCityCellCoord& Cell) const;
	void RebuildRaisedRoadClasses();
	bool CanRaisedSpansIntersect(const FCityRoadStructure& A, const FCityRoadStructure& B, const FCityCellCoord& Cell,
	                             FString& Error) const;
	bool CanGroundCrossSpan(const FCityRoadStructure& Span, const FCityCellCoord& GroundCell, FString& Error) const;
	bool IsReservedRoadCorridor(const FCityCellCoord& Cell) const
	{
		return FindRoadCorridor(Cell) != nullptr;
	}
	static constexpr int32 RoadElevationStepCm = 200;
	static constexpr int32 MaximumRoadElevationStep = 4;
	bool MakeRoadStructure(const FCityRoadPreview& Preview, FCityRoadStructure& Out, FString& Error,
	                       bool bSnapDeck = true) const;
	bool ValidateRoadStructures(FString& Error) const;
	double SampleStructureHeight(const FCityRoadStructure& Span, double X, double Y) const;
	bool TrySampleStructureHeight(double X, double Y, double& OutHeight) const;
	const TArray<FCityRoadStructure>& GetRoadStructures() const
	{
		return RoadStructures;
	}
	bool UpgradeRoadCells(const TArray<FCityCellCoord>& Cells, FName DefinitionId, int64 CostPerCell,
	                      FString& OutError);

	FCityDemolitionResult DemolishEntity(uint64 EntityId);
	FCityDemolitionResult DemolishAtCell(const FCityCellCoord& Cell);

	bool IsRoad(const FCityCellCoord& Cell) const
	{
		return RoadCells.Contains(Cell);
	}
	bool IsRoadConnected(const FCityCellCoord& Cell) const
	{
		return ConnectedRoadCells.Contains(Cell);
	}
	bool SupportsBuildingFrontage(const FCityCellCoord& Cell) const;
    static bool RequiresFullRoadFrontage(FName DefinitionId)
    {
        // Wide service entrances need a road along their full facade; never
        // funnel separate personnel/vehicle approaches into a single tile.
        return DefinitionId==TEXT("Police") || DefinitionId==TEXT("Fire") ||
            DefinitionId==TEXT("Hospital") || DefinitionId==TEXT("WasteDepot") || DefinitionId==TEXT("Recycling") ||
            DefinitionId==TEXT("School") || DefinitionId==TEXT("PrimarySchool") || DefinitionId==TEXT("SecondarySchool") || DefinitionId==TEXT("College") ||
            DefinitionId==TEXT("CoalPower") || DefinitionId==TEXT("OilPower") ||
            DefinitionId==TEXT("Power") || DefinitionId==TEXT("Service") || DefinitionId==TEXT("NuclearPower");
    }
    bool HasFullRoadFrontage(const FCityCellCoord& Anchor,const FIntPoint& FootprintSize,
        int32 QuarterTurnsClockwise,bool bConnected=false) const;
	bool HasRoadAccess(uint64 EntityId) const;
	bool HasConnectedRoadAccess(const FCityCellCoord& Anchor, const FIntPoint& FootprintSize) const;
	int32 GetDefaultRoadFacingQuarterTurns(const FCityBuildingDefinition& Definition,
	                                       const FCityCellCoord& Anchor) const;
	uint8 GetRoadNeighborMask(const FCityCellCoord& Cell) const;
	const TSet<FCityCellCoord>& GetRoadCells() const
	{
		return RoadCells;
	}
	FName GetRoadDefinitionId(const FCityCellCoord& Cell) const;
	bool SetZone(const FCityCellCoord& Cell, ECityZoneType Zone);
	bool CanSetZone(const FCityCellCoord& Cell) const;
	bool CanSetZoneArea(const FCityCellCoord& Anchor, const FIntPoint& Size) const;
	bool SetZoneArea(const FCityCellCoord& Anchor, const FIntPoint& Size, ECityZoneType Zone);
	ECityZoneType GetZone(const FCityCellCoord& Cell) const;
	bool HasZone(ECityZoneType Zone) const
	{
		return Zones.Contains(Zone);
	}

	int64 GetFunds() const
	{
		return Funds;
	}
	bool TrySpendFunds(int64 Amount);
	int64 ApplyMonthlyEconomy(int64 Income, int64 Upkeep);
	uint64 GetRevision() const
	{
		return Revision;
	}
	const FCityGrid& GetGrid() const
	{
		return Grid;
	}
	const TMap<uint64, FCityPlacedEntity>& GetEntities() const
	{
		return Entities;
	}
	uint64 GetNextEntityId() const
	{
		return NextEntityId;
	}
	FCityPlacementModelState ExportState() const;
	static TUniquePtr<FCityPlacementModel> CreateValidatedFromState(
	    const FCityPlacementModelState& State, const TMap<FName, FCityBuildingDefinition>& Definitions,
	    FString& OutError);

	static FString FailureToString(ECityPlacementFailure Failure);
	static FString FailureToString(ECityRoadFailure Failure);
	static void BuildOrthogonalRoadPath(const FCityCellCoord& Start, const FCityCellCoord& End,
	                                    TArray<FCityCellCoord>& OutPath);

	void GetAdjacentRoadCells(const FCityCellCoord& Anchor, const FIntPoint& Size, bool bConnectedOnly,
	                          TArray<FCityCellCoord>& Out, bool bBuildingFrontageOnly = false) const;
	void GetRoadReach(const TArray<FCityCellCoord>& Starts, int32 HopBudget, bool bConnectedOnly,
	                  TMap<FCityCellCoord, int32>& Out) const;
	const FCitySpatialCacheStats& GetSpatialCacheStats() const
	{
		return SpatialCache.Stats;
	}
	// Reference path for correctness checks and controlled performance comparisons.
	void SetSpatialCacheEnabled(bool bEnabled)
	{
		SpatialCache.Invalidate();
		SpatialCache.bEnabled = bEnabled;
	}

private:
	static void AppendFootprintCells(TArray<FCityCellCoord>& OutCells, const FCityCellCoord& Anchor,
	                                 const FIntPoint& Size);
	void RebuildRoadConnectivity();

	FCityGrid Grid;
	int64 Funds = 0;
	uint64 Revision = 0;
	uint64 NextEntityId = 1;
	TMap<uint64, FCityPlacedEntity> Entities;
	TSet<FCityCellCoord> RoadCells;
	TMap<FCityCellCoord, FName> RoadDefinitionIds;
	TSet<FCityCellCoord> ConnectedRoadCells;
	TArray<FCityRoadStructure> RoadStructures;
	TArray<ECityZoneType> Zones;
	const FCityTerrainHeightfield* Terrain = nullptr;
	mutable FCitySpatialCache SpatialCache;
};

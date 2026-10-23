#include "Commands/CityPlacementModel.h"
#include "Simulation/CityGrowthSimulation.h"

#include "Containers/Queue.h"
#include "Simulation/CityRoadNetwork.h"
#include "Simulation/CityTerrain.h"

const FCityCellCoord FCityPlacementModel::EntranceCell(0, 8);

FCityCellCoord FCityPlacementModel::GetEntranceCell() const
{
	return Terrain ? Terrain->GetParameters().GetEntranceCell() : EntranceCell;
}

bool FCityPlacementModel::SetTerrain(const FCityTerrainHeightfield* InTerrain, FString* OutError)
{
	if (InTerrain && InTerrain->GetDimensions() != Grid.GetDimensions())
	{
		if (OutError)
			*OutError = TEXT("Terrain dimensions differ from city occupancy");
		return false;
	}
	Terrain = InTerrain;
	RebuildRoadConnectivity();
	return true;
}

namespace
{
	bool IsKnownRoadDefinition(const FName Id)
	{
		return Id == TEXT("DirtRoad") || Id == TEXT("LocalRoad") || Id == TEXT("Avenue") || Id == TEXT("Highway");
	}

	int64 RoadPlacementCost(const FName Id)
	{
		if (Id == TEXT("DirtRoad"))
			return FCityRoadDefinition::Dirt().PlacementCostPerCell;
		if (Id == TEXT("Avenue"))
			return FCityRoadDefinition::Avenue().PlacementCostPerCell;
		if (Id == TEXT("Highway"))
			return FCityRoadDefinition::Highway().PlacementCostPerCell;
		return FCityRoadDefinition::Local().PlacementCostPerCell;
	}

	ECityRoadAccess RoadAccess(const FName Id)
	{
		if (Id == TEXT("DirtRoad"))
			return FCityRoadDefinition::Dirt().Access;
		if (Id == TEXT("Avenue"))
			return FCityRoadDefinition::Avenue().Access;
		if (Id == TEXT("Highway"))
			return FCityRoadDefinition::Highway().Access;
		return FCityRoadDefinition::Local().Access;
	}
} // namespace

FCityPlacementModel::FCityPlacementModel(const int64 StartingFunds, const FCityGridDimensions Dimensions)
    : Grid(Dimensions), Funds(FMath::Max<int64>(0, StartingFunds))
{
	Zones.Init(ECityZoneType::None, Grid.GetDimensions().GetCellCount());
}

bool FCityPlacementModel::TrySpendFunds(const int64 Amount)
{
	if (Amount < 0 || Funds < Amount)
	{
		return false;
	}
	Funds -= Amount;
	++Revision;
	return true;
}

int64 FCityPlacementModel::ApplyMonthlyEconomy(const int64 Income, const int64 Upkeep)
{
	const int64 SafeIncome = FMath::Max<int64>(0, Income);
	const int64 SafeUpkeep = FMath::Max<int64>(0, Upkeep);
	Funds = SafeIncome > MAX_int64 - Funds ? MAX_int64 : Funds + SafeIncome;
	const int64 Paid = FMath::Min(Funds, SafeUpkeep);
	Funds -= Paid;
	return SafeUpkeep - Paid;
}

FCityPlacementPreview FCityPlacementModel::BeginPreview(const FCityBuildingDefinition& Definition,
                                                        const FCityCellCoord& Anchor,
                                                        const int32 QuarterTurnsClockwise) const
{
	FCityPlacementPreview Preview;
	Preview.DefinitionId = Definition.DefinitionId;
	Preview.Anchor = Anchor;
	Preview.QuarterTurnsClockwise = ((QuarterTurnsClockwise % 4) + 4) % 4;
	Preview.ExpectedRevision = Revision;
	Preview.bActive = true;
	return Preview;
}

FCityPlacementPreview FCityPlacementModel::BeginRoadFacingPreview(const FCityBuildingDefinition& Definition,
                                                                  const FCityCellCoord& Anchor) const
{
	return BeginPreview(Definition, Anchor, GetDefaultRoadFacingQuarterTurns(Definition, Anchor));
}

void FCityPlacementModel::UpdatePreviewAnchor(FCityPlacementPreview& Preview, const FCityCellCoord& Anchor) const
{
	if (Preview.bActive)
	{
		Preview.Anchor = Anchor;
	}
}

void FCityPlacementModel::UpdateRoadFacingPreviewAnchor(FCityPlacementPreview& Preview,
                                                        const FCityBuildingDefinition& Definition,
                                                        const FCityCellCoord& Anchor) const
{
	UpdatePreviewAnchor(Preview, Anchor);
    // Hover is a new review of the current city, not an old unreviewed command.
    if (Preview.bActive)
        Preview.ExpectedRevision = Revision;
	if (Preview.bActive && !Preview.bRotationOverridden)
	{
		Preview.QuarterTurnsClockwise = GetDefaultRoadFacingQuarterTurns(Definition, Anchor);
	}
}

void FCityPlacementModel::RotatePreviewClockwise(FCityPlacementPreview& Preview) const
{
	if (Preview.bActive)
	{
		Preview.QuarterTurnsClockwise = (Preview.QuarterTurnsClockwise + 1) % 4;
		Preview.bRotationOverridden = true;
	}
}

void FCityPlacementModel::CancelPreview(FCityPlacementPreview& Preview) const
{
	Preview.bActive = false;
}

FCityPlacementResult FCityPlacementModel::ValidatePlacement(const FCityPlacementPreview& Preview,
                                                            const FCityBuildingDefinition& Definition,
                                                            const ECityConstructionFunding Funding,
                                                            const FCityServiceSnapshot* Services) const
{
	FCityPlacementResult Result;
	if (!Preview.bActive)
	{
		Result.Failure = ECityPlacementFailure::NoActivePreview;
	}
	else if (!Definition.IsValid() || Preview.DefinitionId != Definition.DefinitionId)
	{
		Result.Failure = ECityPlacementFailure::InvalidDefinition;
	}
	else if (Funding == ECityConstructionFunding::PrivateDeveloper &&
	         (Definition.bPlayerPurchasedLevels ||
	          (Definition.DevelopmentDemandType == ECityDevelopmentDemandType::None &&
	           Definition.DefinitionId != TEXT("Home") && Definition.DefinitionId != TEXT("Shop") &&
	           Definition.DefinitionId != TEXT("Factory"))))
	{
		Result.Failure = ECityPlacementFailure::InvalidDefinition;
	}
	else if (!Definition.PrerequisitesMet(Services))
	{
		Result.Failure = ECityPlacementFailure::TechnologyPrerequisite;
	}
	else if ((!Definition.RequiredOperatingFacilities.IsEmpty() || Definition.bRequiresAlternativePower) &&
	         Services && Services->PlacementRevision != Revision)
	{
		Result.Failure = ECityPlacementFailure::StalePreview;
	}
	else if (Preview.ExpectedRevision != Revision)
	{
		Result.Failure = ECityPlacementFailure::StalePreview;
	}
	else if (Preview.Anchor.RoadLevel != 0 ||
	         !Grid.IsFootprintInBounds(Preview.Anchor, Definition.Footprint, Preview.QuarterTurnsClockwise))
	{
		Result.Failure = ECityPlacementFailure::OutOfBounds;
	}
	else if (!Grid.CanOccupy(Preview.Anchor, Definition.Footprint, Preview.QuarterTurnsClockwise) ||
	         !Grid.CanOccupy(FCityCellCoord(Preview.Anchor.X, Preview.Anchor.Y, 1), Definition.Footprint,
	                         Preview.QuarterTurnsClockwise))
	{
		Result.Failure = ECityPlacementFailure::Occupied;
	}
	else if (Terrain)
	{
		const FIntPoint Size = Definition.Footprint.GetRotatedSize(Preview.QuarterTurnsClockwise);
		int32 Minimum = MAX_int32;
		int32 Maximum = MIN_int32;
		for (int32 Y = Preview.Anchor.Y; Y < Preview.Anchor.Y + Size.Y; ++Y)
		{
			for (int32 X = Preview.Anchor.X; X < Preview.Anchor.X + Size.X; ++X)
			{
				const FCityCellCoord Cell(X, Y);
				if (Terrain->GetWaterClass(Cell) != ECityWaterClass::Land)
				{
					Result.Failure = ECityPlacementFailure::WaterOrShore;
					return Result;
				}
				Minimum = FMath::Min(Minimum, Terrain->GetCellMinHeightCm(Cell));
				Maximum = FMath::Max(Maximum, Terrain->GetCellMaxHeightCm(Cell));
			}
		}
		if (Maximum - Minimum > MaximumFoundationReliefCm)
		{
			Result.Failure = ECityPlacementFailure::FoundationRelief;
		}
	}
    if(Result.IsSuccess() && RequiresFullRoadFrontage(Definition.DefinitionId) &&
        !HasFullRoadFrontage(Preview.Anchor,Definition.Footprint.GetRotatedSize(Preview.QuarterTurnsClockwise),Preview.QuarterTurnsClockwise))
        Result.Failure=ECityPlacementFailure::FullRoadFrontage;
	if (Result.IsSuccess() && Funding == ECityConstructionFunding::CityTreasury && Funds < Definition.Cost)
	{
		Result.Failure = ECityPlacementFailure::InsufficientFunds;
	}
	return Result;
}

FCityPlacementResult FCityPlacementModel::CommitPlacement(FCityPlacementPreview& Preview,
                                                          const FCityBuildingDefinition& Definition,
                                                          const ECityConstructionFunding Funding,
                                                          const FCityServiceSnapshot* Services)
{
	FCityPlacementResult Result = ValidatePlacement(Preview, Definition, Funding, Services);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	const uint64 EntityId = NextEntityId;
	if (!Grid.TryOccupy(Preview.Anchor, Definition.Footprint, EntityId, Preview.QuarterTurnsClockwise))
	{
		Result.Failure = ECityPlacementFailure::Occupied;
		return Result;
	}

	FCityPlacedEntity Entity;
	Entity.EntityId = EntityId;
	Entity.DefinitionId = Definition.DefinitionId;
	Entity.Anchor = Preview.Anchor;
	Entity.QuarterTurnsClockwise = Preview.QuarterTurnsClockwise;
	Entity.FootprintSize = Definition.Footprint.GetRotatedSize(Entity.QuarterTurnsClockwise);
	Entity.RecordedPlacementCost = Funding == ECityConstructionFunding::CityTreasury ? Definition.Cost : 0;
	Entities.Add(EntityId, Entity);
	Funds -= Entity.RecordedPlacementCost;
	++NextEntityId;
	++Revision;
	SpatialCache.Invalidate();
	Preview.bActive = false;

	Result.EntityId = EntityId;
	AppendFootprintCells(Result.ChangedCells, Entity.Anchor, Entity.FootprintSize);
	return Result;
}

FCityRoadPreview FCityPlacementModel::BeginRoadPreview(const FCityCellCoord& Start, const FName DefinitionId,
                                                       const int64 PlacementCostPerCell) const
{
	FCityRoadPreview Preview;
	Preview.Start = Start;
	Preview.End = Start;
	Preview.DefinitionId = DefinitionId;
	Preview.PlacementCostPerCell = PlacementCostPerCell;
	Preview.ExpectedRevision = Revision;
	Preview.bActive = true;
	return Preview;
}

void FCityPlacementModel::UpdateRoadPreviewEnd(FCityRoadPreview& Preview, const FCityCellCoord& End) const
{
	if (Preview.bActive)
	{
		Preview.End = End;
	}
}

void FCityPlacementModel::CancelRoadPreview(FCityRoadPreview& Preview) const
{
	Preview.bActive = false;
}

FCityRoadResult FCityPlacementModel::ValidateRoad(const FCityRoadPreview& Preview) const
{
	FCityRoadResult Result;
	if (!Preview.bActive)
	{
		Result.Failure = ECityRoadFailure::NoActivePreview;
		return Result;
	}
	if (Preview.ExpectedRevision != Revision)
	{
		Result.Failure = ECityRoadFailure::StalePreview;
		return Result;
	}
	if (!IsKnownRoadDefinition(Preview.DefinitionId) || Preview.PlacementCostPerCell < 0)
	{
		Result.Failure = ECityRoadFailure::Blocked;
		return Result;
	}

	BuildOrthogonalRoadPath(Preview.Start, Preview.End, Result.PathCells);
	const bool bStructure =
	    Preview.Profile == ECityRoadProfile::Bridge || Preview.Profile == ECityRoadProfile::Elevated;
	if (static_cast<uint8>(Preview.Profile) > 3 ||
	    (bStructure && !MakeRoadStructure(Preview, Result.Structure, Result.Detail)))
	{
		Result.Failure = ECityRoadFailure::InvalidStructure;
		return Result;
	}
	if ((!bStructure && (Preview.Start.RoadLevel != 0 || Preview.End.RoadLevel != 0)) || Preview.Start.RoadLevel > 1 ||
	    Preview.End.RoadLevel > 1)
	{
		Result.Failure = ECityRoadFailure::OutOfBounds;
		return Result;
	}
	if (bStructure)
	{
		Result.Cost = Result.Structure.RecordedCost;
		for (int32 I = 1; I < Result.PathCells.Num() - 1; ++I)
			Result.PathCells[I].RoadLevel = 1;
	}

	for (const FCityCellCoord& Cell : Result.PathCells)
	{
		if (!Grid.IsValidCell(Cell))
		{
			Result.Failure = ECityRoadFailure::OutOfBounds;
			return Result;
		}

		const uint64 Occupant = Grid.GetOccupant(Cell);
		const bool bEndpoint = Cell.RoadLevel == 0 && (Cell == Preview.Start || Cell == Preview.End);
		if (bStructure)
		{
			if ((Cell == Preview.Start || Cell == Preview.End) && Cell.RoadLevel == 1 &&
			    !FindRoadStructure(Cell, false))
			{
				Result.Detail = TEXT("Raised branch endpoints must meet an existing flat deck");
				Result.Failure = ECityRoadFailure::InvalidStructure;
				return Result;
			}
			for (const auto& Existing : RoadStructures)
				if (Cell.X >= FMath::Min(Existing.Start.X, Existing.End.X) &&
				    Cell.X <= FMath::Max(Existing.Start.X, Existing.End.X) &&
				    Cell.Y >= FMath::Min(Existing.Start.Y, Existing.End.Y) &&
				    Cell.Y <= FMath::Max(Existing.Start.Y, Existing.End.Y))
				{
					if (!CanRaisedSpansIntersect(Existing, Result.Structure, Cell, Result.Detail))
					{
						Result.Failure = ECityRoadFailure::InvalidStructure;
						return Result;
					}
				}
			if (!bEndpoint)
			{
				const FCityCellCoord Ground(Cell.X, Cell.Y);
				const uint64 Below = Grid.GetOccupant(Ground);
				if (Below != FCityGrid::EmptyEntityId &&
				    (Below != RoadEntityId || !CanGroundCrossSpan(Result.Structure, Ground, Result.Detail) ||
				     GetRoadNeighborMask(Ground) != (Preview.Start.Y == Preview.End.Y ? 5 : 10)))
				{
					if (Result.Detail.IsEmpty())
						Result.Detail = TEXT("Span overlaps a building or incompatible ground road");
					Result.Failure = ECityRoadFailure::InvalidStructure;
					return Result;
				}
			}
		}
		else if (const auto* Span = FindRoadCorridor(Cell); Span && Cell != Span->Start && Cell != Span->End)
		{
			const bool AlongX = Span->Start.Y == Span->End.Y;
			const FCityCellCoord Before(Cell.X - (AlongX ? 0 : 1), Cell.Y - (AlongX ? 1 : 0));
			const FCityCellCoord After(Cell.X + (AlongX ? 0 : 1), Cell.Y + (AlongX ? 1 : 0));
			for (const auto& Other : RoadStructures)
				if (Cell.X >= FMath::Min(Other.Start.X, Other.End.X) &&
				    Cell.X <= FMath::Max(Other.Start.X, Other.End.X) &&
				    Cell.Y >= FMath::Min(Other.Start.Y, Other.End.Y) &&
				    Cell.Y <= FMath::Max(Other.Start.Y, Other.End.Y) && Cell != Other.Start && Cell != Other.End)
					if (!CanGroundCrossSpan(Other, Cell, Result.Detail))
					{
						Result.Failure = ECityRoadFailure::InvalidStructure;
						return Result;
					}
			if (!CanGroundCrossSpan(*Span, Cell, Result.Detail) || !Result.PathCells.Contains(Before) ||
			    !Result.PathCells.Contains(After))
			{
				if (Result.Detail.IsEmpty())
					Result.Detail = TEXT("Draw a straight perpendicular ground crossing beneath the deck");
				Result.Failure = ECityRoadFailure::InvalidStructure;
				return Result;
			}
		}
		if (Preview.Profile == ECityRoadProfile::Ground && Terrain && Terrain->IsWaterCell(Cell))
		{
			Result.Detail = TEXT("Ground roads need dry land; select Bridge to cross water");
			Result.Failure = ECityRoadFailure::InvalidStructure;
			return Result;
		}
		if (Occupant != FCityGrid::EmptyEntityId && Occupant != RoadEntityId)
		{
			Result.Failure = ECityRoadFailure::Blocked;
			return Result;
		}
		if (Occupant == FCityGrid::EmptyEntityId)
		{
			const FCityRoadTerrainEvaluation TerrainEvaluation = EvaluateRoadTerrain(Cell);
			if (!bStructure && TerrainEvaluation.TerrainClass == ECityRoadTerrainClass::Invalid)
			{
				Result.Failure = ECityRoadFailure::UnsupportedGrade;
				return Result;
			}
			Result.ChangedCells.Add(Cell);
			if (!bStructure || bEndpoint)
				Result.Cost += TerrainEvaluation.Cost - RoadCellCost + Preview.PlacementCostPerCell;
		}
		else if (!bStructure && GetRoadDefinitionId(Cell) != Preview.DefinitionId)
		{
			Result.UpgradedCells.Add(Cell);
			Result.Cost +=
			    FMath::Max<int64>(0, Preview.PlacementCostPerCell - RoadPlacementCost(GetRoadDefinitionId(Cell)));
		}
		const FCityRoadTerrainEvaluation TerrainEvaluation = EvaluateRoadTerrain(Cell);
		if (TerrainEvaluation.TerrainClass == ECityRoadTerrainClass::Supported)
		{
			Result.SupportedCells.Add(Cell);
		}
		else if (TerrainEvaluation.TerrainClass == ECityRoadTerrainClass::Bridge)
		{
			Result.BridgeCells.Add(Cell);
		}
	}

	if (Funds < Result.Cost)
	{
		Result.Failure = ECityRoadFailure::InsufficientFunds;
	}
	return Result;
}

FCityRoadResult FCityPlacementModel::CommitRoad(FCityRoadPreview& Preview)
{
	FCityRoadResult Result = ValidateRoad(Preview);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	TArray<FCityCellCoord> OccupiedThisCommit;
	OccupiedThisCommit.Reserve(Result.ChangedCells.Num());
	for (const FCityCellCoord& Cell : Result.ChangedCells)
	{
		if (!Grid.TryOccupy(Cell, FCityCellFootprint(1, 1), RoadEntityId))
		{
			for (const FCityCellCoord& RollbackCell : OccupiedThisCommit)
			{
				Grid.ClearCell(RollbackCell, RoadEntityId);
				RoadCells.Remove(RollbackCell);
				RoadDefinitionIds.Remove(RollbackCell);
			}
			Result.Failure = ECityRoadFailure::Blocked;
			Result.ChangedCells.Reset();
			Result.Cost = 0;
			return Result;
		}
		RoadCells.Add(Cell);
		RoadDefinitionIds.Add(Cell, Preview.DefinitionId);
		OccupiedThisCommit.Add(Cell);
	}
	for (const FCityCellCoord& Cell : Result.UpgradedCells)
	{
		RoadDefinitionIds.Add(Cell, Preview.DefinitionId);
	}

	if (Preview.Profile == ECityRoadProfile::Bridge || Preview.Profile == ECityRoadProfile::Elevated)
	{
		RoadStructures.Add(Result.Structure);
		RebuildRaisedRoadClasses();
	}
	Funds -= Result.Cost;
	Preview.bActive = false;
	if (!Result.ChangedCells.IsEmpty() || !Result.UpgradedCells.IsEmpty())
	{
		++Revision;
		RebuildRoadConnectivity();
	}
	return Result;
}

bool FCityPlacementModel::RemoveRoad(const FCityCellCoord& Cell)
{

	if (const auto* Structure = FindRoadStructure(Cell))
	{
		const FCityRoadStructure Span = *Structure;
		TArray<FCityCellCoord> Path;
		BuildOrthogonalRoadPath(Span.Start, Span.End, Path);
		// Remove only this span's ownership. Shared raised junctions survive with the other span.
		RoadStructures.Remove(Span);
		for (int32 I = 0; I < Path.Num(); ++I)
		{
			if (I > 0 && I < Path.Num() - 1)
				Path[I].RoadLevel = 1;
			if (Path[I].RoadLevel == 0)
				continue;
			if (FindRoadStructure(Path[I], false))
				continue;
			Grid.ClearCell(Path[I], RoadEntityId);
			RoadCells.Remove(Path[I]);
			RoadDefinitionIds.Remove(Path[I]);
		}
		RebuildRaisedRoadClasses();
		Funds += Span.RecordedCost * DemolitionRefundPercent / 100;
		++Revision;
		RebuildRoadConnectivity();
		return true;
	}
	if (!RoadCells.Contains(Cell) || !Grid.ClearCell(Cell, RoadEntityId))
	{
		return false;
	}

	RoadCells.Remove(Cell);
	RoadDefinitionIds.Remove(Cell);
	Funds += EvaluateRoadTerrain(Cell).Cost * DemolitionRefundPercent / 100;
	++Revision;
	RebuildRoadConnectivity();
	return true;
}

FName FCityPlacementModel::GetRoadDefinitionId(const FCityCellCoord& Cell) const
{
	const FName* DefinitionId = RoadDefinitionIds.Find(Cell);
	return DefinitionId ? *DefinitionId : NAME_None;
}

bool FCityPlacementModel::UpgradeRoadCells(const TArray<FCityCellCoord>& Cells, const FName DefinitionId,
                                           const int64 CostPerCell, FString& OutError)
{
	if (!IsKnownRoadDefinition(DefinitionId) || CostPerCell < 0 || Cells.IsEmpty())
	{
		OutError = TEXT("Road upgrade parameters are invalid");
		return false;
	}
	TSet<FCityCellCoord> UniqueCells;
	for (const FCityCellCoord& Cell : Cells)
	{
		if (!RoadCells.Contains(Cell) || FindRoadStructure(Cell, false))
		{
			OutError = TEXT("Road upgrade includes a non-road cell");
			return false;
		}
		if (GetRoadDefinitionId(Cell) != DefinitionId)
			UniqueCells.Add(Cell);
	}
	if (UniqueCells.IsEmpty())
		return true;
	const int64 TotalCost =
	    CostPerCell > 0 && UniqueCells.Num() > MAX_int64 / CostPerCell ? MAX_int64 : CostPerCell * UniqueCells.Num();
	if (Funds < TotalCost)
	{
		OutError = TEXT("Insufficient funds for road upgrade");
		return false;
	}
	for (const FCityCellCoord& Cell : UniqueCells)
	{
		RoadDefinitionIds.Add(Cell, DefinitionId);
	}
	Funds -= TotalCost;
	++Revision;
	SpatialCache.Invalidate();
	return true;
}

FCityDemolitionResult FCityPlacementModel::DemolishEntity(const uint64 EntityId)
{
	FCityDemolitionResult Result;
	const FCityPlacedEntity* Entity = Entities.Find(EntityId);
	if (!Entity)
	{
		Result.Failure = ECityDemolitionFailure::EntityNotFound;
		return Result;
	}

	Result.EntityId = EntityId;
	Result.Refund = Entity->RecordedPlacementCost * DemolitionRefundPercent / 100;
	AppendFootprintCells(Result.ChangedCells, Entity->Anchor, Entity->FootprintSize);

	Grid.ClearEntity(EntityId);
	Entities.Remove(EntityId);
	Funds += Result.Refund;
	++Revision;
	RebuildRoadConnectivity();
	return Result;
}

FCityDemolitionResult FCityPlacementModel::DemolishAtCell(const FCityCellCoord& Cell)
{
	const uint64 EntityId = Grid.GetOccupant(Cell);
	return EntityId != FCityGrid::EmptyEntityId && EntityId != RoadEntityId
	           ? DemolishEntity(EntityId)
	           : FCityDemolitionResult{ECityDemolitionFailure::EntityNotFound};
}

bool FCityPlacementModel::HasRoadAccess(const uint64 EntityId) const
{
	const auto* Entity = Entities.Find(EntityId);
	return Entity && (RequiresFullRoadFrontage(Entity->DefinitionId)
        ? HasFullRoadFrontage(Entity->Anchor,Entity->FootprintSize,Entity->QuarterTurnsClockwise,true)
        : HasConnectedRoadAccess(Entity->Anchor, Entity->FootprintSize));
}

bool FCityPlacementModel::HasFullRoadFrontage(const FCityCellCoord& Anchor,const FIntPoint& Size,
    const int32 QuarterTurns,const bool bConnected) const
{
    const int32 Side=QuarterTurns%4;
    const int32 Length=Side%2?Size.Y:Size.X;
    if(Length<=0) return false;
    for(int32 Along=0;Along<Length;++Along) {
        const FCityCellCoord Cell=Side==0?FCityCellCoord(Anchor.X+Along,Anchor.Y-1):
            Side==1?FCityCellCoord(Anchor.X+Size.X,Anchor.Y+Along):
            Side==2?FCityCellCoord(Anchor.X+Along,Anchor.Y+Size.Y):
                    FCityCellCoord(Anchor.X-1,Anchor.Y+Along);
        if(!SupportsBuildingFrontage(Cell) || (bConnected && !ConnectedRoadCells.Contains(Cell))) return false;
    }
    return true;
}

bool FCityPlacementModel::HasConnectedRoadAccess(const FCityCellCoord& Anchor, const FIntPoint& FootprintSize) const
{
	TArray<FCityCellCoord> Roads;
	GetAdjacentRoadCells(Anchor, FootprintSize, true, Roads, true);
	return !Roads.IsEmpty();
}

bool FCityPlacementModel::SupportsBuildingFrontage(const FCityCellCoord& Cell) const
{
	return RoadCells.Contains(Cell) && !FindRoadStructure(Cell, false) &&
	       EnumHasAnyFlags(RoadAccess(GetRoadDefinitionId(Cell)), ECityRoadAccess::BuildingFrontage);
}

int32 FCityPlacementModel::GetDefaultRoadFacingQuarterTurns(const FCityBuildingDefinition& Definition,
                                                            const FCityCellCoord& Anchor) const
{
	int32 BestQuarterTurns = 0;
	int32 BestConnectedRoadCount = 0;
	int32 BestRoadCount = -1;
	bool bBestClear = false;
	bool bBestFullFrontage = false;
	for (int32 QuarterTurns = 0; QuarterTurns < 4; ++QuarterTurns)
	{
		const FIntPoint Size = Definition.Footprint.GetRotatedSize(QuarterTurns);
        const bool bClear = Grid.CanOccupy(Anchor, Definition.Footprint, QuarterTurns) &&
            Grid.CanOccupy(FCityCellCoord(Anchor.X, Anchor.Y, 1), Definition.Footprint, QuarterTurns);
        const bool bFullFrontage = !RequiresFullRoadFrontage(Definition.DefinitionId) ||
            HasFullRoadFrontage(Anchor, Size, QuarterTurns);
		int32 ConnectedRoadCount = 0;
		int32 RoadCount = 0;
		auto CountRoad = [this, &ConnectedRoadCount, &RoadCount](const FCityCellCoord& Cell)
		{
			if (SupportsBuildingFrontage(Cell))
			{
				++RoadCount;
				if (ConnectedRoadCells.Contains(Cell))
				{
					++ConnectedRoadCount;
				}
			}
		};

		for (int32 LocalSide = 0; LocalSide < 4; ++LocalSide)
		{
			if ((Definition.EntranceSides & (1 << LocalSide)) == 0)
			{
				continue;
			}
			const int32 WorldSide = (LocalSide + QuarterTurns) % 4;
			if (WorldSide == 0 || WorldSide == 2)
			{
				const int32 RoadY = WorldSide == 0 ? Anchor.Y - 1 : Anchor.Y + Size.Y;
				for (int32 X = Anchor.X; X < Anchor.X + Size.X; ++X)
				{
					CountRoad(FCityCellCoord(X, RoadY));
				}
			}
			else
			{
				const int32 RoadX = WorldSide == 1 ? Anchor.X + Size.X : Anchor.X - 1;
				for (int32 Y = Anchor.Y; Y < Anchor.Y + Size.Y; ++Y)
				{
					CountRoad(FCityCellCoord(RoadX, Y));
				}
			}
		}

		if (BestRoadCount < 0 || (bClear && !bBestClear) ||
            (bClear == bBestClear && ((bFullFrontage && !bBestFullFrontage) ||
            (bFullFrontage == bBestFullFrontage && (ConnectedRoadCount > BestConnectedRoadCount ||
            (ConnectedRoadCount == BestConnectedRoadCount && RoadCount > BestRoadCount))))))
		{
			BestQuarterTurns = QuarterTurns;
            bBestClear = bClear;
            bBestFullFrontage = bFullFrontage;
			BestConnectedRoadCount = ConnectedRoadCount;
			BestRoadCount = RoadCount;
		}
	}
	return BestQuarterTurns;
}

bool FCityPlacementModel::SetZone(const FCityCellCoord& Cell, const ECityZoneType Zone)
{
	return SetZoneArea(Cell, FIntPoint(1, 1), Zone);
}

bool FCityPlacementModel::CanSetZone(const FCityCellCoord& Cell) const
{
	return Grid.IsValidCell(Cell) && Cell.RoadLevel == 0 && !FindRoadCorridor(Cell) &&
	       Grid.GetOccupant(Cell) == FCityGrid::EmptyEntityId && (!Terrain || Terrain->IsCellBuildable(Cell));
}

bool FCityPlacementModel::CanSetZoneArea(const FCityCellCoord& Anchor, const FIntPoint& Size) const
{
	if (Anchor.RoadLevel != 0)
		return false;
	if (Size.X <= 0 || Size.Y <= 0 || !Grid.IsFootprintInBounds(Anchor, FCityCellFootprint(Size.X, Size.Y)))
	{
		return false;
	}
	bool bHasEligibleCell = false;
	for (int32 Y = Anchor.Y; Y < Anchor.Y + Size.Y; ++Y)
	{
		for (int32 X = Anchor.X; X < Anchor.X + Size.X; ++X)
		{
			bHasEligibleCell |= CanSetZone(FCityCellCoord(X, Y));
		}
	}
	return bHasEligibleCell;
}

bool FCityPlacementModel::SetZoneArea(const FCityCellCoord& Anchor, const FIntPoint& Size, const ECityZoneType Zone)
{
	if (!CanSetZoneArea(Anchor, Size))
	{
		return false;
	}
	bool bChanged = false;
	for (int32 Y = Anchor.Y; Y < Anchor.Y + Size.Y; ++Y)
	{
		for (int32 X = Anchor.X; X < Anchor.X + Size.X; ++X)
		{
			const FCityCellCoord Cell(X, Y);
			bChanged |= CanSetZone(Cell) && Zones[Y * Grid.GetDimensions().GetWidth() + X] != Zone;
		}
	}
	if (!bChanged)
	{
		return true;
	}
	for (int32 Y = Anchor.Y; Y < Anchor.Y + Size.Y; ++Y)
	{
		for (int32 X = Anchor.X; X < Anchor.X + Size.X; ++X)
		{
			if (CanSetZone(FCityCellCoord(X, Y)))
			{
				Zones[Y * Grid.GetDimensions().GetWidth() + X] = Zone;
			}
		}
	}
	++Revision;
	return true;
}

ECityZoneType FCityPlacementModel::GetZone(const FCityCellCoord& Cell) const
{
	return Cell.RoadLevel == 0 && Grid.IsValidCell(Cell) ? Zones[Cell.Y * Grid.GetDimensions().GetWidth() + Cell.X]
	                                                     : ECityZoneType::None;
}

FCityPlacementModelState FCityPlacementModel::ExportState() const
{
	FCityPlacementModelState State;
	State.Dimensions = Grid.GetDimensions();
	State.Funds = Funds;
	State.Revision = Revision;
	State.NextEntityId = NextEntityId;
	Entities.GenerateValueArray(State.Entities);
	State.Entities.Sort([](const FCityPlacedEntity& Left, const FCityPlacedEntity& Right)
	                    { return Left.EntityId < Right.EntityId; });
	State.RoadCells = RoadCells.Array();
	State.RoadCells.Sort(
	    [](const FCityCellCoord& Left, const FCityCellCoord& Right)
	    {
		    return Left.Y == Right.Y ? (Left.X == Right.X ? Left.RoadLevel < Right.RoadLevel : Left.X < Right.X)
		                             : Left.Y < Right.Y;
	    });
	for (const FCityCellCoord& Cell : State.RoadCells)
	{
		State.RoadRecords.Add(
		    {Cell, GetRoadDefinitionId(Cell).IsNone() ? FName(TEXT("LocalRoad")) : GetRoadDefinitionId(Cell)});
	}
	State.RoadStructures = RoadStructures;
	State.Zones = Zones;
	return State;
}

TUniquePtr<FCityPlacementModel> FCityPlacementModel::CreateValidatedFromState(
    const FCityPlacementModelState& State, const TMap<FName, FCityBuildingDefinition>& Definitions, FString& OutError)
{
	if (!State.Dimensions.IsValid() || State.Funds < 0 || State.NextEntityId == FCityGrid::EmptyEntityId ||
	    State.NextEntityId == RoadEntityId || State.Zones.Num() != State.Dimensions.GetCellCount())
	{
		OutError = TEXT("Invalid balance, next entity ID, or zone dimensions");
		return nullptr;
	}
	for (const ECityZoneType Zone : State.Zones)
	{
		if (Zone != ECityZoneType::None && Zone != ECityZoneType::Residential && Zone != ECityZoneType::Commercial &&
		    Zone != ECityZoneType::Industrial)
		{
			OutError = TEXT("Save contains an unknown zone value");
			return nullptr;
		}
	}

	TUniquePtr<FCityPlacementModel> Model = MakeUnique<FCityPlacementModel>(State.Funds, State.Dimensions);
	TSet<FCityCellCoord> UniqueRoads;
	for (const FCityCellCoord& Cell : State.RoadCells)
	{
		if (!Model->Grid.IsValidCell(Cell) || UniqueRoads.Contains(Cell) ||
		    !Model->Grid.TryOccupy(Cell, FCityCellFootprint(1, 1), RoadEntityId))
		{
			OutError = TEXT("Save contains an invalid or duplicate road cell");
			return nullptr;
		}
		UniqueRoads.Add(Cell);
		Model->RoadCells.Add(Cell);
		Model->RoadDefinitionIds.Add(Cell, TEXT("LocalRoad"));
	}
	TSet<FCityCellCoord> DefinedRoadCells;
	for (const FCityRoadCellRecord& Record : State.RoadRecords)
	{
		if (!Model->RoadCells.Contains(Record.Cell) || DefinedRoadCells.Contains(Record.Cell) ||
		    !IsKnownRoadDefinition(Record.DefinitionId))
		{
			OutError = TEXT("Save contains an invalid road definition record");
			return nullptr;
		}
		DefinedRoadCells.Add(Record.Cell);
		Model->RoadDefinitionIds.Add(Record.Cell, Record.DefinitionId);
	}

	Model->RoadStructures = State.RoadStructures;
	if (!Model->ValidateRoadStructures(OutError))
		return nullptr;

	TSet<uint64> UniqueIds;
	uint64 MaxEntityId = 0;
	for (const FCityPlacedEntity& Entity : State.Entities)
	{
		const FCityBuildingDefinition* Definition = Definitions.Find(Entity.DefinitionId);
		if (Entity.EntityId == FCityGrid::EmptyEntityId || Entity.EntityId == RoadEntityId ||
		    UniqueIds.Contains(Entity.EntityId))
		{
			OutError = TEXT("Save contains a duplicate or reserved entity ID");
			return nullptr;
		}
		if (!Definition)
		{
			OutError = FString::Printf(TEXT("Unknown building content ID: %s"), *Entity.DefinitionId.ToString());
			return nullptr;
		}
		if (Entity.Anchor.RoadLevel != 0 || Entity.QuarterTurnsClockwise < 0 || Entity.QuarterTurnsClockwise > 3 ||
		    Entity.RecordedPlacementCost < 0 ||
		    Entity.FootprintSize != Definition->Footprint.GetRotatedSize(Entity.QuarterTurnsClockwise))
		{
			OutError = FString::Printf(TEXT("Invalid saved footprint for entity %llu"), Entity.EntityId);
			return nullptr;
		}
		if (!Model->Grid.CanOccupy(FCityCellCoord(Entity.Anchor.X, Entity.Anchor.Y, 1), Definition->Footprint,
		                           Entity.QuarterTurnsClockwise))
		{
			OutError = TEXT("Building overlaps a raised corridor");
			return nullptr;
		}
		if (!Model->Grid.TryOccupy(Entity.Anchor, Definition->Footprint, Entity.EntityId, Entity.QuarterTurnsClockwise))
		{
			OutError = FString::Printf(TEXT("Entity %llu overlaps or is out of bounds"), Entity.EntityId);
			return nullptr;
		}
		UniqueIds.Add(Entity.EntityId);
		Model->Entities.Add(Entity.EntityId, Entity);
		MaxEntityId = FMath::Max(MaxEntityId, Entity.EntityId);
	}
	if (State.NextEntityId <= MaxEntityId)
	{
		OutError = TEXT("Next entity ID is not greater than saved IDs");
		return nullptr;
	}

	Model->Zones = State.Zones;
	Model->Revision = State.Revision;
	Model->NextEntityId = State.NextEntityId;
	Model->RebuildRoadConnectivity();
	return Model;
}

uint8 FCityPlacementModel::GetRoadNeighborMask(const FCityCellCoord& Cell) const
{
	if (!RoadCells.Contains(Cell))
	{
		return 0;
	}

	uint8 Mask = 0;
	TArray<FCityCellCoord> Neighbors;
	GetConnectedRoadNeighbors(Cell, Neighbors);
	for (const auto& N : Neighbors)
		Mask |= N.Y > Cell.Y ? 1 : N.X > Cell.X ? 2 : N.Y < Cell.Y ? 4 : 8;
	return Mask;
}

FString FCityPlacementModel::FailureToString(const ECityPlacementFailure Failure)
{
	switch (Failure)
	{
	case ECityPlacementFailure::None:
		return TEXT("Valid");
	case ECityPlacementFailure::NoActivePreview:
		return TEXT("No active preview");
	case ECityPlacementFailure::InvalidDefinition:
		return TEXT("Invalid building definition");
	case ECityPlacementFailure::StalePreview:
		return TEXT("City changed; refresh preview");
	case ECityPlacementFailure::OutOfBounds:
		return TEXT("Footprint is outside the city");
	case ECityPlacementFailure::Occupied:
		return TEXT("Footprint overlaps another building");
	case ECityPlacementFailure::WaterOrShore:
		return TEXT("Foundation requires dry inland ground");
	case ECityPlacementFailure::FoundationRelief:
		return TEXT("Terrain relief exceeds the foundation limit");
	case ECityPlacementFailure::TechnologyPrerequisite:
		return TEXT("LOCKED - operating technology prerequisites required");
	case ECityPlacementFailure::FullRoadFrontage:
		return TEXT("This facility needs ground road across its entire entrance frontage");
	case ECityPlacementFailure::InsufficientFunds:
		return TEXT("Insufficient funds");
	default:
		return TEXT("Unknown placement error");
	}
}

FString FCityPlacementModel::FailureToString(const ECityRoadFailure Failure)
{
	switch (Failure)
	{
	case ECityRoadFailure::None:
		return TEXT("Valid road");
	case ECityRoadFailure::NoActivePreview:
		return TEXT("Choose a road start cell");
	case ECityRoadFailure::StalePreview:
		return TEXT("City changed; restart the road");
	case ECityRoadFailure::OutOfBounds:
		return TEXT("Road leaves the city");
	case ECityRoadFailure::Blocked:
		return TEXT("Road crosses a building");
	case ECityRoadFailure::InvalidStructure:
		return TEXT("Invalid span: straight dry approaches, safe ramps and clearance required");
	case ECityRoadFailure::UnsupportedGrade:
		return TEXT("Grade is too steep for supported roadwork");
	case ECityRoadFailure::InsufficientFunds:
		return TEXT("Insufficient funds");
	default:
		return TEXT("Unknown road error");
	}
}

FCityRoadTerrainEvaluation FCityPlacementModel::EvaluateRoadTerrain(const FCityCellCoord& Cell) const
{
	FCityRoadTerrainEvaluation Result;
	Result.Cost = RoadCellCost;
	if (!Terrain || !Grid.IsValidCell(Cell))
	{
		return Result;
	}
	if (Terrain->IsWaterCell(Cell))
	{
		Result.TerrainClass = ECityRoadTerrainClass::Bridge;
		Result.Cost += BridgeSurcharge;
		return Result;
	}
	const int32 Relief = Terrain->GetCellMaxHeightCm(Cell) - Terrain->GetCellMinHeightCm(Cell);
	if (Relief > MaximumSupportedRoadReliefCm)
	{
		Result.TerrainClass = ECityRoadTerrainClass::Invalid;
		return Result;
	}
	if (Relief > 240)
	{
		Result.TerrainClass = ECityRoadTerrainClass::Supported;
		Result.Cost += SupportedGradeSurcharge;
	}
	return Result;
}

void FCityPlacementModel::BuildOrthogonalRoadPath(const FCityCellCoord& Start, const FCityCellCoord& End,
                                                  TArray<FCityCellCoord>& OutPath)
{
	OutPath.Reset();
	OutPath.Add(Start);
	const int32 XStep = End.X >= Start.X ? 1 : -1;
	for (int32 X = Start.X + XStep; X != End.X + XStep; X += XStep)
	{
		OutPath.Emplace(X, Start.Y);
	}
	const int32 YStep = End.Y >= Start.Y ? 1 : -1;
	for (int32 Y = Start.Y + YStep; Y != End.Y + YStep; Y += YStep)
	{
		OutPath.Emplace(End.X, Y);
	}
	if (!OutPath.IsEmpty())
		OutPath.Last().RoadLevel = End.RoadLevel;
}

void FCityPlacementModel::AppendFootprintCells(TArray<FCityCellCoord>& OutCells, const FCityCellCoord& Anchor,
                                               const FIntPoint& Size)
{
	OutCells.Reserve(Size.X * Size.Y);
	for (int32 Y = Anchor.Y; Y < Anchor.Y + Size.Y; ++Y)
	{
		for (int32 X = Anchor.X; X < Anchor.X + Size.X; ++X)
		{
			OutCells.Emplace(X, Y);
		}
	}
}

void FCityPlacementModel::RebuildRoadConnectivity()
{
	SpatialCache.Invalidate();
	const FCityCellCoord Entrance = GetEntranceCell();
	ConnectedRoadCells.Reset();
	TQueue<FCityCellCoord> Frontier;
	for (const FCityCellCoord& Cell : RoadCells)
	{
		const int32 EntranceDistance = FMath::Abs(Cell.X - Entrance.X) + FMath::Abs(Cell.Y - Entrance.Y);
		if (Cell.RoadLevel == 0 && EntranceDistance <= 1)
		{
			ConnectedRoadCells.Add(Cell);
			Frontier.Enqueue(Cell);
		}
	}

	FCityCellCoord Current;
	while (Frontier.Dequeue(Current))
	{
		TArray<FCityCellCoord> Neighbors;
		GetConnectedRoadNeighbors(Current, Neighbors);
		for (const FCityCellCoord& Neighbor : Neighbors)
		{
			if (CanRoadsConnect(Current, Neighbor) && !ConnectedRoadCells.Contains(Neighbor))
			{
				ConnectedRoadCells.Add(Neighbor);
				Frontier.Enqueue(Neighbor);
			}
		}
	}
}

#include "Presentation/CityDiagnosticOverlay.h"
#include "Simulation/CityHealthSimulation.h"
#include "Simulation/CityWasteSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityHousingHistory.h"
#include "Simulation/CityCommercialAi.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityParkSimulation.h"

#include "Commands/CityPlacementModel.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"

namespace
{
	bool DiagnosticOverlayCellLess(const FCityCellCoord& Left, const FCityCellCoord& Right)
	{
		return Left.Y != Right.Y ? Left.Y < Right.Y : Left.X < Right.X;
	}

	void AddFootprintCells(TSet<FCityCellCoord>& Cells, const FCityCellCoord& Anchor, const FIntPoint& Size,
	                       const FCityGridDimensions Dimensions)
	{
		for (int32 Y = Anchor.Y; Y < Anchor.Y + Size.Y; ++Y)
		{
			for (int32 X = Anchor.X; X < Anchor.X + Size.X; ++X)
			{
				const FCityCellCoord Cell(X, Y);
				if (Cell.X >= 0 && Cell.X < Dimensions.GetWidth() && Cell.Y >= 0 && Cell.Y < Dimensions.GetHeight())
				{
					Cells.Add(Cell);
				}
			}
		}
	}

	void CopySorted(const TSet<FCityCellCoord>& Source, TArray<FCityCellCoord>& Target)
	{
		Target = Source.Array();
		Target.Sort(DiagnosticOverlayCellLess);
	}

	bool IsDemandDefinition(const FName DefinitionId, const FCityEconomyTuning& Tuning)
	{
		return DefinitionId == Tuning.HomeDefinitionId || DefinitionId == Tuning.ShopDefinitionId ||
		       DefinitionId == Tuning.IndustrialDefinitionId;
	}

	bool IsServiceSatisfied(const ECityOperationFailure Failure)
	{
		return Failure == ECityOperationFailure::None || Failure == ECityOperationFailure::NoPower ||
		       Failure == ECityOperationFailure::PowerCapacity;
	}
} // namespace

void FCityDiagnosticOverlayEvaluator::BuildEducationReach(const FCityPlacementModel& City,
                                                          const FCityServiceSnapshot& Services,
                                                          const FCityServiceReach& Reach, const FString& Label,
                                                          const int32 Range, FCityDiagnosticOverlayResult& Out)
{
	Out = {};
	Out.Dimensions = City.GetGrid().GetDimensions();
	Out.Overlay = ECityDiagnosticOverlay::Education;
	Out.Description = Label;
	Out.Values.Init(0, Out.Dimensions.GetCellCount());
	Out.Status.Init(0, Out.Dimensions.GetCellCount());
	Out.Legend = FString::Printf(
	    TEXT(
	        "%s | reach %d road steps\nCyan = potential reach; places need matching tier, staff and utilities\nHomes: green all / yellow some / red no study places (all schools)"),
	    *Label, Range);
	const FCityCellCoord Offsets[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
	for (const auto& R : Reach.RoadHopDistances)
	{
		Out.RoadCells.Add(R.Key);
		Out.CellDetails.Add(R.Key, FString::Printf(TEXT("Road (%d,%d): %d/%d steps from school frontage"), R.Key.X,
		                                           R.Key.Y, R.Value, Range));
		if (!R.Key.RoadLevel)
			Out.Status[R.Key.Y * Out.Dimensions.GetWidth() + R.Key.X] = 7;
		for (const auto& Offset : Offsets)
		{
			const FCityCellCoord Cell(R.Key.X + Offset.X, R.Key.Y + Offset.Y);
			if (City.GetGrid().IsValidCell(Cell) && !City.GetGrid().GetOccupant(Cell) && !City.IsRoad(Cell))
			{
				Out.Status[Cell.Y * Out.Dimensions.GetWidth() + Cell.X] = 7;
				Out.CellDetails.Add(
				    Cell,
				    TEXT("Potential school reach beside roads; staffing, places and education tier still required"));
			}
		}
	}
	Out.RoadCells.Sort(
	    [](const auto& A, const auto& B)
	    { return A.RoadLevel != B.RoadLevel ? A.RoadLevel < B.RoadLevel : DiagnosticOverlayCellLess(A, B); });
	for (const auto& E : City.GetEntities())
	{
		if (E.Value.DefinitionId != TEXT("Home") ||
		    !FCityServiceEvaluator::IsFootprintReached(City, Reach, E.Value.Anchor, E.Value.FootprintSize))
			continue;
		const int32 Demand = Services.HomeEducationDemand.FindRef(E.Key),
		            Served = Services.HomeEducationPlaces.FindRef(E.Key);
		for (int32 Y = E.Value.Anchor.Y; Y < E.Value.Anchor.Y + E.Value.FootprintSize.Y; ++Y)
			for (int32 X = E.Value.Anchor.X; X < E.Value.Anchor.X + E.Value.FootprintSize.X; ++X)
			{
				const int32 I = Y * Out.Dimensions.GetWidth() + X;
				Out.Values[I] = Demand > 0 ? Served * 100 / Demand : 0;
				Out.Status[I] = Demand == 0 ? 7 : Served >= Demand ? 1 : Served > 0 ? 3 : 2;
				Out.CellDetails.Add(
				    {X, Y},
				    FString::Printf(
				        TEXT(
				            "Home #%llu in potential school reach\nCurrent places from all schools %d/%d; inspect home for tier, route and capacity"),
				        E.Key, Served, Demand));
			}
	}
}

void FCityDiagnosticOverlayEvaluator::Build(
    const FCityPlacementModel& City, const FCityServiceSnapshot& Services, const FCityNeedsSnapshot& Needs,
    const FCityEconomyTuning& Tuning, const FCityBuildingDefinition* HomeDefinition,
    const FCityBuildingDefinition* ShopDefinition, const FCityBuildingDefinition* IndustryDefinition,
    const FCityEnvironmentSnapshot& Environment, const ECityDiagnosticOverlay Overlay, const uint64 FocusEntityId,
    FCityDiagnosticOverlayResult& OutResult, const FCityTrafficSnapshot* Traffic,
    const FCityRoadNetworkSnapshot* Network, const FCityDevelopmentSnapshot* Development,
    const FCityHealthSnapshot* Health, const FCityPopulationSnapshot* People, const FCityWasteSnapshot* Waste)
{
	OutResult = FCityDiagnosticOverlayResult();
	const auto Dimensions = City.GetGrid().GetDimensions();
	OutResult.Dimensions = Dimensions;
	OutResult.Overlay = Overlay;
	OutResult.Description = DisplayName(Overlay);
	if (Services.Dimensions != Dimensions || Environment.Dimensions != Dimensions ||
	    (Network && Network->Dimensions != Dimensions) || (Waste && Waste->Dimensions != Dimensions))
	{
		OutResult.Description += TEXT(" unavailable: city dimensions disagree");
		return;
	}
	if (Overlay == ECityDiagnosticOverlay::None)
	{
		return;
	}
    if (Overlay == ECityDiagnosticOverlay::CommercialAi)
    {
        OutResult.Values.Init(0, Dimensions.GetCellCount());
        OutResult.Status.Init(0, Dimensions.GetCellCount());
        OutResult.Legend = TEXT("AI pilot shops: green = paid + staffed; amber = paid, staffing/payroll gap; red = selected, awaiting fee; gray = ordinary/offline.\nPoint at a shop for its exact state and access center; this view changes no city facts.");
        for (const auto& Pair : City.GetEntities())
        {
            if (Pair.Value.DefinitionId != Tuning.ShopDefinitionId) continue;
            const uint8 State = Services.CommercialAiStaffedShops.Contains(Pair.Key) ? 4
                              : Services.CommercialAiShops.Contains(Pair.Key) ? 3
                              : Services.CommercialAiProviders.Contains(Pair.Key) ? 2 : 1;
            const FString Detail = People ? FCityCommercialAi::Status(*People, Services, Pair.Key) : TEXT("Unknown population state");
            for (int32 Y=Pair.Value.Anchor.Y; Y<Pair.Value.Anchor.Y+Pair.Value.FootprintSize.Y; ++Y)
                for (int32 X=Pair.Value.Anchor.X; X<Pair.Value.Anchor.X+Pair.Value.FootprintSize.X; ++X)
                {
                    const FCityCellCoord Cell(X,Y);
                    if (!City.GetGrid().IsValidCell(Cell)) continue;
                    OutResult.Status[Y*Dimensions.GetWidth()+X]=State;
                    OutResult.Values[Y*Dimensions.GetWidth()+X]=State;
                    FString Link = Services.CommercialAiProviders.Contains(Pair.Key) ? FString::Printf(TEXT(" | center #%llu"), Services.CommercialAiProviders.FindRef(Pair.Key)) : FString();
                    OutResult.CellDetails.Add(Cell,FString::Printf(TEXT("Shop #%llu | %s%s"),Pair.Key,*Detail,*Link));
                }
        }
        return;
    }
	if (Overlay == ECityDiagnosticOverlay::Education)
		if (const auto* Focus = City.GetEntities().Find(FocusEntityId);
		    Focus && FCityBuildingDefinition::IsSchoolFacility(Focus->DefinitionId))
		{
			FCityServiceReach Reach;
			const int32 Range = FMath::Clamp(Tuning.SchoolRoadRangeHops, 1, 64);
			FCityServiceEvaluator::BuildProviderReach(City, FocusEntityId, Range, true, Reach);
			BuildEducationReach(City, Services, Reach, Focus->DefinitionId.ToString() + TEXT(" potential reach"), Range,
			                    OutResult);
			OutResult.Legend += FString::Printf(
			    TEXT("\nCurrent places used %d/%d | %s"), Services.SchoolUsedCapacity.FindRef(FocusEntityId),
			    Services.SchoolCapacities.FindRef(FocusEntityId),
			    Services.SchoolCapacities.FindRef(FocusEntityId) > 0 ? TEXT("operating")
			                                                         : TEXT("offline / no staffed places"));
			return;
		}
	if (Overlay == ECityDiagnosticOverlay::Happiness)
	{
		OutResult.Values.Init(0, Dimensions.GetCellCount());
		OutResult.Status.Init(0, Dimensions.GetCellCount());
		OutResult.Legend = TEXT(
		    "Home happiness /100 | red <50 | amber 50-69 | green 70+\nGray = vacant | HOME SEARCH badges identify seeking households\nDaily recorded mood; new residents start at60");
		TMap<uint64, FCityHappinessSummary> Homes;
		TSet<uint64> Searching;
		if (People)
			for (const auto& C : People->Citizens)
			{
				auto& H = Homes.FindOrAdd(C.HomeEntityId);
				++H.Residents;
				H.Average += C.Happiness;
				H.Target += C.HappinessTarget;
				H.Discontented += C.Happiness < 50;
				H.Sustained += C.DiscontentMonths >= 3;
				H.Concerns |= C.HappinessConcerns;
				if (FCityHousingHistory::IsSearching(static_cast<uint8>(C.HousingStatus)))
					Searching.Add(C.HomeEntityId);
			}
		for (const auto& Pair : City.GetEntities())
		{
			if (Pair.Value.DefinitionId != Tuning.HomeDefinitionId)
				continue;
			const auto* H = Homes.Find(Pair.Key);
			const bool Seek = Searching.Contains(Pair.Key);
			const FString Detail =
			    H && H->Residents
			        ? FString::Printf(
			              TEXT(
			                  "Home #%llu | happiness %d/100 | outlook %d\nDiscontented %d/%d | sustained %d | %s\n%s"),
			              Pair.Key, H->Average / H->Residents, H->Target / H->Residents, H->Discontented, H->Residents,
			              H->Sustained, Seek ? TEXT("HOME SEARCH") : TEXT("no active search"),
			              *FCityHappinessEvaluator::DescribeConcerns(H->Concerns))
			        : FString::Printf(TEXT("Home #%llu | vacant; no resident happiness"), Pair.Key);
			for (int32 Y = Pair.Value.Anchor.Y; Y < Pair.Value.Anchor.Y + Pair.Value.FootprintSize.Y; ++Y)
				for (int32 X = Pair.Value.Anchor.X; X < Pair.Value.Anchor.X + Pair.Value.FootprintSize.X; ++X)
				{
					const FCityCellCoord Cell(X, Y);
					const int32 I = Y * Dimensions.GetWidth() + X;
					OutResult.CellDetails.Add(Cell, Detail);
					if (!H || !H->Residents)
						continue;
					OutResult.Values[I] = H->Average / H->Residents;
					OutResult.Status[I] = Seek ? 3 : H->Discontented ? 2 : 1;
					(H->Discontented || Seek ? OutResult.WarningCells : OutResult.PositiveCells).Add(Cell);
				}
		}
		OutResult.PositiveCells.Sort(DiagnosticOverlayCellLess);
		OutResult.WarningCells.Sort(DiagnosticOverlayCellLess);
		return;
	}
	if (Overlay == ECityDiagnosticOverlay::Service &&
	    (Services.Water.RulesVersion > 0 || !Services.Water.EntityDemand.IsEmpty()))
	{
		OutResult.Values.Init(0, Dimensions.GetCellCount());
		OutResult.Status.Init(0, Dimensions.GetCellCount());
		OutResult.DisplayMaximum = 100;
		OutResult.Legend = FString::Printf(
		    TEXT(
		        "Blue = pipe pressure; homes green full / yellow partial / red none\nWater %d/%d used | %s\nBlue pressure does not guarantee enough source output; inspect shortages"),
		    Services.ServiceUsed, Services.ServiceCapacity,
		    Services.Water.RulesVersion ? TEXT("pressure falls with road distance")
		                                : TEXT("legacy city: original water rules"));
		for (const auto& Pair : Services.Water.RoadPressurePercent)
		{
			if (!Dimensions.IsValidCell(Pair.Key))
				continue;
			if (Pair.Value < FCityWaterEvaluator::MinimumUsablePressurePercent)
				continue;
			OutResult.RoadCells.Add(Pair.Key);
			const int32 Index = Pair.Key.Y * Dimensions.GetWidth() + Pair.Key.X;
			if (!Pair.Key.RoadLevel)
			{
				OutResult.Values[Index] = Pair.Value;
				OutResult.Status[Index] = 7; // Raised pipes render separately at deck height.
			}
			OutResult.CellDetails.Add(
			    Pair.Key, FString::Printf(TEXT("(%d,%d) %s pipe: pressure %d%%"), Pair.Key.X, Pair.Key.Y,
			                              Pair.Key.RoadLevel ? TEXT("raised") : TEXT("ground"), Pair.Value));
		}
		OutResult.RoadCells.Sort(
		    [](const auto& A, const auto& B)
		    { return A.RoadLevel != B.RoadLevel ? A.RoadLevel < B.RoadLevel : DiagnosticOverlayCellLess(A, B); });
		for (const auto& Pair : Services.Water.EntityDemand)
		{
			const auto* Entity = City.GetEntities().Find(Pair.Key);
			if (!Entity || Pair.Value <= 0)
				continue;
			const int32 Delivered = Services.Water.EntityDelivered.FindRef(Pair.Key);
			const int32 Pressure = Services.Water.EntityPressurePercent.FindRef(Pair.Key);
			const auto Cause = Services.Water.EntityCauses.FindRef(Pair.Key);
			const FString Detail = FString::Printf(TEXT("Water %d/%d | pressure %d%% | %s"), Delivered, Pair.Value,
			                                       Pressure, *FCityWaterEvaluator::FailureToString(Cause));
			for (int32 Y = Entity->Anchor.Y; Y < Entity->Anchor.Y + Entity->FootprintSize.Y; ++Y)
				for (int32 X = Entity->Anchor.X; X < Entity->Anchor.X + Entity->FootprintSize.X; ++X)
				{
					const FCityCellCoord Cell(X, Y);
					const int32 Index = Y * Dimensions.GetWidth() + X;
					OutResult.Values[Index] = static_cast<int32>(static_cast<int64>(Delivered) * 100 / Pair.Value);
					OutResult.Status[Index] = Delivered >= Pair.Value ? 1 : Delivered > 0 ? 3 : 2;
					(Delivered >= Pair.Value ? OutResult.PositiveCells : OutResult.WarningCells).Add(Cell);
					OutResult.CellDetails.Add(Cell, FString::Printf(TEXT("(%d,%d) %s"), X, Y, *Detail));
				}
		}
		OutResult.PositiveCells.Sort(DiagnosticOverlayCellLess);
		OutResult.WarningCells.Sort(DiagnosticOverlayCellLess);
		return;
	}

	TSet<FCityCellCoord> Roads;
	TSet<FCityCellCoord> Positive;
	TSet<FCityCellCoord> Warning;
	if (Overlay == ECityDiagnosticOverlay::Service)
	{
		FCityServiceReach Reach = Services.ServiceReach;
		if (const FCityPlacedEntity* Focus = City.GetEntities().Find(FocusEntityId);
		    Focus && Focus->DefinitionId == Tuning.WaterTreatmentDefinitionId)
		{
			const bool bOperational = Services.GetFailure(FocusEntityId) == ECityOperationFailure::None;
			FCityServiceEvaluator::BuildProviderReach(City, FocusEntityId, Tuning.WaterRangeRoadHops, bOperational,
			                                          Reach);
		}
		for (const TPair<FCityCellCoord, int32>& Reached : Reach.RoadHopDistances)
		{
			Roads.Add(Reached.Key);
		}
		for (const TPair<uint64, FCityPlacedEntity>& Pair : City.GetEntities())
		{
			if (!IsDemandDefinition(Pair.Value.DefinitionId, Tuning))
			{
				continue;
			}
			const ECityOperationFailure Failure = Services.GetFailure(Pair.Key);
			AddFootprintCells(
			    (IsServiceSatisfied(Failure) &&
			     FCityServiceEvaluator::IsFootprintReached(City, Reach, Pair.Value.Anchor, Pair.Value.FootprintSize))
			        ? Positive
			        : Warning,
			    Pair.Value.Anchor, Pair.Value.FootprintSize, Dimensions);
		}
	}
	else if (Overlay == ECityDiagnosticOverlay::Power)
	{
		for (const TPair<uint64, FCityPlacedEntity>& Pair : City.GetEntities())
		{
			const bool bPower = FCityBuildingDefinition::IsPowerFacility(Pair.Value.DefinitionId) ||
			                    Pair.Value.DefinitionId == Tuning.PowerDefinitionId;
			const bool bDemand = IsDemandDefinition(Pair.Value.DefinitionId, Tuning);
			if (!bPower && !bDemand)
			{
				continue;
			}
			const ECityOperationFailure Failure = Services.GetFailure(Pair.Key);
			const bool bPositive = Failure == ECityOperationFailure::None;
			AddFootprintCells(bPositive ? Positive : Warning, Pair.Value.Anchor, Pair.Value.FootprintSize, Dimensions);
		}
	}
	else if (Overlay == ECityDiagnosticOverlay::Pollution)
	{
		const int32 WarningThreshold =
		    FMath::Max(1, Tuning.MinimumResidentialDesirability * Tuning.PollutionPerDesirabilityPenalty);
		for (int32 Y = 0; Y < Dimensions.GetHeight(); ++Y)
		{
			for (int32 X = 0; X < Dimensions.GetWidth(); ++X)
			{
				const FCityCellCoord Cell(X, Y);
				const int32 Pollution = Environment.GetPollution(Cell);
				if (Pollution > 0)
				{
					(Pollution >= WarningThreshold ? Warning : Positive).Add(Cell);
				}
			}
		}
	}
	else if (Overlay != ECityDiagnosticOverlay::Traffic && Overlay != ECityDiagnosticOverlay::Crime &&
	         Overlay != ECityDiagnosticOverlay::Fire && Overlay != ECityDiagnosticOverlay::Education &&
	         Overlay != ECityDiagnosticOverlay::Parks && Overlay != ECityDiagnosticOverlay::Health &&
	         Overlay != ECityDiagnosticOverlay::Waste)
	{
		const ECityZoneType Zone = Overlay == ECityDiagnosticOverlay::ResidentialDemand  ? ECityZoneType::Residential
		                           : Overlay == ECityDiagnosticOverlay::CommercialDemand ? ECityZoneType::Commercial
		                                                                                 : ECityZoneType::Industrial;
		const ECityDemandConstraint Constraint = Zone == ECityZoneType::Residential  ? Needs.ResidentialConstraint
		                                         : Zone == ECityZoneType::Commercial ? Needs.CommercialConstraint
		                                                                             : Needs.IndustrialConstraint;
		const int32 Demand = Zone == ECityZoneType::Residential  ? Needs.ResidentialDemand
		                     : Zone == ECityZoneType::Commercial ? Needs.CommercialDemand
		                                                         : Needs.IndustrialDemand;
		const FCityBuildingDefinition* Definition = Zone == ECityZoneType::Residential  ? HomeDefinition
		                                            : Zone == ECityZoneType::Commercial ? ShopDefinition
		                                                                                : IndustryDefinition;
		for (int32 Y = 0; Y < Dimensions.GetHeight(); ++Y)
		{
			for (int32 X = 0; X < Dimensions.GetWidth(); ++X)
			{
				const FCityCellCoord Cell(X, Y);
				if (City.GetZone(Cell) != Zone || City.GetGrid().GetOccupant(Cell) != FCityGrid::EmptyEntityId)
				{
					continue;
				}
				Warning.Add(Cell);
			}
		}
		if (Demand > 0 && Definition && Constraint != ECityDemandConstraint::OperatingShortfall)
		{
			FCityBuildingDefinition GrowthDefinition = *Definition;
			GrowthDefinition.Footprint = FCityCellFootprint(2, 2);
			const FIntPoint GrowthSize(2, 2);
			for (int32 Y = 0; Y <= Dimensions.GetHeight() - GrowthSize.Y; ++Y)
			{
				for (int32 X = 0; X <= Dimensions.GetWidth() - GrowthSize.X; ++X)
				{
					const FCityCellCoord Anchor(X, Y);
					bool bUniformEmpty = true;
					for (int32 CellY = Y; CellY < Y + GrowthSize.Y && bUniformEmpty; ++CellY)
					{
						for (int32 CellX = X; CellX < X + GrowthSize.X; ++CellX)
						{
							const FCityCellCoord Cell(CellX, CellY);
							if (City.GetZone(Cell) != Zone ||
							    City.GetGrid().GetOccupant(Cell) != FCityGrid::EmptyEntityId)
							{
								bUniformEmpty = false;
								break;
							}
						}
					}
					if (!bUniformEmpty ||
					    FCityServiceEvaluator::EvaluateCandidate(City, Services, Tuning, Definition->DefinitionId,
					                                             Anchor, GrowthSize) != ECityOperationFailure::None)
					{
						continue;
					}
					const FCityPlacementPreview Preview = City.BeginPreview(GrowthDefinition, Anchor);
					if (City.ValidatePlacement(Preview, GrowthDefinition, ECityConstructionFunding::PrivateDeveloper)
					        .IsSuccess() &&
					    (Zone != ECityZoneType::Residential ||
					     FCityEnvironmentSimulation::EvaluateResidentialDesirability(City, Services, Environment,
					                                                                 Anchor, GrowthSize, Tuning)
					         .IsResidentialEligible(Tuning.MinimumResidentialDesirability)))
					{
						AddFootprintCells(Positive, Anchor, GrowthSize, Dimensions);
					}
				}
			}
		}
	}

	for (const FCityCellCoord& Cell : Positive)
	{
		Warning.Remove(Cell);
	}
	CopySorted(Roads, OutResult.RoadCells);
	CopySorted(Positive, OutResult.PositiveCells);
	CopySorted(Warning, OutResult.WarningCells);
	OutResult.Values.Init(0, Dimensions.GetCellCount());
	OutResult.Status.Init(0, Dimensions.GetCellCount());
	for (const auto& Cell : Positive)
		OutResult.Status[Cell.Y * Dimensions.GetWidth() + Cell.X] = 1;
	for (const auto& Cell : Warning)
		OutResult.Status[Cell.Y * Dimensions.GetWidth() + Cell.X] = 2;
	if (Overlay == ECityDiagnosticOverlay::Waste && Waste)
	{
		OutResult.DisplayMaximum = 100;
		OutResult.Legend = FString::Printf(
		    TEXT(
		        "Waste pile /capacity%%\nLots %lld | trucks %lld | storage %lld\nLargest reachable piles first; ordinary traffic"),
		    Waste->Piled(), Waste->InTrucks(), Waste->InDepots());
		for (const auto& S : Waste->Sites)
			if (const auto* E = City.GetEntities().Find(S.EntityId))
				for (int32 Y = E->Anchor.Y; Y < E->Anchor.Y + E->FootprintSize.Y; ++Y)
					for (int32 X = E->Anchor.X; X < E->Anchor.X + E->FootprintSize.X; ++X)
					{
						const int32 I = Y * Dimensions.GetWidth() + X;
						OutResult.Values[I] = S.Units * 100 / S.Capacity;
						OutResult.Status[I] = S.Units * 2 >= S.Capacity ? 2 : 1;
					}
	}
	else if (Overlay == ECityDiagnosticOverlay::Health && Health && People)
	{
		TMap<uint64, int32> Residents, Ill;
		for (const auto& C : People->Citizens)
			++Residents.FindOrAdd(C.HomeEntityId);
		for (const auto& P : Health->Patients)
			if (const auto* C = People->Citizens.FindByPredicate([&](const auto& X) { return X.Id == P.CitizenId; }))
				++Ill.FindOrAdd(C->HomeEntityId);
		OutResult.DisplayMaximum = 100;
		OutResult.Legend = FString::Printf(
		    TEXT(
		        "Ill residents /100%%: dark to bright\nCity ill %d | recovered %lld\nInspect homes and hospitals for care"),
		    Health->Patients.Num(), Health->TotalRecovered);
		for (const auto& E : City.GetEntities())
			if (Residents.FindRef(E.Key) > 0)
				for (int32 Y = E.Value.Anchor.Y; Y < E.Value.Anchor.Y + E.Value.FootprintSize.Y; ++Y)
					for (int32 X = E.Value.Anchor.X; X < E.Value.Anchor.X + E.Value.FootprintSize.X; ++X)
					{
						const int32 I = Y * Dimensions.GetWidth() + X;
						OutResult.Values[I] = Ill.FindRef(E.Key) * 100 / Residents.FindRef(E.Key);
						OutResult.Status[I] = 1;
					}
	}
	else if (Overlay == ECityDiagnosticOverlay::Pollution)
	{
		OutResult.DisplayMaximum =
		    FMath::Max(1, Tuning.MinimumResidentialDesirability * Tuning.PollutionPerDesirabilityPenalty);
		OutResult.Legend = FString::Printf(TEXT("Exposure: 0 dark - %d+ bright/raised"), OutResult.DisplayMaximum);
		for (int32 Y = 0; Y < Dimensions.GetHeight(); ++Y)
			for (int32 X = 0; X < Dimensions.GetWidth(); ++X)
			{
				const int32 Index = Y * Dimensions.GetWidth() + X;
				OutResult.Values[Index] = Environment.GetPollution({X, Y});
				OutResult.Status[Index] = 1;
			}
	}
	else if (Overlay == ECityDiagnosticOverlay::Parks)
	{
		OutResult.DisplayMaximum = FCityParkEvaluator::MaximumBonus;
		OutResult.Legend = FString::Printf(
		    TEXT(
		        "Park appeal +0 to +30: dark to bright\nOperating %d/%d parks | upkeep $%lld/day\nStrongest benefit wins; needs paid bills and connected roads\nLarger parks need power/water; inspect for details"),
		    Services.OperationalParks, Services.BuiltParks, Services.ParkUpkeep);
		for (int32 I = 0; I < Dimensions.GetCellCount(); ++I)
		{
			OutResult.Values[I] = Services.ParkCoverage.IsValidIndex(I) ? Services.ParkCoverage[I] : 0;
			OutResult.Status[I] = 1;
		}
	}
	else if (Overlay == ECityDiagnosticOverlay::Education)
	{
		OutResult.DisplayMaximum = 100;
		OutResult.Legend = FString::Printf(
		    TEXT(
		        "Education access /100: dark to bright\nResident places %d/%d | capacity %d\nSecondary or higher %d; inspect homes for tier/places; inspect schools for road reach"),
		    Services.EducationServed, Services.EducationDemand, Services.EducationCapacity,
		    Services.SecondaryResidents);
		for (int32 I = 0; I < Dimensions.GetCellCount(); ++I)
		{
			OutResult.Values[I] = Services.EducationCoverage.IsValidIndex(I) ? Services.EducationCoverage[I] : 0;
			OutResult.Status[I] = OutResult.Values[I] > 0 ? 1 : 0;
		}
	}
	else if (Overlay == ECityDiagnosticOverlay::Fire)
	{
		OutResult.DisplayMaximum = 100;
		OutResult.Legend = TEXT("Fire risk /100: dark - bright\nRed = burning | amber = damaged | blue = repairing");
		for (int32 Index = 0; Index < Dimensions.GetCellCount(); ++Index)
		{
			OutResult.Values[Index] = Services.FireRisk.IsValidIndex(Index) ? Services.FireRisk[Index] : 0;
			OutResult.Status[Index] = Services.FireStatus.IsValidIndex(Index) ? Services.FireStatus[Index] : 0;
		}
	}
	else if (Overlay == ECityDiagnosticOverlay::Crime)
	{
		OutResult.DisplayMaximum = 100;
		OutResult.Legend = TEXT("Crime risk: 0 dark - 100 bright/raised\nPolice lowers risk; gray = no developed lot");
		for (const auto& Pair : City.GetEntities())
		{
			const auto& Entity = Pair.Value;
			if (Entity.DefinitionId != Tuning.HomeDefinitionId && Entity.DefinitionId != Tuning.ShopDefinitionId &&
			    Entity.DefinitionId != Tuning.IndustrialDefinitionId)
				continue;
			const auto* State = Development ? Development->Find(Pair.Key) : nullptr;
			if (State && State->Stage == ECityDevelopmentStage::Construction)
				continue;
			for (int32 Y = Entity.Anchor.Y; Y < Entity.Anchor.Y + Entity.FootprintSize.Y; ++Y)
				for (int32 X = Entity.Anchor.X; X < Entity.Anchor.X + Entity.FootprintSize.X; ++X)
				{
					const int32 Index = Y * Dimensions.GetWidth() + X;
					OutResult.Status[Index] = 1;
					OutResult.Values[Index] = Services.CrimeRisk.IsValidIndex(Index) ? Services.CrimeRisk[Index] : 0;
				}
		}
	}
	else if (Overlay == ECityDiagnosticOverlay::Traffic)
	{
		OutResult.DisplayMaximum = 1000;
		OutResult.Legend = TEXT("Motor lanes: 0 dark - 1000 full/raised\nHighest lane load per edge; gray = no road");
		for (const auto& Cell : City.GetRoadCells())
			OutResult.Status[Cell.Y * Dimensions.GetWidth() + Cell.X] = 1;
		if (Traffic && Network)
			for (const auto& Load : Traffic->LaneLoads)
			{
				if (Load.bPedestrian || !Network->Edges.IsValidIndex(Load.EdgeId))
					continue;
				for (const auto& Cell : Network->Edges[Load.EdgeId].Cells)
				{
					const int32 Index = Cell.Y * Dimensions.GetWidth() + Cell.X;
					if (OutResult.Values.IsValidIndex(Index))
						OutResult.Values[Index] = FMath::Max(OutResult.Values[Index], Load.CongestionPermille);
				}
			}
	}
	else if (Overlay == ECityDiagnosticOverlay::Service)
	{
		OutResult.DisplayMaximum = FMath::Max(1, Tuning.WaterRangeRoadHops);
		OutResult.Legend = FString::Printf(
		    TEXT("Water: bright = reached; gray = blocked/no reach\nRoad range %d hops | capacity %d/%d used"),
		    Tuning.WaterRangeRoadHops, Services.ServiceUsed, Services.ServiceCapacity);
		FCityServiceReach Reach = Services.ServiceReach;
		if (const auto* Focus = City.GetEntities().Find(FocusEntityId);
		    Focus && Focus->DefinitionId == Tuning.WaterTreatmentDefinitionId)
			FCityServiceEvaluator::BuildProviderReach(City, FocusEntityId, Tuning.WaterRangeRoadHops,
			                                          Services.GetFailure(FocusEntityId) == ECityOperationFailure::None,
			                                          Reach);
		for (const auto& Reached : Reach.RoadHopDistances)
		{
			if (!Dimensions.IsValidCell(Reached.Key))
				continue;
			const int32 Index = Reached.Key.Y * Dimensions.GetWidth() + Reached.Key.X;
			OutResult.Values[Index] = Reached.Value;
			OutResult.Status[Index] = 1;
		}
	}
	else if (Overlay == ECityDiagnosticOverlay::Power)
	{
		OutResult.DisplayMaximum = 1;
		OutResult.Legend =
		    FString::Printf(TEXT("Bright/raised = operational | gray = blocked\nPower capacity %d/%d used"),
		                    Services.PowerUsed, Services.PowerCapacity);
		for (int32 Index = 0; Index < Dimensions.GetCellCount(); ++Index)
			OutResult.Values[Index] = OutResult.Status[Index] == 1 ? 1 : 0;
	}
	else
	{
		const int32 Balance = Overlay == ECityDiagnosticOverlay::ResidentialDemand  ? Needs.ResidentialDemandBalance
		                      : Overlay == ECityDiagnosticOverlay::CommercialDemand ? Needs.CommercialDemandBalance
		                                                                            : Needs.IndustrialDemandBalance;
		OutResult.DisplayMaximum = 100;
		OutResult.Legend = FString::Printf(
		    TEXT("City demand %+d | brightness/height 0-100+\nVacant: eligibility | buildings: upgrade pressure"),
		    Balance);
		for (int32 Index = 0; Index < Dimensions.GetCellCount(); ++Index)
			if (OutResult.Status[Index])
				OutResult.Values[Index] = Balance;
		const FName DefinitionId = Overlay == ECityDiagnosticOverlay::ResidentialDemand ? Tuning.HomeDefinitionId
		                           : Overlay == ECityDiagnosticOverlay::CommercialDemand
		                               ? Tuning.ShopDefinitionId
		                               : Tuning.IndustrialDefinitionId;
		if (Development)
			for (const auto& Pair : Development->LatestPressures)
			{
				const auto* Entity = City.GetEntities().Find(Pair.Key);
				if (!Entity || Entity->DefinitionId != DefinitionId)
					continue;
				for (int32 Y = Entity->Anchor.Y; Y < Entity->Anchor.Y + Entity->FootprintSize.Y; ++Y)
					for (int32 X = Entity->Anchor.X; X < Entity->Anchor.X + Entity->FootprintSize.X; ++X)
					{
						const int32 Index = Y * Dimensions.GetWidth() + X;
						if (!OutResult.Values.IsValidIndex(Index))
							continue;
						OutResult.Values[Index] = Pair.Value.Total;
						OutResult.Status[Index] = 3;
					}
			}
	}
}

ECityDiagnosticOverlay FCityDiagnosticOverlayEvaluator::OverlayForOperationFailure(
    const ECityOperationFailure OperationFailure)
{
	switch (OperationFailure)
	{
	case ECityOperationFailure::NoService:
	case ECityOperationFailure::ServiceCapacity:
	case ECityOperationFailure::LowWaterPressure:
		return ECityDiagnosticOverlay::Service;
	case ECityOperationFailure::NoPower:
	case ECityOperationFailure::PowerCapacity:
		return ECityDiagnosticOverlay::Power;
	default:
		return ECityDiagnosticOverlay::None;
	}
}

FString FCityDiagnosticOverlayEvaluator::DisplayName(const ECityDiagnosticOverlay Overlay)
{
	switch (Overlay)
	{
	case ECityDiagnosticOverlay::Service:
		return TEXT("Water reach/capacity");
	case ECityDiagnosticOverlay::Power:
		return TEXT("Power status/capacity");
	case ECityDiagnosticOverlay::ResidentialDemand:
		return TEXT("Residential demand");
	case ECityDiagnosticOverlay::CommercialDemand:
		return TEXT("Commercial/job demand");
	case ECityDiagnosticOverlay::IndustrialDemand:
		return TEXT("Industrial/job demand");
	case ECityDiagnosticOverlay::Pollution:
		return TEXT("Pollution exposure");
	case ECityDiagnosticOverlay::Education:
		return TEXT("Education access /100");
	case ECityDiagnosticOverlay::Parks:
		return TEXT("Park appeal /30");
	case ECityDiagnosticOverlay::Waste:
		return TEXT("Waste pile /capacity%");
	case ECityDiagnosticOverlay::CommercialAi: return TEXT("Commercial AI participation");
	case ECityDiagnosticOverlay::Happiness:
		return TEXT("Citizen happiness /100");
	case ECityDiagnosticOverlay::Health:
		return TEXT("Ill residents /100%");
	case ECityDiagnosticOverlay::Fire:
		return TEXT("Fire risk /100 and recovery");
	case ECityDiagnosticOverlay::Crime:
		return TEXT("Crime risk /100");
	case ECityDiagnosticOverlay::Traffic:
		return TEXT("Traffic congestion");
	default:
		return TEXT("Off");
	}
}

FLinearColor FCityDiagnosticOverlayResult::ColorAt(const int32 Index) const
{
	const FLinearColor Neutral(0.025f, 0.032f, 0.045f);
	if (!Status.IsValidIndex(Index) || Status[Index] == 0)
		return Neutral;
	if (Overlay == ECityDiagnosticOverlay::Happiness)
		return Values[Index] < 50   ? FLinearColor(1, .06, .03)
		       : Values[Index] < 70 ? FLinearColor(1, .68, .03)
		                            : FLinearColor(.05, .8, .22);
	if (Overlay == ECityDiagnosticOverlay::Education &&
	    (Status[Index] == 7 || Status[Index] == 2 || Status[Index] == 3))
		return Status[Index] == 7   ? FLinearColor(.04f, .65f, 1.f)
		       : Status[Index] == 2 ? FLinearColor(1.f, .04f, .02f)
		                            : FLinearColor(1.f, .8f, .02f);
    if (Overlay == ECityDiagnosticOverlay::CommercialAi)
        return Status[Index] == 4 ? FLinearColor(.08f,.8f,.35f)
             : Status[Index] == 3 ? FLinearColor(1.f,.7f,.15f)
             : Status[Index] == 2 ? FLinearColor(.9f,.25f,.12f) : FLinearColor(.35f,.4f,.45f);
	if (Overlay == ECityDiagnosticOverlay::Education && Status[Index] == 1 && !CellDetails.IsEmpty())
		return FLinearColor(.04f, .7f, .2f);
	if (Overlay == ECityDiagnosticOverlay::Service)
	{
		if (Status[Index] == 7)
			return FLinearColor(.04f, .35f, 1.f);
		if (Status[Index] == 2)
			return FLinearColor(1.f, .04f, .02f);
		if (Status[Index] == 3)
			return FLinearColor(1.f, .8f, .02f);
		return FLinearColor(.04f, .7f, .2f);
	}
	if (Status[Index] == 4)
		return FLinearColor(1, .07, .02);
	if (Status[Index] == 5)
		return FLinearColor(.75, .4, .04);
	if (Status[Index] == 6)
		return FLinearColor(.08, .4, .9);
	if (Overlay == ECityDiagnosticOverlay::Waste)
	{
		const float Strength = FMath::Clamp(Values[Index] / 100.0f, 0.0f, 1.0f);
		return Strength < .5f
		           ? FMath::Lerp(FLinearColor(.03, .12, .10), FLinearColor(.9, .65, .04), Strength * 2)
		           : FMath::Lerp(FLinearColor(.9, .65, .04), FLinearColor(1, .06, .02), (Strength - .5f) * 2);
	}
	if (Status[Index] == 2)
		return FLinearColor(0.04f, 0.04f, 0.04f);
	float Strength = FMath::Clamp(static_cast<float>(Values[Index]) / FMath::Max(1, DisplayMaximum), 0.0f, 1.0f);
	if (Overlay == ECityDiagnosticOverlay::Service)
		Strength = 1.0f - Strength * 0.7f;
	const FLinearColor Low(0.03f, 0.09f, 0.13f);
	const FLinearColor High(1.0f, 0.78f, 0.3f);
	return FMath::Lerp(Low, High, Strength);
}

FString FCityDiagnosticOverlayResult::DescribeCell(const FCityCellCoord& Cell) const
{
	if (Cell.X < 0 || Cell.X >= Dimensions.GetWidth() || Cell.Y < 0 || Cell.Y >= Dimensions.GetHeight())
		return TEXT("Point at the map for exact values");
	if (const auto* Detail = CellDetails.Find(Cell))
		return *Detail;
	const int32 Index = Cell.Y * Dimensions.GetWidth() + Cell.X;
	if (!Values.IsValidIndex(Index) || !Status.IsValidIndex(Index) || !Status[Index])
		return FString::Printf(TEXT("(%d,%d) No data for this view"), Cell.X, Cell.Y);
	if (Overlay == ECityDiagnosticOverlay::Waste)
		return FString::Printf(TEXT("(%d,%d) Waste pile %d%%; inspect lot and depot"), Cell.X, Cell.Y, Values[Index]);
	if (Overlay == ECityDiagnosticOverlay::Health)
		return FString::Printf(TEXT("(%d,%d) Ill residents %d%%; inspect hospital access"), Cell.X, Cell.Y,
		                       Values[Index]);
	if (Overlay == ECityDiagnosticOverlay::Parks)
		return FString::Printf(TEXT("(%d,%d) Park appeal +%d/30; strongest operating benefit"), Cell.X, Cell.Y,
		                       Values[Index]);
	if (Overlay == ECityDiagnosticOverlay::Education)
		return FString::Printf(TEXT("(%d,%d) Education access %d%%; inspect residents/school"), Cell.X, Cell.Y,
		                       Values[Index]);
	if (Overlay == ECityDiagnosticOverlay::Fire)
		return FString::Printf(TEXT("(%d,%d) Fire risk %d/100 | %s"), Cell.X, Cell.Y, Values[Index],
		                       Status[Index] == 4   ? TEXT("BURNING")
		                       : Status[Index] == 5 ? TEXT("DAMAGED: inspect repair cost")
		                       : Status[Index] == 6 ? TEXT("REPAIRING")
		                                            : TEXT("ready"));
	const TCHAR* Label = Overlay == ECityDiagnosticOverlay::Pollution ? TEXT("exposure")
	                     : Overlay == ECityDiagnosticOverlay::Crime   ? TEXT("crime risk /100")
	                     : Overlay == ECityDiagnosticOverlay::Traffic ? TEXT("congestion /1000")
	                     : Overlay == ECityDiagnosticOverlay::Service ? TEXT("water road hops")
	                     : Overlay == ECityDiagnosticOverlay::Power   ? TEXT("operational 0/1")
	                     : Status[Index] == 3                         ? TEXT("upgrade pressure")
	                                                                  : TEXT("city demand balance");
	if (Overlay == ECityDiagnosticOverlay::Service && !RoadCells.Contains(Cell))
		return FString::Printf(TEXT("(%d,%d) Water parcel: %s"), Cell.X, Cell.Y,
		                       Status[Index] == 1 ? TEXT("served") : TEXT("blocked"));
	return FString::Printf(TEXT("(%d,%d) %s: %d%s"), Cell.X, Cell.Y, Label, Values[Index],
	                       Status[Index] == 2 ? TEXT(" | blocked lot") : TEXT(""));
}

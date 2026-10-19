#include "Simulation/CityGrowthSimulation.h"
#include "Performance/CityEvaluationTiming.h"
#include "Simulation/CityUtilitySimulation.h"
#include "Simulation/CityParkSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityHousingMigration.h"
#include "Simulation/CityEducationSimulation.h"
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CityTerrain.h"
#include "Simulation/CityTechnology.h"

#include "Containers/Queue.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityNeedsSimulation.h"

namespace
{
	bool CellLess(const FCityCellCoord& Left, const FCityCellCoord& Right)
	{
		return Left.Y != Right.Y ? Left.Y < Right.Y : Left.X < Right.X;
	}

	void AddClamped(int32& Value, const int32 Amount)
	{
		Value = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Value) + FMath::Max(0, Amount)));
	}

	int32 WaterDemandForDefinition(const FCityEconomyTuning& Tuning, const FName DefinitionId)
	{
		const int64 Demand =
		    DefinitionId == Tuning.HomeDefinitionId
		        ? static_cast<int64>(Tuning.ResidentsPerOccupiedHome) * Tuning.WaterDemandPerResident
		    : DefinitionId == Tuning.ShopDefinitionId
		        ? static_cast<int64>(Tuning.JobsPerOperationalShop) * Tuning.WaterDemandPerCommercialJob
		    : DefinitionId == Tuning.IndustrialDefinitionId ? Tuning.IndustrialWaterDemand
		                                                    : 0;
		return static_cast<int32>(FMath::Clamp<int64>(Demand, 0, MAX_int32));
	}

	bool HasUniformEmptyZone(const FCityPlacementModel& City, const FCityCellCoord& Anchor, const ECityZoneType Zone)
	{
		for (int32 Y = Anchor.Y; Y < Anchor.Y + 2; ++Y)
		{
			for (int32 X = Anchor.X; X < Anchor.X + 2; ++X)
			{
				const FCityCellCoord Cell(X, Y);
				if (City.GetGrid().GetOccupant(Cell) != FCityGrid::EmptyEntityId || City.GetZone(Cell) != Zone)
				{
					return false;
				}
			}
		}
		return true;
	}

	FString OperationBlockerToString(const ECityOperationFailure Failure)
	{
		switch (Failure)
		{
	    case ECityOperationFailure::TechnologyPrerequisite:
		    return TEXT("needs operating technology prerequisites\nFix: inspect Technology under Build");
	    case ECityOperationFailure::NoStaff:
		    return TEXT(
		        "needs qualified public staff/paid wages\nFix: serviced housing and a funded payroll");
	    case ECityOperationFailure::AwaitingFreight:
		    return TEXT("is awaiting opening delivery\nFreight arrives from the city entrance");
		case ECityOperationFailure::NoRoad:
			return TEXT("has no connected road\nFix: connect it to white entrance");
		case ECityOperationFailure::NoPower:
			return TEXT("has no power\nFix: Power beside connected road");
		case ECityOperationFailure::PowerCapacity:
			return TEXT("has no spare power capacity\nFix: add connected Power");
		case ECityOperationFailure::NoService:
			return TEXT("has no usable water supply\nFix: connect an operational water source within its road range");
		case ECityOperationFailure::LowWaterPressure:
			return TEXT("has low water pressure\nFix: place a water source nearer by road");
		case ECityOperationFailure::ServiceCapacity:
			return TEXT("has no spare water capacity\nFix: add connected Water Treatment");
		default:
			return TEXT("is not operational");
		}
	}

	const TCHAR* ZoneToString(const ECityZoneType Zone)
	{
		return Zone == ECityZoneType::Commercial   ? TEXT("Commercial")
		       : Zone == ECityZoneType::Industrial ? TEXT("Industrial")
		                                           : TEXT("Residential");
	}
} // namespace

int32 FCityServiceReach::GetRoadHopDistance(const FCityCellCoord& Cell) const
{
	const int32* Distance = RoadHopDistances.Find(Cell);
	return Distance ? *Distance : INDEX_NONE;
}

void FCityServiceEvaluator::GetAdjacentRoadCells(const FCityPlacementModel& City, const FCityCellCoord& Anchor,
                                                 const FIntPoint& FootprintSize, const bool bConnectedRoadsOnly,
                                                 TArray<FCityCellCoord>& OutRoadCells)
{
	City.GetAdjacentRoadCells(Anchor, FootprintSize, bConnectedRoadsOnly, OutRoadCells);
}

void FCityServiceEvaluator::BuildServiceReach(const FCityPlacementModel& City, const TArray<uint64>& ServiceEntityIds,
                                              const int32 HopBudget, const bool bConnectedRoadsOnly,
                                              FCityServiceReach& OutReach)
{
	OutReach.RoadHopDistances.Reset();
	if (HopBudget < 0)
	{
		return;
	}

	TSet<FCityCellCoord> UniqueSources;
	for (const uint64 EntityId : ServiceEntityIds)
	{
		const FCityPlacedEntity* Entity = City.GetEntities().Find(EntityId);
		if (!Entity)
		{
			continue;
		}
		TArray<FCityCellCoord> ProviderSources;
		GetAdjacentRoadCells(City, Entity->Anchor, Entity->FootprintSize, bConnectedRoadsOnly, ProviderSources);
		for (const FCityCellCoord& ProviderSource : ProviderSources)
		{
			UniqueSources.Add(ProviderSource);
		}
	}

	TArray<FCityCellCoord> Sources = UniqueSources.Array();
	Sources.Sort(CellLess);
	City.GetRoadReach(Sources, HopBudget, bConnectedRoadsOnly, OutReach.RoadHopDistances);
}

void FCityServiceEvaluator::BuildProviderReach(const FCityPlacementModel& City, const uint64 ServiceEntityId,
                                               const int32 HopBudget, const bool bConnectedRoadsOnly,
                                               FCityServiceReach& OutReach)
{
	TArray<uint64> ProviderIds;
	ProviderIds.Add(ServiceEntityId);
	BuildServiceReach(City, ProviderIds, HopBudget, bConnectedRoadsOnly, OutReach);
}

bool FCityServiceEvaluator::IsFootprintReached(const FCityPlacementModel& City, const FCityServiceReach& Reach,
                                               const FCityCellCoord& Anchor, const FIntPoint& FootprintSize)
{
	TArray<FCityCellCoord> AdjacentRoadCells;
	GetAdjacentRoadCells(City, Anchor, FootprintSize, false, AdjacentRoadCells);
	return AdjacentRoadCells.ContainsByPredicate([&Reach](const FCityCellCoord& RoadCell)
	                                             { return Reach.IsRoadReached(RoadCell); });
}

ECityOperationFailure FCityServiceSnapshot::GetFailure(const uint64 EntityId) const
{
	const ECityOperationFailure* Failure = EntityFailures.Find(EntityId);
	return Failure ? *Failure : ECityOperationFailure::None;
}

FCityServiceSnapshot FCityServiceEvaluator::BuildSnapshot(
    const FCityPlacementModel& City, const FCityEconomyTuning& Tuning, const UCityBuildingCatalog* Catalog,
    const FCityDevelopmentSnapshot* Development, const FCityEnvironmentSnapshot* Environment,
    const FCityPopulationSnapshot* Population, const FCityFireSnapshot* Fire, const FCityTerrainHeightfield* InTerrain,
    const int32 InWaterRulesVersion, FCityServiceEvaluationTiming* Timing, const bool bPrepareQueries)
{
	double PhaseStart = Timing ? FPlatformTime::Seconds() : 0;
	const FCityTerrainHeightfield* Terrain = InTerrain ? InTerrain : City.GetTerrain();
	const int32 WaterRulesVersion = InWaterRulesVersion >= 0 ? InWaterRulesVersion
	                                : Terrain                ? Terrain->GetParameters().WaterRulesVersion
	                                                         : 0;
	TSet<uint64> OpenBusinesses;
	if (Population)
		for (const FCityBusinessLedgerRecord& Business : Population->Businesses)
			if (Business.bInitialFreightReceived)
				OpenBusinesses.Add(Business.EntityId);
	FCityServiceSnapshot Snapshot;
	Snapshot.Dimensions = City.GetGrid().GetDimensions();
	Snapshot.PlacementRevision = City.GetRevision();
	checkf(!Terrain || Terrain->GetDimensions() == Snapshot.Dimensions,
	       TEXT("Service terrain must belong to this city"));
	Snapshot.Water.RulesVersion = WaterRulesVersion;
	Snapshot.Water.bPrepareQueries = bPrepareQueries;
	Snapshot.Water.EvaluationTiming = Timing;
	Snapshot.bUsesPublicStaffing = Population != nullptr;
	TArray<uint64> EntityIds;
	City.GetEntities().GetKeys(EntityIds);
	EntityIds.Sort();
	for (const uint64 EntityId : EntityIds)
	{
		Snapshot.Water.EntityDemand.Add(EntityId, 0);
		Snapshot.Water.EntityDelivered.Add(EntityId, 0);
		Snapshot.Water.EntityPressurePercent.Add(EntityId, 0);
		Snapshot.Water.EntityCauses.Add(EntityId, ECityWaterFailure::None);
	}

	for (const uint64 EntityId : EntityIds)
	{
		const FCityPlacedEntity& Entity = City.GetEntities().FindChecked(EntityId);
		if (Fire && Fire->IsDisabled(EntityId))
		{
			const auto* Damage = Fire->DamagedBuildings.Find(EntityId);
			Snapshot.EntityFailures.Add(EntityId, Fire->IsBurning(EntityId) ? ECityOperationFailure::OnFire
			                                      : Damage && Damage->RepairCompleteStep >= 0
			                                          ? ECityOperationFailure::Repairing
			                                          : ECityOperationFailure::FireDamage);
			Snapshot.bHasDisabledEntities = true;
		}
		const auto* UtilityDefinition = Catalog ? Catalog->FindDefinition(Entity.DefinitionId) : nullptr;
		const auto* UtilityState = Development ? Development->Find(EntityId) : nullptr;
		const auto* UtilityLevel = UtilityDefinition && UtilityDefinition->bPlayerPurchasedLevels
		                               ? UtilityDefinition->FindDensityLevel(UtilityState ? UtilityState->DensityLevel
		                                                                                  : ECityDensityLevel::Low)
		                               : nullptr;
		const int32 SupplyPercent = UtilityLevel ? UtilityLevel->UtilitySupplyPercent : 100;
		const bool Alternative = FCityBuildingDefinition::IsAlternativeUtility(Entity.DefinitionId);
		const bool Utility = FCityBuildingDefinition::IsPowerFacility(Entity.DefinitionId) ||
		                     FCityBuildingDefinition::IsWaterFacility(Entity.DefinitionId) ||
		                     Entity.DefinitionId == Tuning.PowerDefinitionId ||
		                     Entity.DefinitionId == Tuning.WaterTreatmentDefinitionId;
		const int32 AlternativeOutput =
		    Alternative && UtilityDefinition
		        ? CityUtilityOutputPercent(
		              *UtilityDefinition, CityUtilityStaffPercent(Population, EntityId, UtilityDefinition->JobsProvided,
		                                                          UtilityDefinition->MinimumStaffEducation)) *
		              CityUtilityWeatherPercent(Entity.DefinitionId, Environment) / 100
		        : 0;
		if (Alternative && UtilityDefinition)
			Snapshot.UtilityUpgradeUpkeep += UtilityDefinition->MonthlyUpkeep;
		if (Alternative && UtilityDefinition && AlternativeOutput > 0 &&
		    (!Development || Development->IsOperational(EntityId)) && (!Fire || !Fire->IsDisabled(EntityId)) &&
		    City.HasRoadAccess(EntityId))
			Snapshot.EntityPollutionEmissions.Add(EntityId, UtilityDefinition->PollutionEmission);
		if (Utility && UtilityLevel && UtilityState && UtilityState->DensityLevel != ECityDensityLevel::Low)
			Snapshot.UtilityUpgradeUpkeep +=
			    FMath::Max<int64>(0, UtilityLevel->MonthlyUpkeep - (Entity.DefinitionId == Tuning.PowerDefinitionId
			                                                            ? Tuning.PowerUpkeep
			                                                            : Tuning.WaterTreatmentUpkeep));
		if (Utility && UtilityLevel && (!Fire || !Fire->IsDisabled(EntityId)) && City.HasRoadAccess(EntityId))
			Snapshot.EntityPollutionEmissions.Add(EntityId, UtilityLevel->PollutionEmission);
		const auto Park = FCityParkEvaluator::Policy(Entity.DefinitionId, Tuning, Catalog);
		if (Park.bIsPark)
		{
			++Snapshot.BuiltParks;
			Snapshot.ParkUpkeep += Park.Upkeep;
			continue;
		}
		if (FCityBuildingDefinition::IsPowerFacility(Entity.DefinitionId) ||
		    Entity.DefinitionId == Tuning.PowerDefinitionId)
		{
			++Snapshot.BuiltPowerBuildings;
			if (!Alternative)
				++Snapshot.LegacyPowerBuildings;
			if (Fire && Fire->IsDisabled(EntityId))
				continue;
			const ECityOperationFailure Failure =
			    City.HasRoadAccess(EntityId) ? (Alternative && Development && !Development->IsOperational(EntityId)
			                                        ? ECityOperationFailure::NoStaff
			                                        : ECityOperationFailure::None)
			                                 : ECityOperationFailure::NoRoad;
			Snapshot.EntityFailures.Add(EntityId, Failure);
			Snapshot.bHasDisabledEntities |= Failure != ECityOperationFailure::None;
			if (Failure == ECityOperationFailure::None)
			{
				if (!Alternative || AlternativeOutput > 0)
					++Snapshot.OperationalPowerBuildings;
				Snapshot.PowerCapacity = static_cast<int32>(FMath::Min<int64>(
				    MAX_int32,
				    static_cast<int64>(Snapshot.PowerCapacity) +
				        (Alternative && UtilityDefinition
				             ? static_cast<int64>(UtilityDefinition->UtilityCapacity) * AlternativeOutput / 100
				             : static_cast<int64>(Tuning.PowerCapacityPerBuilding) * SupplyPercent / 100 *
				                   Tuning.GetUtilityOutputPercent(
				                       Population ? Population->StaffPercent(EntityId, UtilityLevel
				                                                                           ? UtilityLevel->JobsProvided
				                                                                           : Tuning.UtilityStaff)
				                                  : 100) /
				                   100)));
			}
		}
		else if (FCityBuildingDefinition::IsWaterFacility(Entity.DefinitionId) ||
		         Entity.DefinitionId == Tuning.WaterTreatmentDefinitionId)
		{
			++Snapshot.BuiltServiceBuildings;
			if (!Alternative)
				++Snapshot.LegacyServiceBuildings;
			FCityWaterSource Source;
			Source.EntityId = EntityId;
			Source.SourcePressurePercent = Entity.DefinitionId == TEXT("WaterTower") ? 120 : 100;
			Source.YieldPercent =
			    WaterRulesVersion > 0 && Terrain ? Terrain->GetGroundwaterYieldPercent(Entity.Anchor) : 100;
			Source.StaffPercent = UtilityDefinition && (Alternative || WaterRulesVersion > 0)
			                          ? CityUtilityStaffPercent(Population, EntityId,
			                                                    UtilityLevel ? UtilityLevel->JobsProvided
			                                                                 : UtilityDefinition->JobsProvided,
			                                                    UtilityDefinition->MinimumStaffEducation)
			                      : Population
			                          ? Population->StaffPercent(EntityId, UtilityLevel ? UtilityLevel->JobsProvided
			                                                                            : Tuning.UtilityStaff)
			                          : 100;
			ECityOperationFailure Failure =
			    Fire && Fire->IsDisabled(EntityId) ? Snapshot.GetFailure(EntityId)
			    : !City.HasRoadAccess(EntityId)    ? ECityOperationFailure::NoRoad
			    : Development && !Development->IsOperational(EntityId) && (WaterRulesVersion > 0 || Alternative)
			        ? ECityOperationFailure::NoStaff
			        : ECityOperationFailure::None;
			if (Failure == ECityOperationFailure::None)
			{
				Source.Capacity = static_cast<int32>(FMath::Min<int64>(
				    MAX_int32, (Alternative && UtilityDefinition
				                    ? static_cast<int64>(UtilityDefinition->UtilityCapacity) * AlternativeOutput / 100
				                    : static_cast<int64>(Tuning.WaterCapacityPerTreatment) * SupplyPercent / 100 *
				                          Tuning.GetUtilityOutputPercent(Source.StaffPercent) / 100) *
				                   Source.YieldPercent / 100));
				Source.bOperational = (!Alternative || AlternativeOutput > 0) && Source.Capacity > 0;
				if (Source.bOperational)
				{
					++Snapshot.OperationalServiceBuildings;
					Snapshot.OperationalServiceEntityIds.Add(EntityId);
				}
				AddClamped(Snapshot.ServiceCapacity, Source.Capacity);
			}
			Snapshot.EntityFailures.Add(EntityId, Failure);
			Snapshot.bHasDisabledEntities |= Failure != ECityOperationFailure::None;
			Snapshot.Water.Sources.Add(MoveTemp(Source));
		}
	}
	if (WaterRulesVersion > 0)
	{
		Snapshot.ServiceCapacity = 0;
		Snapshot.OperationalServiceBuildings = 0;
		Snapshot.OperationalServiceEntityIds.Reset();
		for (auto& Source : Snapshot.Water.Sources)
		{
			const auto& Entity = City.GetEntities().FindChecked(Source.EntityId);
			const auto* Definition = Catalog ? Catalog->FindDefinition(Entity.DefinitionId) : nullptr;
			const auto* State = Development ? Development->Find(Source.EntityId) : nullptr;
			const auto* Level = Definition && Definition->bPlayerPurchasedLevels
			                        ? Definition->FindDensityLevel(State ? State->DensityLevel : ECityDensityLevel::Low)
			                        : nullptr;
			const int32 PowerDemand = FMath::Max(0, Level        ? Level->PowerDemand
			                                        : Definition ? Definition->PowerDemand
			                                                     : 0);
			if (Source.bOperational)
			{
				AddClamped(Snapshot.PowerDemand, PowerDemand);
				if (Snapshot.GetAvailablePower() < PowerDemand)
				{
					Source.bOperational = false;
					Source.Capacity = 0;
					Snapshot.EntityFailures.Add(Source.EntityId, Snapshot.PowerCapacity <= 0
					                                                 ? ECityOperationFailure::NoPower
					                                                 : ECityOperationFailure::PowerCapacity);
					Snapshot.bHasDisabledEntities = true;
				}
				else
					AddClamped(Snapshot.PowerUsed, PowerDemand);
			}
			if (Source.bOperational)
			{
				++Snapshot.OperationalServiceBuildings;
				Snapshot.OperationalServiceEntityIds.Add(Source.EntityId);
				AddClamped(Snapshot.ServiceCapacity, Source.Capacity);
			}
		}
	}
	if (Timing)
	{
		Timing->Providers.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	FCityWaterEvaluator::BuildNetwork(City, Snapshot.Water, Tuning.WaterRangeRoadHops);
	for (const auto& Source : Snapshot.Water.Sources)
		Snapshot.Water.EntityPressurePercent.Add(Source.EntityId,
		                                         Source.bOperational ? Source.SourcePressurePercent : 0);
	if (WaterRulesVersion == 0)
		BuildServiceReach(City, Snapshot.OperationalServiceEntityIds, Tuning.WaterRangeRoadHops, true,
		                  Snapshot.ServiceReach);
	else
		for (const auto& Pair : Snapshot.Water.RoadPressurePercent)
			if (Pair.Value >= FCityWaterEvaluator::MinimumUsablePressurePercent)
				Snapshot.ServiceReach.RoadHopDistances.Add(Pair.Key, 0);

	// Reserve emergency utilities first; IDs still break ties deterministically.
	if (Timing)
	{
		Timing->WaterNetwork.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	TArray<uint64> Consumers = EntityIds;
	Consumers.Sort(
	    [&](uint64 A, uint64 B)
	    {
		    auto Priority = [&](uint64 Id)
		    {
			    const FName D = City.GetEntities().FindChecked(Id).DefinitionId;
			    return D == TEXT("Fire") || D == TEXT("Police") || D == TEXT("Hospital") ? 0
			           : FCityParkEvaluator::Policy(D, Tuning, Catalog).bIsPark          ? 2
			                                                                             : 1;
		    };
		    return Priority(A) == Priority(B) ? A < B : Priority(A) < Priority(B);
	    });
	for (const uint64 EntityId : Consumers)
	{
		const FCityPlacedEntity& Entity = City.GetEntities().FindChecked(EntityId);
		const bool bHome = Entity.DefinitionId == Tuning.HomeDefinitionId;
		const bool bShop = Entity.DefinitionId == Tuning.ShopDefinitionId;
		const bool bIndustry = Entity.DefinitionId == Tuning.IndustrialDefinitionId;
		const bool bPark = FCityParkEvaluator::Policy(Entity.DefinitionId, Tuning, Catalog).bIsPark;
		const bool bPolice = Entity.DefinitionId == TEXT("Police") || Entity.DefinitionId == TEXT("Fire") ||
		                     Entity.DefinitionId == TEXT("Hospital") || Entity.DefinitionId == TEXT("WasteDepot") ||
		                     Entity.DefinitionId == TEXT("Recycling") ||
		                     FCityBuildingDefinition::IsSchoolFacility(Entity.DefinitionId);
		if (!bHome && !bShop && !bIndustry && !bPolice && !bPark)
		{
			continue;
		}
		Snapshot.BuiltHomes += bHome ? 1 : 0;
		Snapshot.BuiltShops += bShop ? 1 : 0;
		Snapshot.BuiltIndustries += bIndustry ? 1 : 0;
		const FCityDevelopmentState* DevelopmentState = Development ? Development->Find(EntityId) : nullptr;
		const FCityBuildingDefinition* CatalogDefinition =
		    Catalog ? Catalog->FindDefinition(Entity.DefinitionId) : nullptr;
		const FCityBuildingLevelDefinition* Level =
		    CatalogDefinition && DevelopmentState ? CatalogDefinition->FindDensityLevel(DevelopmentState->DensityLevel)
		                                          : nullptr;
		if (bPolice && CatalogDefinition)
			Snapshot.CivicUpkeep += Level ? Level->MonthlyUpkeep : CatalogDefinition->MonthlyUpkeep;
		if (CatalogDefinition && CatalogDefinition->bPlayerPurchasedLevels && Level)
		{
			Snapshot.StationRoadRanges.Add(EntityId, Level->CivicRoadRange);
			Snapshot.StationProtectionCapacities.Add(EntityId, Level->CivicProtectionCapacity);
			Snapshot.StationFleetCapacities.Add(EntityId, Level->CivicResponseVehicles);
		}
		const int32 ResidentsProvided = Level ? Level->ResidentsProvided : Tuning.ResidentsPerOccupiedHome;
		const int32 JobsProvided = bPolice ? 0
		                           : Level ? Level->JobsProvided
		                                   : (bShop       ? Tuning.JobsPerOperationalShop
		                                      : bIndustry ? Tuning.JobsPerOperationalIndustry
		                                                  : 0);
		const int32 EconomicOutput = Level ? Level->EconomicOutput
		                                   : (bShop       ? Tuning.JobsPerOperationalShop
		                                      : bIndustry ? Tuning.OutputPerOperationalIndustry
		                                                  : 0);
		Snapshot.BuiltHousingCapacity += bHome ? ResidentsProvided : 0;
		AddClamped(Snapshot.BuiltJobs, JobsProvided);
		if (DevelopmentState && DevelopmentState->Stage == ECityDevelopmentStage::Construction)
		{
			continue;
		}
		if (Fire && Fire->IsDisabled(EntityId))
			continue;
		if (Population && !bHome && !bPolice && !bPark && !OpenBusinesses.Contains(EntityId))
		{
			Snapshot.EntityFailures.Add(EntityId, City.HasRoadAccess(EntityId) ? ECityOperationFailure::AwaitingFreight
			                                                                   : ECityOperationFailure::NoRoad);
			Snapshot.bHasDisabledEntities = true;
			continue;
		}
		const int32 PowerDemand = Level                                     ? Level->PowerDemand
		                          : (bPolice || bPark) && CatalogDefinition ? CatalogDefinition->PowerDemand
		                          : bPark
		                              ? 0
		                              : (bIndustry ? Tuning.IndustrialPowerDemand : Tuning.UtilityDemandPerBuilding);
		AddClamped(Snapshot.PowerDemand, PowerDemand);
		const int32 BaseWaterDemand = Level                                     ? Level->WaterDemand
		                              : (bPolice || bPark) && CatalogDefinition ? CatalogDefinition->WaterDemand
		                              : bPark                                   ? 0
		                                      : WaterDemandForDefinition(Tuning, Entity.DefinitionId);
		const int32 WaterDemand = FMath::Max(
		    0, (BaseWaterDemand * (Environment ? Environment->GetWaterDemandModifierPercent() : 100) + 99) / 100);
		AddClamped(Snapshot.ServiceDemand, WaterDemand);

		int32 Delivered = 0;
		if (WaterRulesVersion > 0)
		{
			Delivered = FCityWaterEvaluator::Allocate(City, Snapshot.Water, EntityId, Entity.Anchor,
			                                          Entity.FootprintSize, WaterDemand);
			if (IsFootprintReached(City, Snapshot.ServiceReach, Entity.Anchor, Entity.FootprintSize))
				AddClamped(Snapshot.ServiceCoveredDemand, WaterDemand);
			AddClamped(Snapshot.ServiceUsed, Delivered);
		}
		ECityOperationFailure Failure = ECityOperationFailure::None;
		if (!City.HasRoadAccess(EntityId))
		{
			Failure = ECityOperationFailure::NoRoad;
		}
		else if (WaterRulesVersion > 0 && Delivered < WaterDemand)
		{
			Failure = Snapshot.Water.EntityCauses.FindRef(EntityId) == ECityWaterFailure::LowPressure
			              ? ECityOperationFailure::LowWaterPressure
			          : Snapshot.Water.EntityCauses.FindRef(EntityId) == ECityWaterFailure::SourceCapacity
			              ? ECityOperationFailure::ServiceCapacity
			              : ECityOperationFailure::NoService;
		}
		else if (WaterRulesVersion == 0 && (!bPark || WaterDemand > 0) &&
		         !IsFootprintReached(City, Snapshot.ServiceReach, Entity.Anchor, Entity.FootprintSize))
		{
			Failure = ECityOperationFailure::NoService;
		}
		else if (WaterRulesVersion == 0 && Snapshot.GetAvailableService() < WaterDemand)
		{
			if (WaterRulesVersion == 0)
				AddClamped(Snapshot.ServiceCoveredDemand, WaterDemand);
			Failure = ECityOperationFailure::ServiceCapacity;
		}
		else if ((!bPark || PowerDemand > 0) && Snapshot.PowerCapacity <= 0)
		{
			if (WaterRulesVersion == 0)
				AddClamped(Snapshot.ServiceCoveredDemand, WaterDemand);
			if (WaterRulesVersion == 0)
				AddClamped(Snapshot.ServiceUsed, WaterDemand);
			Failure = ECityOperationFailure::NoPower;
		}
		else if (Snapshot.GetAvailablePower() < PowerDemand)
		{
			if (WaterRulesVersion == 0)
				AddClamped(Snapshot.ServiceCoveredDemand, WaterDemand);
			if (WaterRulesVersion == 0)
				AddClamped(Snapshot.ServiceUsed, WaterDemand);
			Failure = ECityOperationFailure::PowerCapacity;
		}
		else
		{
			if (WaterRulesVersion == 0)
				AddClamped(Snapshot.ServiceCoveredDemand, WaterDemand);
			if (WaterRulesVersion == 0)
				AddClamped(Snapshot.ServiceUsed, WaterDemand);
			AddClamped(Snapshot.PowerUsed, PowerDemand);
			Snapshot.OperationalHomes += bHome ? 1 : 0;
			Snapshot.OperationalShops += bShop ? 1 : 0;
			Snapshot.OperationalIndustries += bIndustry ? 1 : 0;
			Snapshot.EntityResidents.Add(EntityId, bHome ? ResidentsProvided : 0);
			Snapshot.EntityJobs.Add(EntityId, JobsProvided);
			if ((bShop || bIndustry) && Level && Level->Level != ECityDensityLevel::Low)
				Snapshot.EntitySkilledJobs.Add(EntityId, JobsProvided / 4);
			Snapshot.EntityOutput.Add(EntityId, bHome ? 0 : EconomicOutput);
			Snapshot.Residents += bHome ? ResidentsProvided : 0;
			Snapshot.HousingCapacity += bHome ? ResidentsProvided : 0;
			Snapshot.ResidentialUpkeep += bHome ? (Level ? Level->MonthlyUpkeep : Tuning.HomeUpkeep) : 0;
			Snapshot.CommercialUpkeep += bShop ? (Level ? Level->MonthlyUpkeep : Tuning.ShopUpkeep) : 0;
			Snapshot.IndustrialUpkeep += bIndustry ? (Level ? Level->MonthlyUpkeep : Tuning.IndustrialUpkeep) : 0;
			AddClamped(Snapshot.Jobs, JobsProvided);
			AddClamped(Snapshot.CommercialJobs, bShop ? JobsProvided : 0);
			AddClamped(Snapshot.CommercialOutput, bShop ? EconomicOutput : 0);
			AddClamped(Snapshot.IndustrialJobs, bIndustry ? JobsProvided : 0);
			AddClamped(Snapshot.IndustrialOutput, bIndustry ? EconomicOutput : 0);
			if (bIndustry)
			{
				Snapshot.EntityPollutionEmissions.Add(EntityId, Level ? Level->PollutionEmission
				                                                      : Tuning.PollutionEmissionPerIndustrialBuilding);
			}
		}
		if (WaterRulesVersion == 0)
		{
			const bool Reached = IsFootprintReached(City, Snapshot.ServiceReach, Entity.Anchor, Entity.FootprintSize);
			Snapshot.Water.EntityDemand.Add(EntityId, WaterDemand);
			Snapshot.Water.EntityDelivered.Add(EntityId, Failure == ECityOperationFailure::NoRoad ||
			                                                     Failure == ECityOperationFailure::NoService ||
			                                                     Failure == ECityOperationFailure::ServiceCapacity
			                                                 ? 0
			                                                 : WaterDemand);
			Snapshot.Water.EntityPressurePercent.Add(EntityId, Reached ? 100 : 0);
			Snapshot.Water.EntityCauses.Add(EntityId, WaterDemand == 0 ? ECityWaterFailure::None
			                                          : !Reached       ? ECityWaterFailure::Disconnected
			                                          : Failure == ECityOperationFailure::ServiceCapacity
			                                              ? ECityWaterFailure::SourceCapacity
			                                              : ECityWaterFailure::None);
		}
		Snapshot.EntityFailures.Add(EntityId, Failure);
		Snapshot.bHasDisabledEntities |= !bPark && Failure != ECityOperationFailure::None;
	}

	if (WaterRulesVersion == 0)
	{
		// Legacy cities intentionally retain their old pooled capacity; source usage is diagnostic only.
		int32 Remaining = Snapshot.ServiceUsed;
		for (auto& Source : Snapshot.Water.Sources)
		{
			Source.Used = FMath::Min(Remaining, Source.Capacity);
			Remaining -= Source.Used;
		}
	}
	if (Timing)
	{
		Timing->Consumers.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	if (Population)
	{
		for (const uint64 Id : EntityIds)
		{
			const auto& E = City.GetEntities().FindChecked(Id);
			if (FCityTechnology::IsFacility(E.DefinitionId))
				continue; // Technology owns its vacancies, utilities and prerequisite evaluation below.
			const auto* D = Catalog ? Catalog->FindDefinition(E.DefinitionId) : nullptr;
			const auto* S = Development ? Development->Find(Id) : nullptr;
			const auto* L = D && D->bPlayerPurchasedLevels
			                    ? D->FindDensityLevel(S ? S->DensityLevel : ECityDensityLevel::Low)
			                    : nullptr;

			const bool Specialized = FCityBuildingDefinition::IsAlternativeUtility(E.DefinitionId) ||
			                         E.DefinitionId == TEXT("College") || E.DefinitionId == TEXT("Hospital") ||
			                         E.DefinitionId == TEXT("WasteDepot") || E.DefinitionId == TEXT("Recycling");
			const int32 Jobs = L                  ? L->JobsProvided
			                   : Specialized && D ? D->JobsProvided
			                                      : FCityPopulationSimulation::PublicJobs(E.DefinitionId, Tuning);
			Snapshot.PublicMinimumEducation.Add(Id, D ? D->MinimumStaffEducation : 1);
			if (!Jobs)
				continue;
			const bool bReady = (!Development || Development->IsOperational(Id)) &&
			                    Snapshot.GetFailure(Id) == ECityOperationFailure::None;
			Snapshot.PublicJobs.Add(Id, bReady ? Jobs : 0);
			Snapshot.PublicStaffPercent.Add(
			    Id, bReady ? (Specialized && D ? CityUtilityStaffPercent(Population, Id, Jobs, D->MinimumStaffEducation)
			                                   : Population->StaffPercent(Id, Jobs))
			               : 0);
			if ((E.DefinitionId == TEXT("WasteDepot") || E.DefinitionId == TEXT("Recycling")) && D && bReady)
				Snapshot.EntityPollutionEmissions.Add(Id, D->PollutionEmission *
				                                              Snapshot.PublicStaffPercent.FindRef(Id) / 100);
			if (bReady &&
			    (E.DefinitionId == TEXT("Police") || E.DefinitionId == TEXT("Fire") ||
			     E.DefinitionId == TEXT("Hospital") || E.DefinitionId == TEXT("WasteDepot") ||
			     E.DefinitionId == TEXT("Recycling") || FCityBuildingDefinition::IsSchoolFacility(E.DefinitionId)) &&
			    Population->StaffPercent(Id) == 0)
			{
				Snapshot.EntityFailures.Add(Id, ECityOperationFailure::NoStaff);
				Snapshot.bHasDisabledEntities = true;
			}
			Snapshot.EntityJobs.Add(Id, bReady ? Jobs : 0);
			AddClamped(Snapshot.Jobs, bReady ? Jobs : 0);
			AddClamped(Snapshot.BuiltJobs, Jobs);
			if (Specialized && D && FCityBuildingDefinition::IsAlternativeUtility(E.DefinitionId) &&
			    D->UtilityMinimumOutputPercent == 0 && Snapshot.PublicStaffPercent.FindRef(Id) == 0 && bReady)
			{
				Snapshot.EntityFailures.Add(Id, ECityOperationFailure::NoStaff);
				Snapshot.bHasDisabledEntities = true;
			}
		}
		// Predict the same bounded admissions used by population reconciliation;
		// held/reserved housing cannot manufacture replacement residents or taxes.
		Snapshot.Residents = FCityHousingMigration::AdmissionPlan(*Population, Snapshot).TargetResidents;
		Snapshot.UnemployedResidents = 0;
		for (const auto& Citizen : Population->Citizens)
		{
			Snapshot.HomelessResidents += Citizen.IsHomeless() ? 1 : 0;
			Snapshot.DetainedResidents += Citizen.IsDetained() ? 1 : 0;
			Snapshot.UnemployedResidents += !Citizen.IsDetained() && Citizen.WorkplaceEntityId == 0 ? 1 : 0;
		}
	}
	for (const uint64 Id : EntityIds)
	{
		const auto& Entity = City.GetEntities().FindChecked(Id);
		if (Entity.DefinitionId != TEXT("Hospital"))
			continue;
		const auto* Definition = Catalog ? Catalog->FindDefinition(Entity.DefinitionId) : nullptr;
		const auto* State = Development ? Development->Find(Id) : nullptr;
		const auto* Level =
		    Definition ? Definition->FindDensityLevel(State ? State->DensityLevel : ECityDensityLevel::Low) : nullptr;
		const bool Ready = Level && (!Development || Development->IsOperational(Id)) &&
		                   Snapshot.GetFailure(Id) == ECityOperationFailure::None;
		const int32 Staff = Ready ? (Population ? Snapshot.PublicStaffPercent.FindRef(Id) : 100) : 0;
		Snapshot.HospitalCapacities.Add(Id, Level ? Level->CivicProtectionCapacity * Staff / 100 : 0);
		Snapshot.HospitalFleetCapacities.Add(
		    Id, Level && Staff > 0 ? FMath::Max(1, Level->CivicResponseVehicles * Staff / 100) : 0);
	}
	if (Timing)
	{
		Timing->PublicStaff.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	FCityEducationEvaluator::Build(City, Snapshot, Tuning, Population, Development, bPrepareQueries, Timing);
	if (Timing)
	{
		Timing->Education.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	FCityTechnology::Evaluate(City, Snapshot, Catalog, Population, Development, Environment);
	if (Population)
		Snapshot.Residents = FCityHousingMigration::AdmissionPlan(*Population, Snapshot).TargetResidents;
	FCityParkEvaluator::Build(City, Snapshot, Tuning, Catalog, Development);
	if (Timing)
		Timing->Parks.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
	return Snapshot;
}

ECityOperationFailure FCityServiceEvaluator::EvaluateCandidate(const FCityPlacementModel& City,
                                                               const FCityServiceSnapshot& Services,
                                                               const FCityEconomyTuning& Tuning,
                                                               const FName DefinitionId, const FCityCellCoord& Anchor,
                                                               const FIntPoint& FootprintSize,
                                                               const UCityBuildingCatalog* Catalog)
{
	const bool bUtility = FCityBuildingDefinition::IsPowerFacility(DefinitionId) ||
	                      FCityBuildingDefinition::IsWaterFacility(DefinitionId) ||
	                      DefinitionId == Tuning.PowerDefinitionId || DefinitionId == Tuning.WaterTreatmentDefinitionId;
	const bool bDemand = DefinitionId == Tuning.HomeDefinitionId || DefinitionId == Tuning.ShopDefinitionId ||
	                     DefinitionId == Tuning.IndustrialDefinitionId;
	const bool bPark = FCityParkEvaluator::Policy(DefinitionId, Tuning, Catalog).bIsPark;
	const bool bTechnology = FCityTechnology::IsFacility(DefinitionId);
	const FCityBuildingDefinition* Definition = Catalog ? Catalog->FindDefinition(DefinitionId) : nullptr;
	if (Definition && !Definition->PrerequisitesMet(&Services))
		return ECityOperationFailure::TechnologyPrerequisite;
	if (!bUtility && !bDemand && !bPark && !bTechnology)
	{
		return ECityOperationFailure::None;
	}
	if (!City.HasConnectedRoadAccess(Anchor, FootprintSize))
	{
		return ECityOperationFailure::NoRoad;
	}
	if (bUtility)
	{
		return ECityOperationFailure::None;
	}
	const int32 WaterDemand = bPark || bTechnology ? (Definition ? Definition->WaterDemand : 0)
	                                               : WaterDemandForDefinition(Tuning, DefinitionId);
	if (Services.Water.RulesVersion == 0 && (!bPark || WaterDemand > 0) &&
	    !IsFootprintReached(City, Services.ServiceReach, Anchor, FootprintSize))
	{
		return ECityOperationFailure::NoService;
	}
	if (Services.Water.RulesVersion > 0)
	{
		const auto Cause = FCityWaterEvaluator::EvaluateAt(City, Services.Water, Anchor, FootprintSize, WaterDemand);
		if (Cause != ECityWaterFailure::None)
			return Cause == ECityWaterFailure::SourceCapacity ? ECityOperationFailure::ServiceCapacity
			       : Cause == ECityWaterFailure::LowPressure  ? ECityOperationFailure::LowWaterPressure
			                                                  : ECityOperationFailure::NoService;
	}
	if (Services.Water.RulesVersion == 0 && Services.GetAvailableService() < WaterDemand)
	{
		return ECityOperationFailure::ServiceCapacity;
	}
	const int32 PowerDemand = bPark || bTechnology ? (Definition ? Definition->PowerDemand : 0)
	                                               : (DefinitionId == Tuning.IndustrialDefinitionId
	                                                      ? Tuning.IndustrialPowerDemand
	                                                      : Tuning.UtilityDemandPerBuilding);
	if ((!bPark || PowerDemand > 0) && Services.PowerCapacity <= 0)
	{
		return ECityOperationFailure::NoPower;
	}
	return Services.GetAvailablePower() >= PowerDemand ? ECityOperationFailure::None
	                                                   : ECityOperationFailure::PowerCapacity;
}

FString FCityServiceEvaluator::FailureToString(const ECityOperationFailure Failure)
{
	switch (Failure)
	{
	case ECityOperationFailure::OnFire:
		return TEXT("Disabled: building on fire");
	case ECityOperationFailure::FireDamage:
		return TEXT("Disabled: structural damage; select REPAIR");
	case ECityOperationFailure::Repairing:
		return TEXT("Disabled: repair underway");
	case ECityOperationFailure::TechnologyPrerequisite:
		return TEXT("Technology prerequisites offline");
	case ECityOperationFailure::NoStaff:
		return TEXT("Disabled: needs staff and paid public wages");
	case ECityOperationFailure::None:
		return TEXT("Operational");
	case ECityOperationFailure::AwaitingFreight:
		return TEXT("Awaiting opening delivery");
	case ECityOperationFailure::NoRoad:
		return TEXT("Disabled: no connected road");
	case ECityOperationFailure::NoPower:
		return TEXT("Disabled: no power");
	case ECityOperationFailure::PowerCapacity:
		return TEXT("Disabled: power capacity exhausted");
	case ECityOperationFailure::NoService:
		return TEXT("Disabled: no usable water source/pipe path");
	case ECityOperationFailure::LowWaterPressure:
		return TEXT("Disabled: low water pressure; source too far by road");
	case ECityOperationFailure::ServiceCapacity:
		return TEXT("Disabled: water capacity exhausted");
	default:
		return TEXT("Disabled");
	}
}

FCityGrowthResult FCityGrowthSimulation::AdvanceToStep(
    const int64 FixedStepId, FCityPlacementModel& City, const FCityServiceSnapshot& Services,
    const FCityNeedsSnapshot& Needs, const FCityEconomyTuning& Tuning, const FCityBuildingDefinition* HomeDefinition,
    const FCityBuildingDefinition* ShopDefinition, const bool bGrowthBlockedByShortfall,
    const FCityBuildingDefinition* IndustryDefinition, const FCityEnvironmentSnapshot* Environment)
{
	FCityGrowthResult Result;
	const int64 Interval = FMath::Max(1, Tuning.GrowthIntervalSteps);
	if (NextGrowthStep <= 0)
	{
		NextGrowthStep = Interval;
	}
	if (FixedStepId < NextGrowthStep)
	{
		return Result;
	}
	while (NextGrowthStep <= FixedStepId)
	{
		NextGrowthStep += Interval;
	}
	if (bGrowthBlockedByShortfall)
	{
		Result.Failure = ECityGrowthFailure::UpkeepShortfall;
		return Result;
	}
	// Local damage, outages and opening freight must not freeze unrelated serviced
	// zones. EvaluateCandidate below uses the current operational providers and
	// remaining capacity, matching the lot checks shown in City Needs.

	Result.Zone = Needs.RecommendedGrowthZone;
	if (Result.Zone == ECityZoneType::None)
	{
		Result.Failure = ECityGrowthFailure::LowDemand;
		return Result;
	}
	const FCityBuildingDefinition* Definition = Result.Zone == ECityZoneType::Residential  ? HomeDefinition
	                                            : Result.Zone == ECityZoneType::Commercial ? ShopDefinition
	                                            : Result.Zone == ECityZoneType::Industrial ? IndustryDefinition
	                                                                                       : nullptr;
	if (!Definition)
	{
		Result.Failure = ECityGrowthFailure::MissingDefinition;
		return Result;
	}

	TArray<FCityCellCoord> Candidates;
	bool bHadUniformZone = false;
	const FIntPoint GrowthSize(2, 2);
	for (int32 Y = 0; Y <= City.GetGrid().GetDimensions().GetHeight() - GrowthSize.Y; ++Y)
	{
		for (int32 X = 0; X <= City.GetGrid().GetDimensions().GetWidth() - GrowthSize.X; ++X)
		{
			const FCityCellCoord Anchor(X, Y);
			if (!HasUniformEmptyZone(City, Anchor, Result.Zone))
			{
				continue;
			}
			bHadUniformZone = true;
			const ECityOperationFailure CandidateFailure = FCityServiceEvaluator::EvaluateCandidate(
			    City, Services, Tuning, Definition->DefinitionId, Anchor, GrowthSize);
			if (CandidateFailure != ECityOperationFailure::None)
			{
				if (Result.BlockingOperationFailure == ECityOperationFailure::None)
				{
					Result.BlockingOperationFailure = CandidateFailure;
					Result.Anchor = Anchor;
					Result.BlockingDefinitionId = Definition->DefinitionId;
				}
				continue;
			}
			if (Result.Zone == ECityZoneType::Residential && Environment &&
			    !FCityEnvironmentSimulation::EvaluateResidentialDesirability(City, Services, *Environment, Anchor,
			                                                                 GrowthSize, Tuning)
			         .IsResidentialEligible(Tuning.MinimumResidentialDesirability))
			{
				continue;
			}
			FCityBuildingDefinition GrowthDefinition = *Definition;
			GrowthDefinition.Footprint = FCityCellFootprint(2, 2);
			const FCityPlacementPreview Preview = City.BeginPreview(GrowthDefinition, Anchor);
			const ECityPlacementFailure PlacementFailure =
			    City.ValidatePlacement(Preview, GrowthDefinition, ECityConstructionFunding::PrivateDeveloper,
			                           &Services)
			        .Failure;
			if (PlacementFailure != ECityPlacementFailure::None)
			{
				continue;
			}
			Candidates.Add(Anchor);
		}
	}
	if (Candidates.IsEmpty())
	{
		Result.Failure = ECityGrowthFailure::NoEligibleZone;
		if (!bHadUniformZone)
		{
			Result.BlockingOperationFailure = ECityOperationFailure::None;
		}
		return Result;
	}

	Result.Anchor = Candidates[NextRandom() % Candidates.Num()];
	FCityBuildingDefinition GrowthDefinition = *Definition;
	GrowthDefinition.Footprint = FCityCellFootprint(2, 2);
	FCityPlacementPreview Preview = City.BeginRoadFacingPreview(GrowthDefinition, Result.Anchor);
	const FCityPlacementResult Placement = City.CommitPlacement(
	    Preview, GrowthDefinition, ECityConstructionFunding::PrivateDeveloper, &Services);
	if (!Placement.IsSuccess())
	{
		Result.Failure = ECityGrowthFailure::NoEligibleZone;
		return Result;
	}
	Result.Failure = ECityGrowthFailure::None;
	Result.EntityId = Placement.EntityId;
	return Result;
}

uint32 FCityGrowthSimulation::NextRandom()
{
	RandomState = RandomState == 0 ? 1u : RandomState;
	RandomState ^= RandomState << 13;
	RandomState ^= RandomState >> 17;
	RandomState ^= RandomState << 5;
	return RandomState;
}

bool FCityGrowthSimulation::RestoreState(const uint32 InRandomState, const int64 InNextGrowthStep, FString& OutError)
{
	if (InRandomState == 0 || InNextGrowthStep < 0)
	{
		OutError = TEXT("Invalid saved growth random state or schedule");
		return false;
	}
	RandomState = InRandomState;
	NextGrowthStep = InNextGrowthStep;
	return true;
}

FString FCityGrowthSimulation::FailureToString(const ECityGrowthFailure Failure)
{
	switch (Failure)
	{
	case ECityGrowthFailure::None:
		return TEXT("Growth placed");
	case ECityGrowthFailure::NotDue:
		return TEXT("Growth not due");
	case ECityGrowthFailure::UpkeepShortfall:
		return TEXT("Unpaid monthly bills: City > TAXES or reduce upkeep");
	case ECityGrowthFailure::DisabledBuilding:
		return TEXT("Growth blocked by a disabled building");
	case ECityGrowthFailure::MissingDefinition:
		return TEXT("Growth building definition missing");
	case ECityGrowthFailure::LowDemand:
		return TEXT("No demand for new low-density lots");
	case ECityGrowthFailure::NoEligibleZone:
		return TEXT("No eligible zoned 2x2 footprint");
	case ECityGrowthFailure::InsufficientFunds:
		return TEXT("Growth blocked by insufficient funds");
	default:
		return TEXT("Growth blocked");
	}
}

FString FCityGrowthSimulation::DescribeResult(const FCityGrowthResult& Result)
{
	if (Result.Failure == ECityGrowthFailure::DisabledBuilding &&
	    Result.BlockingOperationFailure != ECityOperationFailure::None)
	{
		const FString Building =
		    Result.BlockingDefinitionId.IsNone() ? TEXT("Building") : Result.BlockingDefinitionId.ToString();
		return FString::Printf(TEXT("Blocked: %s #%llu %s"), *Building, Result.BlockingEntityId,
		                       *OperationBlockerToString(Result.BlockingOperationFailure));
	}
	if (Result.Failure == ECityGrowthFailure::NoEligibleZone)
	{
		if (Result.BlockingOperationFailure != ECityOperationFailure::None)
		{
			return FString::Printf(TEXT("Blocked: %s zone %s"), ZoneToString(Result.Zone),
			                       *OperationBlockerToString(Result.BlockingOperationFailure));
		}
		return FString::Printf(TEXT("Needs empty 2x2 %s zone beside connected road"), ZoneToString(Result.Zone));
	}
	if (Result.Failure == ECityGrowthFailure::InsufficientFunds)
	{
		return FString::Printf(TEXT("Blocked: need funds to grow a %s building"), ZoneToString(Result.Zone));
	}
	return FailureToString(Result.Failure);
}

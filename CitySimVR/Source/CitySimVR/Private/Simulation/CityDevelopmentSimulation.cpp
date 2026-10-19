#include "Simulation/CityDevelopmentSimulation.h"

#include "Commands/CityPlacementModel.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityNeedsSimulation.h"

namespace
{
constexpr int32 InfillReservationLots = 2;

ECityDensityLevel NextDensity(const ECityDensityLevel Current)
{
	return Current == ECityDensityLevel::Low ? ECityDensityLevel::Medium : ECityDensityLevel::High;
}

int32 DemandBalanceFor(const ECityDevelopmentDemandType DemandType, const FCityNeedsSnapshot& Needs)
{
	switch (DemandType)
	{
	case ECityDevelopmentDemandType::Residential:
		return Needs.ResidentialDemandBalance;
	case ECityDevelopmentDemandType::Commercial:
		return Needs.CommercialDemandBalance;
	case ECityDevelopmentDemandType::Industrial:
		return Needs.IndustrialDemandBalance;
	default:
		return 0;
	}
}

int32 EligibleFootprintsFor(const ECityDevelopmentDemandType DemandType, const FCityNeedsSnapshot& Needs)
{
	switch (DemandType)
	{
	case ECityDevelopmentDemandType::Residential:
		return Needs.ResidentialEligibleFootprints;
	case ECityDevelopmentDemandType::Commercial:
		return Needs.CommercialEligibleFootprints;
	case ECityDevelopmentDemandType::Industrial:
		return Needs.IndustrialEligibleFootprints;
	default:
		return 0;
	}
}

int32 CapacityFor(const ECityDevelopmentDemandType DemandType, const FCityBuildingLevelDefinition& Level)
{
	switch (DemandType)
	{
	case ECityDevelopmentDemandType::Residential:
		return Level.ResidentsProvided;
	case ECityDevelopmentDemandType::Commercial:
		return Level.JobsProvided;
	case ECityDevelopmentDemandType::Industrial:
		return FMath::Max(Level.JobsProvided, Level.EconomicOutput);
	default:
		return 0;
	}
}

int32 AveragePollution(const FCityPlacedEntity& Entity, const FCityEnvironmentSnapshot& Environment)
{
	int64 Total = 0;
	int32 Cells = 0;
	for (int32 Y = Entity.Anchor.Y; Y < Entity.Anchor.Y + Entity.FootprintSize.Y; ++Y)
	{
		for (int32 X = Entity.Anchor.X; X < Entity.Anchor.X + Entity.FootprintSize.X; ++X)
		{
			Total += Environment.GetPollution(FCityCellCoord(X, Y));
			++Cells;
		}
	}
	return Cells > 0 ? static_cast<int32>(Total / Cells) : 0;
}

FCityDevelopmentPressure BuildPressure(const FCityPlacementModel& City, const FCityPlacedEntity& Entity,
                                       const FCityBuildingDefinition& Definition,
                                       const FCityServiceSnapshot& Services,
                                       const FCityEnvironmentSnapshot& Environment,
                                       const FCityEconomyTuning& Tuning, const int32 UpgradeDemand)
{
	FCityDevelopmentPressure Pressure;
	Pressure.CitywideDemand = FMath::Clamp(UpgradeDemand, 0, 100);
	TArray<FCityCellCoord> AdjacentRoads;
	FCityServiceEvaluator::GetAdjacentRoadCells(City, Entity.Anchor, Entity.FootprintSize, true,
	                                            AdjacentRoads);
	Pressure.RoadAccess = FMath::Min(30, AdjacentRoads.Num() * 10);
	const int32 Pollution = AveragePollution(Entity, Environment);
	if (Definition.DevelopmentDemandType == ECityDevelopmentDemandType::Residential)
	{
		Pressure.LandValue = FCityEnvironmentSimulation::EvaluateResidentialDesirability(
		                         City, Services, Environment, Entity.Anchor, Entity.FootprintSize, Tuning)
		                         .Score;
		Pressure.PollutionCompatibility = FMath::Clamp(20 - Pollution / 10, 0, 20);
	}
	else if (Definition.DevelopmentDemandType == ECityDevelopmentDemandType::Commercial)
	{
		Pressure.LandValue = FMath::Clamp(70 - Pollution / 10, 0, 100);
		Pressure.PollutionCompatibility = FMath::Clamp(20 - Pollution / 10, 0, 20);
	}
	else
	{
		int32 NearestHomeDistance = 20;
		for (const TPair<uint64, FCityPlacedEntity>& Pair : City.GetEntities())
		{
			if (Pair.Value.DefinitionId == Tuning.HomeDefinitionId)
			{
				NearestHomeDistance =
				    FMath::Min(NearestHomeDistance, FMath::Abs(Pair.Value.Anchor.X - Entity.Anchor.X) +
				                                        FMath::Abs(Pair.Value.Anchor.Y - Entity.Anchor.Y));
			}
		}
		Pressure.LandValue = FMath::Clamp(35 + NearestHomeDistance * 3, 0, 100);
		Pressure.PollutionCompatibility = FMath::Clamp(Pollution / 10, 0, 20);
	}
	const int32 PowerPercent =
	    Services.PowerCapacity > 0 ? 100 * Services.GetAvailablePower() / Services.PowerCapacity : 0;
	const int32 WaterPercent =
	    Services.ServiceCapacity > 0 ? 100 * Services.GetAvailableService() / Services.ServiceCapacity : 0;
	Pressure.InfrastructureHeadroom = FMath::Clamp(FMath::Min(PowerPercent, WaterPercent) / 5, 0, 20);
	Pressure.Total = Pressure.CitywideDemand + Pressure.RoadAccess + Pressure.LandValue +
	                 Pressure.PollutionCompatibility + Pressure.InfrastructureHeadroom;
	return Pressure;
}
} // namespace

bool FCityDevelopmentSnapshot::IsOperational(const uint64 EntityId) const
{
	const FCityDevelopmentState* State = Find(EntityId);
	return !State || State->Stage == ECityDevelopmentStage::Operational;
}

void FCityDevelopmentSimulation::Reset(const int64 FixedStepId)
{
	Snapshot = FCityDevelopmentSnapshot();
	Snapshot.LastFixedStepId = FMath::Max<int64>(0, FixedStepId);
}

void FCityDevelopmentSimulation::Reconcile(const FCityPlacementModel& City,
                                           const UCityBuildingCatalog* Catalog, const int64 CurrentHalfDay, const int32 StepsPerDay)
{
	for (auto It = Snapshot.States.CreateIterator(); It; ++It)
	{
		if (!City.GetEntities().Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
	if (!Catalog)
	{
		return;
	}
	for (const TPair<uint64, FCityPlacedEntity>& Pair : City.GetEntities())
	{
		const FCityBuildingDefinition* Definition = Catalog->FindDefinition(Pair.Value.DefinitionId);
		if (!Definition || (Definition->ConstructionSteps <= 0 && Definition->DensityLevels.IsEmpty()) ||
		    Snapshot.States.Contains(Pair.Key))
		{
			continue;
		}
		FCityDevelopmentState State;
		State.EntityId = Pair.Key;
		State.Stage = Definition->ConstructionSteps > 0 ? ECityDevelopmentStage::Construction
		                                                : ECityDevelopmentStage::Operational;
		State.DensityLevel = ECityDensityLevel::Low;
		State.ConstructionCompleteStep = Snapshot.LastFixedStepId + Definition->ConstructionSteps;
		if (Definition->bPlayerPurchasedLevels &&
		    (Pair.Value.DefinitionId == TEXT("Police") || Pair.Value.DefinitionId == TEXT("Fire")) &&
		    State.Stage == ECityDevelopmentStage::Construction)
		{
			State.StationCompleteHalfDay =
			    (CurrentHalfDay < 0 ? FCityCalendar::HalfDayIndex(Snapshot.LastFixedStepId, 0, StepsPerDay) : CurrentHalfDay) + 1;
			State.ConstructionCompleteStep = FCityCalendar::HalfDayEndStep(State.StationCompleteHalfDay, StepsPerDay);
		}
		State.OperationalSinceStep =
		    State.Stage == ECityDevelopmentStage::Operational ? Snapshot.LastFixedStepId : 0;
		Snapshot.States.Add(Pair.Key, State);
	}
}

void FCityDevelopmentSimulation::DelayConstructionUntil(const uint64 EntityId, const int64 StartStep)
{
	FCityDevelopmentState* State = Snapshot.States.Find(EntityId);
	if (!State || State->Stage != ECityDevelopmentStage::Construction || State->StationCompleteHalfDay >= 0)
		return;
	const int64 ConstructionDuration =
	    FMath::Max<int64>(1, State->ConstructionCompleteStep - Snapshot.LastFixedStepId);
	State->ConstructionCompleteStep =
	    FMath::Max(State->ConstructionCompleteStep, StartStep + ConstructionDuration);
}

void FCityDevelopmentSimulation::HoldForDelivery(const uint64 EntityId, const int64 Step,
                                                 const int64 ConstructionSteps)
{
	FCityDevelopmentState* State = Snapshot.States.Find(EntityId);
	if (State && State->Stage == ECityDevelopmentStage::Construction && State->StationCompleteHalfDay < 0)
		State->ConstructionCompleteStep =
		    FMath::Max(State->ConstructionCompleteStep, Step + FMath::Max<int64>(1, ConstructionSteps));
}

void FCityDevelopmentSimulation::InitializeExistingAsOperational(const FCityPlacementModel& City,
                                                                 const UCityBuildingCatalog* Catalog,
                                                                 const int64 FixedStepId)
{
	Reset(FixedStepId);
	Reconcile(City, Catalog);
	for (TPair<uint64, FCityDevelopmentState>& Pair : Snapshot.States)
	{
		Pair.Value.Stage = ECityDevelopmentStage::Operational;
		Pair.Value.StationCompleteHalfDay = -1;
		Pair.Value.ConstructionCompleteStep = FixedStepId;
		Pair.Value.OperationalSinceStep = FixedStepId;
	}
}

FCityDevelopmentResult FCityDevelopmentSimulation::AdvanceToStep(
    const int64 FixedStepId, FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
    const FCityServiceSnapshot& Services, const FCityNeedsSnapshot& Needs,
    const FCityEnvironmentSnapshot& Environment, const FCityEconomyTuning& Tuning)
{
	FCityDevelopmentResult Result;
	Reconcile(City, Catalog, FCityCalendar::HalfDayIndex(Snapshot.LastFixedStepId, 0, Tuning.FixedStepsPerMonth), Tuning.FixedStepsPerMonth);
	const int64 TargetStep = FMath::Max(Snapshot.LastFixedStepId, FixedStepId);
	while (Snapshot.LastFixedStepId < TargetStep)
	{
		++Snapshot.LastFixedStepId;
		const auto Stations = AdvanceStationsToHalfDay(FCityCalendar::HalfDayIndex(Snapshot.LastFixedStepId, 0, Tuning.FixedStepsPerMonth), Services);
		Result.ChangedEntityIds.Append(Stations.ChangedEntityIds);
		Result.CompletedConstruction += Stations.CompletedConstruction;
		Result.CompletedUpgrades += Stations.CompletedUpgrades;
		Step(Snapshot.LastFixedStepId, City, Catalog, Services, Needs, Environment, Tuning, Result);
	}
	return Result;
}

void FCityDevelopmentSimulation::Step(const int64 StepId, FCityPlacementModel& City,
                                      const UCityBuildingCatalog* Catalog,
                                      const FCityServiceSnapshot& Services, const FCityNeedsSnapshot& Needs,
                                      const FCityEnvironmentSnapshot& Environment,
                                      const FCityEconomyTuning& Tuning, FCityDevelopmentResult& Result)
{
	if (!Catalog)
	{
		return;
	}
	TArray<uint64> EntityIds;
	Snapshot.States.GetKeys(EntityIds);
	EntityIds.Sort();
	Snapshot.LatestPressures.Reset();
	FCityDensityDemand Markets[] = {Needs.ResidentialDensity, Needs.CommercialDensity,
	                                Needs.IndustrialDensity};
	if (Needs.bHasDensityDemand)
	{
		for (auto& Market : Markets)
			for (int32& Capacity : Market.Reserved)
				Capacity = 0;
		// Recount current levels each step: a batched advance must not reuse the
		// pre-upgrade stock and convert several lots against the same allowance.
		for (const uint64 Id : EntityIds)
		{
			const auto* Entity = City.GetEntities().Find(Id);
			const auto* Definition = Entity ? Catalog->FindDefinition(Entity->DefinitionId) : nullptr;
			const auto& State = Snapshot.States.FindChecked(Id);
			if (!Definition || Definition->bPlayerPurchasedLevels ||
			    Definition->DevelopmentDemandType == ECityDevelopmentDemandType::None)
				continue;
			const auto* Level = Definition->FindDensityLevel(State.DensityLevel);
			if (Level)
				Markets[static_cast<int32>(Definition->DevelopmentDemandType) - 1]
				    .Reserved[static_cast<int32>(State.DensityLevel)] +=
				    FCityDensityDemand::Capacity(*Level, Definition->DevelopmentDemandType);
		}
	}
	for (const uint64 EntityId : EntityIds)
	{
		FCityDevelopmentState* State = Snapshot.States.Find(EntityId);
		const FCityPlacedEntity* Entity = City.GetEntities().Find(EntityId);
		const FCityBuildingDefinition* Definition =
		    Entity ? Catalog->FindDefinition(Entity->DefinitionId) : nullptr;
		if (!State || !Entity || !Definition)
		{
			continue;
		}
		if (State->HasCivicUpgrade() && State->StationCompleteHalfDay < 0 &&
		    StepId >= State->CivicUpgradeCompleteStep &&
		    Services.GetFailure(EntityId) != ECityOperationFailure::OnFire &&
		    Services.GetFailure(EntityId) != ECityOperationFailure::FireDamage &&
		    Services.GetFailure(EntityId) != ECityOperationFailure::Repairing)
		{
			State->DensityLevel = static_cast<ECityDensityLevel>(State->PendingCivicLevel);
			State->PendingCivicLevel = -1;
			State->CivicUpgradeCompleteStep = 0;
			Result.ChangedEntityIds.Add(EntityId);
			++Result.CompletedUpgrades;
		}
		if (State->Stage == ECityDevelopmentStage::Construction)
		{
			if (State->StationCompleteHalfDay < 0 && StepId >= State->ConstructionCompleteStep)
			{
				State->Stage = ECityDevelopmentStage::Operational;
				State->OperationalSinceStep = StepId;
				Result.ChangedEntityIds.Add(EntityId);
				++Result.CompletedConstruction;
			}
			continue;
		}
	}

	int32 CommittedPower = 0;
	int32 CommittedWater = 0;
	for (const ECityDevelopmentDemandType DemandType :
	     {ECityDevelopmentDemandType::Residential, ECityDevelopmentDemandType::Commercial,
	      ECityDevelopmentDemandType::Industrial})
	{
		const auto& Market = Markets[static_cast<int32>(DemandType) - 1];
		uint64 BestEntityId = 0;
		int32 BestPressure = MIN_int32;
		int32 BestAddedPower = 0;
		int32 BestAddedWater = 0;
		for (const uint64 EntityId : EntityIds)
		{
			FCityDevelopmentState* State = Snapshot.States.Find(EntityId);
			const FCityPlacedEntity* Entity = City.GetEntities().Find(EntityId);
			const FCityBuildingDefinition* Definition =
			    Entity ? Catalog->FindDefinition(Entity->DefinitionId) : nullptr;
			if (!State || !Entity || !Definition || Definition->bPlayerPurchasedLevels ||
			    Services.GetFailure(EntityId) == ECityOperationFailure::OnFire ||
			    Services.GetFailure(EntityId) == ECityOperationFailure::FireDamage ||
			    Services.GetFailure(EntityId) == ECityOperationFailure::Repairing ||
			    State->Stage != ECityDevelopmentStage::Operational ||
			    State->DensityLevel == ECityDensityLevel::High ||
			    Definition->DevelopmentDemandType != DemandType)
			{
				continue;
			}
			const FCityBuildingLevelDefinition* Current = Definition->FindDensityLevel(State->DensityLevel);
			const FCityBuildingLevelDefinition* Target =
			    Definition->FindDensityLevel(NextDensity(State->DensityLevel));
			if (!Current || !Target)
			{
				continue;
			}
			const int32 VacantFootprints =
			    FMath::Min(EligibleFootprintsFor(DemandType, Needs), InfillReservationLots);
			const int32 UpgradeDemand = Needs.bHasDensityDemand
			                                ? Market.UpgradeDemand(Target->Level)
			                                : DemandBalanceFor(DemandType, Needs) -
			                                      VacantFootprints * CapacityFor(DemandType, *Current);
			const FCityDevelopmentPressure Pressure =
			    BuildPressure(City, *Entity, *Definition, Services, Environment, Tuning, UpgradeDemand);
			Snapshot.LatestPressures.Add(EntityId, Pressure);
			const int32 AddedPower = FMath::Max(0, Target->PowerDemand - Current->PowerDemand);
			const int32 AddedWater = FMath::Max(0, Target->WaterDemand - Current->WaterDemand);
			if (Services.GetFailure(EntityId) != ECityOperationFailure::None ||
			    (Needs.bHasDensityDemand && !Market.CanUpgrade(*Current, *Target, DemandType)) ||
			    StepId - State->OperationalSinceStep < Target->MinimumOperationalSteps ||
			    UpgradeDemand < Target->MinimumDemand ||
			    Pressure.LandValue + Pressure.PollutionCompatibility < Target->MinimumDesirability ||
			    Services.PowerCapacity < Services.PowerDemand + CommittedPower + AddedPower ||
			    Services.ServiceCapacity < Services.ServiceDemand + CommittedWater + AddedWater)
			{
				continue;
			}
			if (Pressure.Total > BestPressure || (Pressure.Total == BestPressure && EntityId < BestEntityId))
			{
				BestEntityId = EntityId;
				BestPressure = Pressure.Total;
				BestAddedPower = AddedPower;
				BestAddedWater = AddedWater;
			}
		}
		if (BestEntityId == 0)
		{
			continue;
		}
		FCityDevelopmentState& State = Snapshot.States.FindChecked(BestEntityId);
		const FCityPlacedEntity& Entity = City.GetEntities().FindChecked(BestEntityId);
		const FCityBuildingDefinition* Definition = Catalog->FindDefinition(Entity.DefinitionId);
		const FCityBuildingLevelDefinition* Target =
		    Definition ? Definition->FindDensityLevel(NextDensity(State.DensityLevel)) : nullptr;
		if (!Target)
		{
			continue;
		}
		State.DensityLevel = Target->Level;
		State.OperationalSinceStep = StepId;
		CommittedPower += BestAddedPower;
		CommittedWater += BestAddedWater;
		Result.ChangedEntityIds.Add(BestEntityId);
		++Result.CompletedUpgrades;
	}
}

bool FCityDevelopmentSimulation::RestoreState(const FCityDevelopmentSnapshot& InSnapshot,
                                              const FCityPlacementModel& City,
                                              const UCityBuildingCatalog* Catalog,
                                              const int64 MaximumFixedStepId, FString& OutError,
                                              const int64 CurrentHalfDay, const int32 StepsPerDay)
{
	if (InSnapshot.LastFixedStepId < 0 || InSnapshot.LastFixedStepId > MaximumFixedStepId)
	{
		OutError = TEXT("Development step is outside the economy timeline");
		return false;
	}
	for (const TPair<uint64, FCityDevelopmentState>& Pair : InSnapshot.States)
	{
		const FCityPlacedEntity* Entity = City.GetEntities().Find(Pair.Key);
		const FCityBuildingDefinition* Definition =
		    Entity && Catalog ? Catalog->FindDefinition(Entity->DefinitionId) : nullptr;
		const auto* Upgrade =
		    Definition && Pair.Value.HasCivicUpgrade()
		        ? Definition->FindDensityLevel(static_cast<ECityDensityLevel>(Pair.Value.PendingCivicLevel))
		        : nullptr;
		if (!Entity || Pair.Value.EntityId != Pair.Key || !Definition ||
		    Pair.Value.StationCompleteHalfDay < -1 ||
		    (Definition->bPlayerPurchasedLevels &&
		     (Entity->DefinitionId == TEXT("Police") || Entity->DefinitionId == TEXT("Fire")) &&
		     (Pair.Value.Stage == ECityDevelopmentStage::Construction || Pair.Value.HasCivicUpgrade()) &&
		     Pair.Value.StationCompleteHalfDay < 0) ||
		    (Pair.Value.StationCompleteHalfDay >= 0 &&
		     (!Definition->bPlayerPurchasedLevels ||
		      (Entity->DefinitionId != TEXT("Police") && Entity->DefinitionId != TEXT("Fire")) ||
		      (Pair.Value.Stage != ECityDevelopmentStage::Construction && !Pair.Value.HasCivicUpgrade()) ||
		      Pair.Value.StationCompleteHalfDay >
		          (CurrentHalfDay < 0 ? FCityCalendar::HalfDayIndex(MaximumFixedStepId, 0, StepsPerDay) : CurrentHalfDay) + 1 ||
		      FCityCalendar::HalfDayEndStep(Pair.Value.StationCompleteHalfDay, StepsPerDay) != (Pair.Value.HasCivicUpgrade()
		                                                          ? Pair.Value.CivicUpgradeCompleteStep
		                                                          : Pair.Value.ConstructionCompleteStep))) ||
		    Pair.Value.PendingCivicLevel < -1 || Pair.Value.PendingCivicLevel > 2 ||
		    Pair.Value.CivicUpgradeCompleteStep < 0 ||
		    (!Pair.Value.HasCivicUpgrade() && Pair.Value.CivicUpgradeCompleteStep != 0) ||
		    (Pair.Value.HasCivicUpgrade() &&
		     (!Definition->bPlayerPurchasedLevels || Pair.Value.Stage != ECityDevelopmentStage::Operational ||
		      Pair.Value.PendingCivicLevel != static_cast<int32>(Pair.Value.DensityLevel) + 1 ||
		      !Definition->FindDensityLevel(static_cast<ECityDensityLevel>(Pair.Value.PendingCivicLevel)) ||
		      Pair.Value.CivicUpgradeCompleteStep == 0 ||
		      (Upgrade && Pair.Value.StationCompleteHalfDay < 0 && Pair.Value.CivicUpgradeCompleteStep > InSnapshot.LastFixedStepId &&
		       Pair.Value.CivicUpgradeCompleteStep - InSnapshot.LastFixedStepId >
		           Upgrade->CivicConstructionSteps))) ||
		    (Definition->DensityLevels.Num() > 0 && !Definition->FindDensityLevel(Pair.Value.DensityLevel)) ||
		    Pair.Value.ConstructionCompleteStep < 0 || Pair.Value.OperationalSinceStep < 0 ||
		    (Pair.Value.Stage == ECityDevelopmentStage::Construction && Pair.Value.StationCompleteHalfDay < 0 &&
		     Pair.Value.ConstructionCompleteStep < InSnapshot.LastFixedStepId) ||
		    (Pair.Value.Stage == ECityDevelopmentStage::Operational &&
		     Pair.Value.OperationalSinceStep > InSnapshot.LastFixedStepId))
		{
			OutError = FString::Printf(TEXT("Invalid development state for entity %llu"), Pair.Key);
			return false;
		}
	}
	Snapshot = InSnapshot;
	Reconcile(City, Catalog, CurrentHalfDay, StepsPerDay);
	return true;
}

bool FCityDevelopmentSimulation::PurchaseCivicUpgrade(const uint64 EntityId,
                                                      const ECityDensityLevel ExpectedLevel,
                                                      FCityPlacementModel& City,
                                                      const UCityBuildingCatalog* Catalog,
                                                      const FCityServiceSnapshot& Services,
                                                      FString& OutStatus, const int64 CurrentHalfDay, const int32 StepsPerDay)
{
	Reconcile(City, Catalog, CurrentHalfDay, StepsPerDay);
	const auto* Entity = City.GetEntities().Find(EntityId);
	const auto* Definition = Entity && Catalog ? Catalog->FindDefinition(Entity->DefinitionId) : nullptr;
	auto* State = Snapshot.States.Find(EntityId);
	if (!Definition || !Definition->bPlayerPurchasedLevels || !Definition->IsValid() || !State)
	{
		OutStatus = TEXT("Select an upgradeable civic facility");
		return false;
	}
	if (State->DensityLevel != ExpectedLevel || State->HasCivicUpgrade())
	{
		OutStatus = TEXT("Facility changed or upgrade already underway; inspect again");
		return false;
	}
	if (State->DensityLevel == ECityDensityLevel::High)
	{
		OutStatus = TEXT("Facility fully expanded");
		return false;
	}
    if(FCityPlacementModel::RequiresFullRoadFrontage(Entity->DefinitionId) &&
        !City.HasFullRoadFrontage(Entity->Anchor,Entity->FootprintSize,Entity->QuarterTurnsClockwise))
    {
        OutStatus=TEXT("Extend ground road across the entire entrance frontage before upgrading");
        return false;
    }
	if (State->Stage != ECityDevelopmentStage::Operational || !City.HasRoadAccess(EntityId) ||
	    Services.GetFailure(EntityId) != ECityOperationFailure::None ||
	    (Services.bUsesPublicStaffing && Services.PublicStaffPercent.FindRef(EntityId) <= 0))
	{
		OutStatus =
		    TEXT("Facility must be built, repaired, connected, supplied and staffed before upgrading");
		return false;
	}
	const auto* Target = Definition->FindDensityLevel(
	    static_cast<ECityDensityLevel>(static_cast<int32>(State->DensityLevel) + 1));
	if (!Target)
	{
		OutStatus = TEXT("Facility fully expanded");
		return false;
	}
	if (!Target || Snapshot.LastFixedStepId > MAX_int64 - Target->CivicConstructionSteps ||
	    !City.TrySpendFunds(Target->UpgradeCost))
	{
		OutStatus = TEXT("Not enough treasury for this facility upgrade");
		return false;
	}
	State->PendingCivicLevel = static_cast<int32>(Target->Level);
	State->CivicUpgradeCompleteStep = Snapshot.LastFixedStepId + Target->CivicConstructionSteps;
	if (Entity->DefinitionId == TEXT("Police") || Entity->DefinitionId == TEXT("Fire"))
	{
		State->StationCompleteHalfDay =
		    (CurrentHalfDay < 0 ? FCityCalendar::HalfDayIndex(Snapshot.LastFixedStepId, 0, StepsPerDay) : CurrentHalfDay) + 1;
		State->CivicUpgradeCompleteStep = FCityCalendar::HalfDayEndStep(State->StationCompleteHalfDay, StepsPerDay);
	}
	OutStatus = FString::Printf(
	    TEXT("Upgrade purchased: $%lld. Current facility remains available during expansion."),
	    Target->UpgradeCost);
	return true;
}

FCityDevelopmentResult FCityDevelopmentSimulation::AdvanceStationsToHalfDay(
    const int64 HalfDay, const FCityServiceSnapshot& Services)
{
	FCityDevelopmentResult Result;
	for (auto& Pair : Snapshot.States)
	{
		auto& State = Pair.Value;
		if (State.StationCompleteHalfDay < 0 || HalfDay < State.StationCompleteHalfDay)
			continue;
		const auto Failure = Services.GetFailure(Pair.Key);
		if (State.HasCivicUpgrade() &&
		    (Failure == ECityOperationFailure::OnFire || Failure == ECityOperationFailure::FireDamage ||
		     Failure == ECityOperationFailure::Repairing))
			continue;
		if (State.HasCivicUpgrade())
		{
			State.DensityLevel = static_cast<ECityDensityLevel>(State.PendingCivicLevel);
			State.PendingCivicLevel = -1;
			State.CivicUpgradeCompleteStep = 0;
			++Result.CompletedUpgrades;
		}
		else
		{
			State.Stage = ECityDevelopmentStage::Operational;
			State.OperationalSinceStep = Snapshot.LastFixedStepId;
			++Result.CompletedConstruction;
		}
		State.StationCompleteHalfDay = -1;
		Result.ChangedEntityIds.Add(Pair.Key);
	}
	return Result;
}

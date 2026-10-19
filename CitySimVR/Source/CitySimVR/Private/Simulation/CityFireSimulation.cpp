#include "Simulation/CityFireSimulation.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityPopulationSimulation.h"

bool FCityFireSnapshot::IsBurning(const uint64 EntityId) const
{
	return RecentIncidents.ContainsByPredicate([EntityId](const auto& Incident)
	                                           { return Incident.EntityId == EntityId && Incident.ResolvedStep < 0; });
}

FString FCityFireSnapshot::Describe() const
{
	return FString::Printf(TEXT("Fire risk %d/100 | burning %d\nFire protection %d/%d | damaged %d%s"), AverageRisk,
	                       ActiveIncidents, ProtectedDemand, ProtectionDemand, DamagedBuildings.Num(),
	                       !IsIntroduced() ? TEXT(" | establishment grace")
	                       : IsInGrace()   ? TEXT(" | warning grace")
	                                       : TEXT(""));
}

void FCityFireSimulation::Reset(const int64 Step)
{
	Snapshot = {};
	Snapshot.LastFixedStepId = Step;
	Snapshot.NextIgnitionStep = Step;
}

uint32 FCityFireSimulation::NextRandom()
{
	Snapshot.RandomState = Snapshot.RandomState * 1664525u + 1013904223u;
	if (Snapshot.RandomState == 0)
		Snapshot.RandomState = 1;
	return Snapshot.RandomState;
}

bool FCityFireSimulation::IsFireTarget(const FName Id, const FCityEconomyTuning& Tuning)
{
	return Id == Tuning.HomeDefinitionId || Id == Tuning.ShopDefinitionId || Id == Tuning.IndustrialDefinitionId ||
	       FCityBuildingDefinition::IsSchoolFacility(Id) || FCityBuildingDefinition::IsAlternativeUtility(Id) ||
	       Id == Tuning.PowerDefinitionId || Id == Tuning.WaterTreatmentDefinitionId || Id == TEXT("Police") ||
	       Id == TEXT("Hospital") || Id == TEXT("WasteDepot") || Id == TEXT("Recycling");
}

bool FCityFireSimulation::AreAdjacent(const FCityPlacedEntity& A, const FCityPlacedEntity& B)
{
	const bool XOverlap = A.Anchor.X < B.Anchor.X + B.FootprintSize.X && B.Anchor.X < A.Anchor.X + A.FootprintSize.X;
	const bool YOverlap = A.Anchor.Y < B.Anchor.Y + B.FootprintSize.Y && B.Anchor.Y < A.Anchor.Y + A.FootprintSize.Y;
	return (XOverlap &&
	        (A.Anchor.Y + A.FootprintSize.Y == B.Anchor.Y || B.Anchor.Y + B.FootprintSize.Y == A.Anchor.Y)) ||
	       (YOverlap && (A.Anchor.X + A.FootprintSize.X == B.Anchor.X || B.Anchor.X + B.FootprintSize.X == A.Anchor.X));
}

int32 FCityFireSimulation::SpreadChance(const FCityPlacedEntity& Source, const FCityPlacedEntity& Target,
                                        const FCityEnvironmentSnapshot& Weather, const FCityEconomyTuning& Tuning,
                                        const int32 Protection)
{
	if (Target.DefinitionId == TEXT("Fire") || !AreAdjacent(Source, Target))
		return 0;
	const FIntPoint Winds[] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
	const FIntPoint Wind = Winds[static_cast<uint8>(Weather.WindDirection)];
	const int32 Dx = Target.Anchor.X * 2 + Target.FootprintSize.X - Source.Anchor.X * 2 - Source.FootprintSize.X;
	const int32 Dy = Target.Anchor.Y * 2 + Target.FootprintSize.Y - Source.Anchor.Y * 2 - Source.FootprintSize.Y;
	int32 Chance = Tuning.FireSpreadChance +
	               FMath::Sign(Dx * Wind.X + Dy * Wind.Y) * Weather.WindStrength * Tuning.FireWindSpreadModifier;
	Chance = FMath::Clamp(Chance, 0, 100);
	if (Weather.bPrecipitation)
		Chance /= 2;
	return Chance * (100 - FMath::Clamp(Protection, 0, 100)) / 100;
}

void FCityFireSimulation::Refresh(const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
                                  const FCityDevelopmentSnapshot& Development, const FCityEnvironmentSnapshot& Weather,
                                  const FCityEconomyTuning& Tuning, const bool bUpkeepShortfall,
                                  const FCityPopulationSnapshot* Population)
{
	Snapshot.Dimensions = City.GetGrid().GetDimensions();
	RestoreFireproofStations(City);
	// Demolition removes damage and ends that incident; rendering refresh never draws random samples.
	for (auto It = Snapshot.DamagedBuildings.CreateIterator(); It; ++It)
		if (!City.GetEntities().Contains(It.Key()))
			It.RemoveCurrent();
	for (auto& Incident : Snapshot.RecentIncidents)
		if (Incident.ResolvedStep < 0 && !City.GetEntities().Contains(Incident.EntityId))
			Incident.ResolvedStep = Snapshot.LastFixedStepId;
	Snapshot.UnprotectedRisk.Reset();
	Snapshot.CrimeRiskContribution.Reset();
	Snapshot.IndustryRiskContribution.Reset();
	Snapshot.EnRouteStations.Reset();
	Snapshot.EntityRisk.Reset();
	Snapshot.EntityProtection.Reset();
	Snapshot.StationUsedCapacity.Reset();
	Snapshot.ResponseStations.Reset();
	Snapshot.CellRisk.Init(0, Snapshot.Dimensions.GetCellCount());
	Snapshot.CellStatus.Init(0, Snapshot.Dimensions.GetCellCount());
	Snapshot.ActiveIncidents = Snapshot.OperationalStations = Snapshot.ProtectionDemand = Snapshot.ProtectedDemand = 0;
	TArray<uint64> Ids;
	City.GetEntities().GetKeys(Ids);
	Ids.Sort();
	TArray<uint64> Stations;
	TMap<uint64, FCityServiceReach> Reaches;
	for (const uint64 Id : Ids)
	{
		if (City.GetEntities().FindChecked(Id).DefinitionId == TEXT("Fire") && Development.IsOperational(Id) &&
		    !Snapshot.IsDisabled(Id) && Tuning.FireProtectionCapacity > 0 &&
		    Services.GetFailure(Id) == ECityOperationFailure::None && !bUpkeepShortfall &&
		    (!Services.bUsesPublicStaffing || Services.PublicStaffPercent.FindRef(Id) > 0))
		{
			Stations.Add(Id);
			Snapshot.StationUsedCapacity.Add(Id, 0);
			FCityServiceEvaluator::BuildProviderReach(City, Id,
			                                          Services.StationRoadRanges.Contains(Id)
			                                              ? Services.StationRoadRanges.FindRef(Id)
			                                              : Tuning.FireRoadRangeHops,
			                                          true, Reaches.Add(Id));
		}
	}
	Snapshot.OperationalStations = Stations.Num();
	int64 TotalRisk = 0;
	for (const uint64 Id : Ids)
	{
		const auto& Entity = City.GetEntities().FindChecked(Id);
		if (!IsFireTarget(Entity.DefinitionId, Tuning) || !Development.IsOperational(Id))
			continue;
		const int32 Demand =
		    FMath::Max(1, Entity.DefinitionId == Tuning.HomeDefinitionId ? Services.EntityResidents.FindRef(Id)
		                                                                 : Services.EntityJobs.FindRef(Id));
		int32 Protected = 0;
		for (const uint64 Station : Stations)
		{
			if (!FCityServiceEvaluator::IsFootprintReached(City, Reaches.FindChecked(Station), Entity.Anchor,
			                                               Entity.FootprintSize))
				continue;
			int32& Used = Snapshot.StationUsedCapacity.FindChecked(Station);
			const int32 Capacity = Services.StationProtectionCapacities.Contains(Station)
			                           ? Services.StationProtectionCapacities.FindRef(Station)
			                           : Tuning.FireProtectionCapacity;
			const int32 Allocated = FMath::Min(
			    Demand - Protected,
			    FMath::Max(
			        0, Capacity * (Services.bUsesPublicStaffing ? Services.PublicStaffPercent.FindRef(Station) : 100) /
			                   100 -
			               Used));
			Used += Allocated;
			Protected += Allocated;
			if (Protected == Demand)
				break;
		}
		Snapshot.ProtectionDemand += Demand;
		Snapshot.ProtectedDemand += Protected;
		const int32 Protection = Protected * 100 / Demand;
		Snapshot.EntityProtection.Add(Id, Protection);
		int32 CrimeRisk = 0;
		for (int32 Y = Entity.Anchor.Y; Y < Entity.Anchor.Y + Entity.FootprintSize.Y; ++Y)
			for (int32 X = Entity.Anchor.X; X < Entity.Anchor.X + Entity.FootprintSize.X; ++X)
				if (Services.CrimeRisk.IsValidIndex(Y * Snapshot.Dimensions.GetWidth() + X))
					CrimeRisk += Services.CrimeRisk[Y * Snapshot.Dimensions.GetWidth() + X];
		CrimeRisk = CrimeRisk * FMath::Clamp(Tuning.FireCrimeRiskWeight, 0, 100) /
		            (100 * Entity.FootprintSize.X * Entity.FootprintSize.Y);
		int32 IndustryCells = 0, NearbyCells = 0;
		const int32 Radius = FMath::Clamp(Tuning.FireIndustryRadiusCells, 1, 16);
		const int32 Cx = Entity.Anchor.X + Entity.FootprintSize.X / 2,
		            Cy = Entity.Anchor.Y + Entity.FootprintSize.Y / 2;
		for (int32 Y = FMath::Max(0, Cy - Radius); Y <= FMath::Min(Snapshot.Dimensions.GetHeight() - 1, Cy + Radius);
		     ++Y)
			for (int32 X = FMath::Max(0, Cx - Radius); X <= FMath::Min(Snapshot.Dimensions.GetWidth() - 1, Cx + Radius);
			     ++X)
			{
				++NearbyCells;
				const auto* Nearby = City.GetEntities().Find(City.GetGrid().GetOccupant({X, Y}));
				IndustryCells += Nearby && Nearby->DefinitionId == Tuning.IndustrialDefinitionId &&
				                         Development.IsOperational(Nearby->EntityId)
				                     ? 1
				                     : 0;
			}
		const int32 IndustrialRisk =
		    IndustryCells * FMath::Clamp(Tuning.FireIndustryRiskWeight, 0, 100) / FMath::Max(1, NearbyCells);
		Snapshot.CrimeRiskContribution.Add(Id, CrimeRisk);
		Snapshot.IndustryRiskContribution.Add(Id, IndustrialRisk);
		int32 Risk = Snapshot.IsIntroduced()
		                 ? Tuning.FireBaseRisk + (Entity.DefinitionId == Tuning.IndustrialDefinitionId ? 10 : 0) +
		                       (Weather.Temperature == ECityTemperatureBand::Hot ? 6 : 0) + CrimeRisk + IndustrialRisk
		                 : 0;
		if (Weather.bPrecipitation)
			Risk /= 2;
		Snapshot.UnprotectedRisk.Add(Id, FMath::Clamp(Risk, 0, 100));
		Risk = FMath::Clamp(Risk * (100 - Tuning.FireRiskReduction * Protection / 100) / 100, 0, 100);
		Snapshot.EntityRisk.Add(Id, Risk);
		TotalRisk += Risk;
		const auto* Damage = Snapshot.DamagedBuildings.Find(Id);
		const uint8 Status = Snapshot.IsBurning(Id) ? 4 : Damage ? (Damage->RepairCompleteStep >= 0 ? 6 : 5) : 1;
		for (int32 Y = Entity.Anchor.Y; Y < Entity.Anchor.Y + Entity.FootprintSize.Y; ++Y)
			for (int32 X = Entity.Anchor.X; X < Entity.Anchor.X + Entity.FootprintSize.X; ++X)
			{
				const int32 Index = Y * Snapshot.Dimensions.GetWidth() + X;
				Snapshot.CellRisk[Index] = Risk;
				Snapshot.CellStatus[Index] = Status;
			}
	}
	Snapshot.AverageRisk =
	    Snapshot.EntityRisk.IsEmpty() ? 0 : static_cast<int32>(TotalRisk / Snapshot.EntityRisk.Num());
	// Each starter station can respond to one fire. Incident ID order prevents allocation oscillation.
	TSet<uint64> BusyStations;
	for (const auto& Incident : Snapshot.RecentIncidents)
	{
		if (Incident.ResolvedStep >= 0)
			continue;
		++Snapshot.ActiveIncidents;
		const auto* Target = City.GetEntities().Find(Incident.EntityId);
		if (!Target)
			continue;
		if (Population)
		{
			for (const auto& Trip : Population->TransitTrips)
				if (Trip.Purpose == ECityTripPurpose::FireResponse && Trip.IncidentId == Incident.Id &&
				    Stations.Contains(Trip.StationEntityId))
				{
					if (Trip.bCompleted)
						Snapshot.ResponseStations.Add(Incident.EntityId, Trip.StationEntityId);
					else
						Snapshot.EnRouteStations.Add(Incident.EntityId, Trip.StationEntityId);
				}
			continue;
		}
		for (const uint64 Station : Stations)
			if (!BusyStations.Contains(Station) &&
			    FCityServiceEvaluator::IsFootprintReached(City, Reaches.FindChecked(Station), Target->Anchor,
			                                              Target->FootprintSize))
			{
				BusyStations.Add(Station);
				Snapshot.ResponseStations.Add(Incident.EntityId, Station);
				break;
			}
	}
}

void FCityFireSimulation::RestoreFireproofStations(const FCityPlacementModel& City)
{
	for (auto It = Snapshot.DamagedBuildings.CreateIterator(); It; ++It)
	{
		const auto* E = City.GetEntities().Find(It.Key());
		if (E && E->DefinitionId == TEXT("Fire") && It.Value().Cause == 0)
			It.RemoveCurrent();
	}
	for (auto& I : Snapshot.RecentIncidents)
	{
		const auto* E = City.GetEntities().Find(I.EntityId);
		if (E && E->DefinitionId == TEXT("Fire") && I.ResolvedStep < 0)
			I.ResolvedStep = Snapshot.LastFixedStepId;
	}
}

bool FCityFireSimulation::ApplyNaturalDamage(const uint64 EntityId, const int32 Cause, const int32 Damage)
{
	if (!EntityId || Cause < 1 || Cause > 2 || Snapshot.IsDisabled(EntityId) ||
	    Snapshot.DamagedBuildings.Num() >= Snapshot.Dimensions.GetCellCount())
		return false;
	Snapshot.DamagedBuildings.Add(EntityId, {FMath::Clamp(Damage, 1, 100), -1, Cause});
	return true;
}

bool FCityFireSimulation::StartTest(const uint64 EntityId, FString& OutStatus)
{
	int32 Active = 0;
	for (const auto& I : Snapshot.RecentIncidents)
		Active += I.ResolvedStep < 0 ? 1 : 0;
	if (!EntityId || Snapshot.IsDisabled(EntityId))
	{
		OutStatus = TEXT("Choose an undamaged building");
		return false;
	}
	if (Active >= MaximumActiveIncidents || Snapshot.FireDamageCount() >= MaximumDamagedBuildings)
	{
		OutStatus = TEXT("Fire test limit reached; extinguish/repair existing fires first");
		return false;
	}
	Ignite(EntityId, 0);
	Snapshot.RecentIncidents.Last().bTest = true;
	OutStatus = TEXT("Test fire started; spread, damage and repair are real");
	return true;
}

void FCityFireSimulation::Ignite(const uint64 EntityId, const uint64 SourceId)
{
	FCityFireIncident Incident;
	Incident.Id = ++Snapshot.TotalIncidents;
	Incident.EntityId = EntityId;
	Incident.SpreadFromEntityId = SourceId;
	Incident.StartStep = Snapshot.LastFixedStepId;
	if (SourceId)
		for (const auto& Source : Snapshot.RecentIncidents)
			if (Source.EntityId == SourceId && Source.ResolvedStep < 0)
			{
				Incident.bTest = Source.bTest;
				Incident.bArson = Source.bArson;
				Incident.OffenderCitizenId = Source.OffenderCitizenId;
			}
	Snapshot.RecentIncidents.Add(Incident);
	Snapshot.DamagedBuildings.Add(EntityId, {1, -1});
	if (Snapshot.RecentIncidents.Num() > MaximumRecentIncidents)
	{
		const int32 OldestResolved =
		    Snapshot.RecentIncidents.IndexOfByPredicate([](const auto& Item) { return Item.ResolvedStep >= 0; });
		check(OldestResolved != INDEX_NONE);
		Snapshot.RecentIncidents.RemoveAt(OldestResolved);
	}
}

bool FCityFireSimulation::StartArsonIgnition(const uint64 EntityId, FString& Status)
{
	if (!Snapshot.IsIntroduced() || Snapshot.IsInGrace() || !StartTest(EntityId, Status))
		return false;
	Snapshot.RecentIncidents.Last().bTest = false;
	return true;
}

void FCityFireSimulation::MarkArson(const uint64 EntityId, const uint64 CitizenId)
{
	for (auto& Incident : Snapshot.RecentIncidents)
		if (Incident.EntityId == EntityId && Incident.ResolvedStep < 0 && !Incident.SpreadFromEntityId)
		{
			Incident.bArson = true;
			Incident.OffenderCitizenId = CitizenId;
		}
}

void FCityFireSimulation::AdvanceToStep(const int64 Step, const FCityPlacementModel& City,
                                        const FCityServiceSnapshot& Services,
                                        const FCityDevelopmentSnapshot& Development,
                                        const FCityEnvironmentSnapshot& Weather, const FCityEconomyTuning& Tuning,
                                        const bool bUpkeepShortfall, const FCityPopulationSnapshot* Population)
{
	while (Snapshot.LastFixedStepId < Step)
	{
		++Snapshot.LastFixedStepId;
		for (auto It = Snapshot.DamagedBuildings.CreateIterator(); It; ++It)
			if (It.Value().RepairCompleteStep >= 0 && It.Value().RepairCompleteStep <= Snapshot.LastFixedStepId)
				It.RemoveCurrent();
		if (!Snapshot.IsIntroduced() && Services.Residents >= Tuning.FirePopulationThreshold)
		{
			Snapshot.ActivationStep = Snapshot.LastFixedStepId;
			Snapshot.GraceEndStep =
			    Snapshot.ActivationStep +
			    static_cast<int64>(FMath::Max(1, Tuning.FixedStepsPerMonth)) * Tuning.FireGraceMonths;
			Snapshot.NextIgnitionStep = Snapshot.GraceEndStep;
		}
		Refresh(City, Services, Development, Weather, Tuning, bUpkeepShortfall, Population);
		TArray<uint64> Targets;
		Snapshot.EntityRisk.GetKeys(Targets);
		Targets.Sort();
		// Sample only the fires present at step start; additions are committed afterwards.
		TMap<uint64, uint64> Additions;
		for (auto& Incident : Snapshot.RecentIncidents)
		{
			if (Incident.ResolvedStep >= 0)
				continue;
			const bool bResponse = Snapshot.ResponseStations.Contains(Incident.EntityId);
			Incident.ResponseSteps += bResponse ? 1 : 0;
			Snapshot.DamagedBuildings.FindChecked(Incident.EntityId).Damage =
			    FMath::Min(100, Snapshot.DamagedBuildings.FindChecked(Incident.EntityId).Damage +
			                        (bResponse ? 1 : FMath::Clamp(Tuning.FireDamagePerStep, 1, 100)));
			if (Incident.ResponseSteps >= ResponseStepsToExtinguish ||
			    Snapshot.LastFixedStepId - Incident.StartStep >= MaximumBurnSteps)
			{
				const bool bExtinguished = Incident.ResponseSteps >= ResponseStepsToExtinguish;
				if (!bExtinguished)
					Snapshot.DamagedBuildings.FindChecked(Incident.EntityId).Damage = 100;
				Incident.ResolvedStep = Snapshot.LastFixedStepId;
				// Prompt suppression leaves only superficial damage. Removing the
				// record restores capacity and the original mesh through the same
				// derived-state path as completed paid repair, without a treasury bill.
				if (bExtinguished && Snapshot.DamagedBuildings.FindChecked(Incident.EntityId).Damage <=
				                         FMath::Clamp(Tuning.FireMinorDamageAutoRepairLimit, 0, 99))
					Snapshot.DamagedBuildings.Remove(Incident.EntityId);
				continue;
			}
			if (bResponse ||
			    (Snapshot.LastFixedStepId - Incident.StartStep) % FMath::Max(1, Tuning.FireSpreadIntervalSteps) != 0)
				continue;
			const auto& Source = City.GetEntities().FindChecked(Incident.EntityId);
			for (const uint64 Target : Targets)
			{
				if (Snapshot.IsDisabled(Target) || Additions.Contains(Target) ||
				    !AreAdjacent(Source, City.GetEntities().FindChecked(Target)))
					continue;
				const int32 Chance = SpreadChance(Source, City.GetEntities().FindChecked(Target), Weather, Tuning,
				                                  Snapshot.EntityProtection.FindRef(Target));
				if (static_cast<int32>((NextRandom() >> 16) % 100) < Chance)
					Additions.Add(Target, Incident.EntityId);
			}
		}
		if (Snapshot.IsIntroduced() && !Snapshot.IsInGrace() && Snapshot.LastFixedStepId >= Snapshot.NextIgnitionStep)
		{
			Snapshot.NextIgnitionStep = Snapshot.LastFixedStepId + FMath::Max(1, Tuning.FireCheckIntervalSteps);
			TArray<uint64> Eligible;
			for (const uint64 Target : Targets)
				if (!Snapshot.IsDisabled(Target) && !Additions.Contains(Target))
					Eligible.Add(Target);
			if (!Eligible.IsEmpty() && Snapshot.ActiveIncidents < MaximumActiveIncidents &&
			    Snapshot.FireDamageCount() < MaximumDamagedBuildings)
			{
				const uint64 Target = Eligible[(NextRandom() >> 16) % Eligible.Num()];
				const int32 Roll = (NextRandom() >> 16) % 100;
				if (Roll < Snapshot.EntityRisk.FindRef(Target))
					Additions.Add(Target, 0);
				else
				{
					const int32 UnprotectedRisk = Snapshot.UnprotectedRisk.FindRef(Target);
					if (Roll < FMath::Clamp(UnprotectedRisk, 0, 100))
						++Snapshot.PreventedIncidents;
				}
			}
		}
		int32 Active = 0;
		for (const auto& Incident : Snapshot.RecentIncidents)
			Active += Incident.ResolvedStep < 0 ? 1 : 0;
		TArray<uint64> NewIds;
		Additions.GetKeys(NewIds);
		NewIds.Sort();
		int32 SpreadAdded = 0;
		for (const uint64 Target : NewIds)
		{
			if (Active >= MaximumActiveIncidents || Snapshot.FireDamageCount() >= MaximumDamagedBuildings)
				break;
			const uint64 Source = Additions.FindChecked(Target);
			if (Source && SpreadAdded >= 1)
				continue;
			Ignite(Target, Source);
			++Active;
			if (Source)
				++SpreadAdded;
		}
		Refresh(City, Services, Development, Weather, Tuning, bUpkeepShortfall, Population);
	}
}

int64 FCityFireSimulation::RepairCost(const uint64 EntityId, const FCityBuildingDefinition& Definition,
                                      const FCityEconomyTuning& Tuning) const
{
	const auto* Damage = Snapshot.DamagedBuildings.Find(EntityId);
	if (!Damage)
		return 0;
	// Scale the affordable recovery bill with damage; no automatic treasury deduction.
	const int64 Base = Definition.Cost / 100 * FMath::Clamp(Tuning.FireRepairCostPercent, 0, 100) +
	                   (Definition.Cost % 100 * FMath::Clamp(Tuning.FireRepairCostPercent, 0, 100) + 99) / 100;
	return FMath::Max<int64>(25, Base / 100 * Damage->Damage + (Base % 100 * Damage->Damage + 99) / 100);
}

bool FCityFireSimulation::Repair(const uint64 EntityId, FCityPlacementModel& City,
                                 const FCityBuildingDefinition& Definition, const FCityEconomyTuning& Tuning,
                                 FString& OutStatus)
{
	auto* Damage = Snapshot.DamagedBuildings.Find(EntityId);
	if (!Damage || !City.GetEntities().Contains(EntityId))
	{
		OutStatus = TEXT("No structural damage to repair");
		return false;
	}
	if (Snapshot.IsBurning(EntityId))
	{
		OutStatus = TEXT("Wait for the fire to be extinguished");
		return false;
	}
	if (Damage->RepairCompleteStep >= 0)
	{
		OutStatus = TEXT("Repair is already underway");
		return false;
	}
	const int64 Cost = RepairCost(EntityId, Definition, Tuning);
	if (!City.TrySpendFunds(Cost))
	{
		OutStatus = FString::Printf(TEXT("Repair needs $%lld; insufficient funds"), Cost);
		return false;
	}
	Damage->RepairCompleteStep = Snapshot.LastFixedStepId + FMath::Max(1, Tuning.FireRepairSteps);
	OutStatus =
	    FString::Printf(TEXT("Repair started: $%lld, %d game days"), Cost, FMath::Max(1, Tuning.FireRepairSteps));
	return true;
}

bool FCityFireSimulation::ValidateState(const FCityFireSnapshot& State, const int64 MaximumStep, FString& OutError)
{
	bool bHasFireDamage = false;
	for (const auto& Pair : State.DamagedBuildings)
		bHasFireDamage |= Pair.Value.Cause == 0;
	if (!State.Dimensions.IsValid() || State.LastFixedStepId < 0 || State.LastFixedStepId > MaximumStep ||
	    State.LastFixedStepId > MAX_int64 - 200000 || State.RandomState == 0 || State.ActivationStep < -1 ||
	    State.ActivationStep > State.LastFixedStepId ||
	    (State.ActivationStep < 0
	         ? State.GraceEndStep != -1
	         : State.GraceEndStep < State.ActivationStep || State.GraceEndStep - State.ActivationStep > 100000) ||
	    (!State.IsIntroduced() && bHasFireDamage &&
	     (State.RecentIncidents.IsEmpty() ||
	      State.RecentIncidents.ContainsByPredicate([](const auto& I) { return !I.bTest; }))) ||
	    State.NextIgnitionStep < 0 ||
	    State.NextIgnitionStep > FMath::Max(State.LastFixedStepId, State.GraceEndStep) + 100000 ||
	    State.TotalIncidents < 0 || State.TotalIncidents > MAX_int64 - 100000 || State.PreventedIncidents < 0 ||
	    State.PreventedIncidents > MAX_int64 - 100000 || State.RecentIncidents.Num() > MaximumRecentIncidents ||
	    State.DamagedBuildings.Num() > State.Dimensions.GetCellCount() ||
	    State.FireDamageCount() > MaximumDamagedBuildings)
	{
		OutError = TEXT("Invalid fire state");
		return false;
	}
	for (const auto& Pair : State.DamagedBuildings)
		if (Pair.Key == 0 || Pair.Value.Cause < 0 || Pair.Value.Cause > 2 || Pair.Value.Damage < 1 ||
		    Pair.Value.Damage > 100 || Pair.Value.RepairCompleteStep < -1 ||
		    (Pair.Value.RepairCompleteStep >= 0 && (Pair.Value.RepairCompleteStep <= State.LastFixedStepId ||
		                                            Pair.Value.RepairCompleteStep > State.LastFixedStepId + 100000)))
		{
			OutError = TEXT("Invalid fire damage/repair state");
			return false;
		}
	int64 Previous = 0;
	TSet<uint64> Active;
	for (const auto& Incident : State.RecentIncidents)
	{
		if ((!Incident.bTest && !State.IsIntroduced()) || Incident.Id <= Previous ||
		    Incident.Id > State.TotalIncidents || Incident.EntityId == 0 ||
		    (!Incident.bTest && Incident.StartStep < State.GraceEndStep) || Incident.StartStep < 0 ||
		    Incident.StartStep > State.LastFixedStepId || Incident.ResolvedStep < -1 ||
		    (Incident.ResolvedStep >= 0 &&
		     (Incident.ResolvedStep < Incident.StartStep || Incident.ResolvedStep > State.LastFixedStepId)) ||
		    Incident.SpreadFromEntityId == Incident.EntityId || Incident.ResponseSteps < 0 ||
		    Incident.ResponseSteps > ResponseStepsToExtinguish)
		{
			OutError = TEXT("Invalid fire incident");
			return false;
		}
		if (Incident.ResolvedStep < 0)
		{
			const auto* Damage = State.DamagedBuildings.Find(Incident.EntityId);
			if (Active.Contains(Incident.EntityId) || Active.Num() >= MaximumActiveIncidents || !Damage ||
			    Damage->Cause != 0 || Damage->RepairCompleteStep >= 0 ||
			    Incident.ResponseSteps >= ResponseStepsToExtinguish ||
			    State.LastFixedStepId - Incident.StartStep >= MaximumBurnSteps)
			{
				OutError = TEXT("Invalid active fire/cap");
				return false;
			}
			Active.Add(Incident.EntityId);
		}
		Previous = Incident.Id;
	}
	return true;
}

bool FCityFireSimulation::RestoreState(const FCityFireSnapshot& State, const int64 MaximumStep, FString& OutError)
{
	if (!ValidateState(State, MaximumStep, OutError))
		return false;
	Snapshot = State;
	return true;
}

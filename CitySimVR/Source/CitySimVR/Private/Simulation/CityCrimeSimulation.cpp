#include "Simulation/CityCrimeSimulation.h"
#include "Commands/CityPlacementModel.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityFireSimulation.h"

int32 FCityCrimeSnapshot::GetRisk(const FCityCellCoord& Cell) const
{
	if (!Dimensions.IsValid() || Cell.X < 0 || Cell.Y < 0 || Cell.X >= Dimensions.GetWidth() ||
	    Cell.Y >= Dimensions.GetHeight())
		return 0;
	const int32 Index = Cell.Y * Dimensions.GetWidth() + Cell.X;
	return Cell.X >= 0 && Cell.X < Dimensions.GetWidth() && Cell.Y >= 0 && Cell.Y < Dimensions.GetHeight() &&
	               CellRisk.IsValidIndex(Index)
	           ? CellRisk[Index]
	           : 0;
}

FString FCityCrimeSnapshot::Describe() const
{
	if (!IsIntroduced() && ActiveIncidents == 0)
		return TEXT("Crime: city establishment grace");
	return FString::Printf(
	    TEXT(
	        "Crime risk %d/100 | active %d\nJob gap %d | homeless %d\nPolice protected %d/%d%s\nArson %lld | arrests %lld"),
	    AverageRisk, ActiveIncidents, JobShortage, HomelessResidents, ProtectedDemand, ProtectionDemand,
	    IsInGrace() ? TEXT(" | grace") : TEXT(""), TotalArsons, TotalArrests);
}

void FCityCrimeSimulation::Reset(const int64 Step, const int32 HousedResidents)
{
	Snapshot = {};
	Snapshot.LastFixedStepId = Step;
	Snapshot.PreviousHousedResidents = FMath::Clamp(HousedResidents, 0, MaximumResidents);
}

uint32 FCityCrimeSimulation::NextRandom()
{
	Snapshot.RandomState = Snapshot.RandomState * 1664525u + 1013904223u;
	if (Snapshot.RandomState == 0)
		Snapshot.RandomState = 1;
	return Snapshot.RandomState;
}

void FCityCrimeSimulation::Refresh(const FCityPlacementModel& City, const FCityServiceSnapshot& Services,
                                   const FCityDevelopmentSnapshot& Development, const FCityEconomyTuning& Tuning,
                                   const bool bUpkeepShortfall)
{
	Snapshot.HomelessResidents = Services.HomelessResidents;
	Snapshot.HousedResidents = Services.Residents - Services.HomelessResidents;
	const int32 People = Snapshot.HousedResidents + Snapshot.HomelessResidents;
	Snapshot.JobShortage =
	    Services.UnemployedResidents >= 0 ? Services.UnemployedResidents : FMath::Max(0, People - Services.Jobs);
	const int32 JobPressure =
	    People > 0
	        ? static_cast<int32>(static_cast<int64>(Snapshot.JobShortage) * Tuning.CrimeJobShortageWeight / People)
	        : 0;
	const int32 HousingPressure = People > 0 ? static_cast<int32>(static_cast<int64>(Snapshot.HomelessResidents) *
	                                                              Tuning.CrimeHomelessnessWeight / People)
	                                         : 0;
	Snapshot.BaseRisk =
	    Snapshot.IsIntroduced() ? FMath::Clamp(Tuning.CrimeBaseRisk + JobPressure + HousingPressure, 0, 100) : 0;
	Snapshot.Dimensions = City.GetGrid().GetDimensions();
	Snapshot.CellRisk.Init(0, Snapshot.Dimensions.GetCellCount());
	Snapshot.EntityRisk.Reset();
	Snapshot.EntityProtection.Reset();
	Snapshot.StationUsedCapacity.Reset();
	Snapshot.OperationalStations = Snapshot.ProtectionDemand = Snapshot.ProtectedDemand = Snapshot.ActiveIncidents = 0;
	TArray<uint64> Ids;
	City.GetEntities().GetKeys(Ids);
	Ids.Sort();
	TArray<uint64> Stations;
	TMap<uint64, FCityServiceReach> Reaches;
	for (const uint64 Id : Ids)
	{
		const auto& Entity = City.GetEntities().FindChecked(Id);
		const auto* State = Development.Find(Id);
		if (Entity.DefinitionId == TEXT("Police") && (!State || State->Stage != ECityDevelopmentStage::Construction) &&
		    Services.GetFailure(Id) == ECityOperationFailure::None && !bUpkeepShortfall &&
		    (!Services.bUsesPublicStaffing || Services.PublicStaffPercent.FindRef(Id) > 0))
		{
			Stations.Add(Id);
			FCityServiceEvaluator::BuildProviderReach(City, Id,
			                                          Services.StationRoadRanges.Contains(Id)
			                                              ? Services.StationRoadRanges.FindRef(Id)
			                                              : Tuning.PoliceRoadRangeHops,
			                                          true, Reaches.Add(Id));
			Snapshot.StationUsedCapacity.Add(Id, 0);
		}
	}
	Snapshot.OperationalStations = Stations.Num();
	int64 TotalRisk = 0;
	for (const uint64 Id : Ids)
	{
		const auto& Entity = City.GetEntities().FindChecked(Id);
		const bool bHome = Entity.DefinitionId == Tuning.HomeDefinitionId;
		if (!bHome && Entity.DefinitionId != Tuning.ShopDefinitionId &&
		    Entity.DefinitionId != Tuning.IndustrialDefinitionId)
			continue;
		const auto* State = Development.Find(Id);
		if (State && State->Stage == ECityDevelopmentStage::Construction)
			continue;
		const int32 Demand =
		    FMath::Max(1, bHome ? Services.EntityResidents.FindRef(Id) : Services.EntityJobs.FindRef(Id));
		int32 Protected = 0;
		for (const uint64 Station : Stations)
		{
			if (!FCityServiceEvaluator::IsFootprintReached(City, Reaches.FindChecked(Station), Entity.Anchor,
			                                               Entity.FootprintSize))
				continue;
			int32& Used = Snapshot.StationUsedCapacity.FindChecked(Station);
			const int32 Capacity = Services.StationProtectionCapacities.Contains(Station)
			                           ? Services.StationProtectionCapacities.FindRef(Station)
			                           : Tuning.PoliceProtectionCapacity;
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
		Snapshot.EntityProtection.Add(Id, Protected * 100 / Demand);
		const bool bIncident = Snapshot.RecentIncidents.ContainsByPredicate(
		    [&](const FCityCrimeIncident& Incident)
		    { return Incident.EntityId == Id && Incident.EndStep > Snapshot.LastFixedStepId; });
		Snapshot.ActiveIncidents += bIncident ? 1 : 0;
		const int32 Risk = FMath::Clamp(
		    Snapshot.BaseRisk - Tuning.PoliceRiskReduction * Protected / Demand + (bIncident ? 15 : 0), 0, 100);
		Snapshot.EntityRisk.Add(Id, Risk);
		TotalRisk += Risk;
		for (int32 Y = Entity.Anchor.Y; Y < Entity.Anchor.Y + Entity.FootprintSize.Y; ++Y)
			for (int32 X = Entity.Anchor.X; X < Entity.Anchor.X + Entity.FootprintSize.X; ++X)
				Snapshot.CellRisk[Y * Snapshot.Dimensions.GetWidth() + X] = Risk;
	}
	Snapshot.AverageRisk =
	    Snapshot.EntityRisk.IsEmpty() ? 0 : static_cast<int32>(TotalRisk / Snapshot.EntityRisk.Num());
}

void FCityCrimeSimulation::AdvanceToStep(const int64 Step, const FCityPlacementModel& City,
                                         const FCityServiceSnapshot& Services,
                                         const FCityDevelopmentSnapshot& Development, const FCityEconomyTuning& Tuning,
                                         const bool bUpkeepShortfall, const FCityPopulationSnapshot* Population)
{
	while (Snapshot.LastFixedStepId < Step)
	{
		++Snapshot.LastFixedStepId;
		if (Population && !bUpkeepShortfall)
			for (const auto& Trip : Population->TransitTrips)
				if (Trip.Purpose == ECityTripPurpose::PoliceResponse && Trip.bCompleted &&
				    Services.GetFailure(Trip.StationEntityId) == ECityOperationFailure::None &&
				    Services.PublicStaffPercent.FindRef(Trip.StationEntityId) > 0)
					for (auto& Incident : Snapshot.RecentIncidents)
						if (Incident.Id == Trip.IncidentId && Incident.EndStep > Snapshot.LastFixedStepId)
							Incident.EndStep = FMath::Min(Incident.EndStep, Snapshot.LastFixedStepId + 1);
		const int32 People = FMath::Clamp(Services.Residents, 0, MaximumResidents);

		Snapshot.PreviousHousedResidents = FMath::Max(0, People - Services.HomelessResidents);
		if (!Snapshot.IsIntroduced() && People >= Tuning.CrimePopulationThreshold)
		{
			Snapshot.ActivationStep = Snapshot.LastFixedStepId;
			Snapshot.GraceEndStep =
			    Snapshot.ActivationStep +
			    static_cast<int64>(FMath::Max(1, Tuning.FixedStepsPerMonth)) * Tuning.CrimeGraceMonths;
		}
		Refresh(City, Services, Development, Tuning, bUpkeepShortfall);
		if (!Snapshot.IsIntroduced() || Snapshot.IsInGrace() || Snapshot.EntityRisk.IsEmpty() ||
		    Snapshot.LastFixedStepId % FMath::Max(1, Tuning.CrimeCheckIntervalSteps) != 0 ||
		    Snapshot.ActiveIncidents >= MaximumActiveIncidents)
			continue;
		TArray<uint64> Targets;
		Snapshot.EntityRisk.GetKeys(Targets);
		Targets.Sort();
		const uint64 Target = Targets[(NextRandom() >> 16) % Targets.Num()];
		const int32 Roll = (NextRandom() >> 16) % 100;
		if (Roll >= Snapshot.EntityRisk.FindChecked(Target))
		{
			if (Roll < Snapshot.BaseRisk)
				++Snapshot.PreventedIncidents;
			continue;
		}
		if (Snapshot.RecentIncidents.ContainsByPredicate(
		        [&](const auto& Incident)
		        { return Incident.EntityId == Target && Incident.EndStep > Snapshot.LastFixedStepId; }))
			continue;
		FCityCrimeIncident Incident;
		Incident.Id = ++Snapshot.TotalIncidents;
		Incident.EntityId = Target;
		Incident.StartStep = Snapshot.LastFixedStepId;
		Incident.EndStep = Incident.StartStep + IncidentDurationSteps;
		Snapshot.RecentIncidents.Add(Incident);
		if (Snapshot.RecentIncidents.Num() > MaximumRecentIncidents)
			Snapshot.RecentIncidents.RemoveAt(0);
		Refresh(City, Services, Development, Tuning, bUpkeepShortfall);
	}
}

bool FCityCrimeSimulation::StartTest(const uint64 EntityId, FString& OutStatus)
{
	int32 Active = 0;
	for (const auto& I : Snapshot.RecentIncidents)
		Active += I.EndStep > Snapshot.LastFixedStepId ? 1 : 0;
	if (!EntityId || Snapshot.RecentIncidents.ContainsByPredicate(
	                     [&](const auto& I) { return I.EntityId == EntityId && I.EndStep > Snapshot.LastFixedStepId; }))
	{
		OutStatus = TEXT("Choose a building without an active crime incident");
		return false;
	}
	if (Active >= MaximumActiveIncidents)
	{
		OutStatus = TEXT("Crime test limit reached; wait for current responses");
		return false;
	}
	FCityCrimeIncident I;
	I.Id = ++Snapshot.TotalIncidents;
	I.EntityId = EntityId;
	I.StartStep = Snapshot.LastFixedStepId;
	I.EndStep = I.StartStep + IncidentDurationSteps;
	I.bTest = true;
	Snapshot.RecentIncidents.Add(I);
	if (Snapshot.RecentIncidents.Num() > MaximumRecentIncidents)
		Snapshot.RecentIncidents.RemoveAt(0);
	OutStatus = TEXT("Test crime started; police use normal road response");
	return true;
}

bool FCityCrimeSimulation::ValidateState(const FCityCrimeSnapshot& State, const int64 MaximumStep, FString& OutError)
{
	if (!State.Dimensions.IsValid() || State.LastFixedStepId < 0 || State.LastFixedStepId > MaximumStep ||
	    State.LastFixedStepId > MAX_int64 - 100000 || State.RandomState == 0 || State.PreviousHousedResidents < 0 ||
	    State.PreviousHousedResidents > MaximumResidents || State.HomelessResidents < 0 ||
	    State.HomelessResidents > MaximumResidents || State.ActivationStep < -1 ||
	    State.ActivationStep > State.LastFixedStepId ||
	    (State.ActivationStep == -1
	         ? State.GraceEndStep != -1
	         : State.GraceEndStep < State.ActivationStep || State.GraceEndStep - State.ActivationStep > 100000) ||
	    State.LastArsonStep < -1 || State.LastArsonStep > State.LastFixedStepId || State.TotalArsons < 0 ||
	    State.TotalArsons > State.TotalIncidents || State.TotalArrests < 0 || State.TotalArrests > State.TotalArsons ||
	    State.TotalIncidents < 0 || State.TotalIncidents > MAX_int64 - 1 || State.PreventedIncidents < 0 ||
	    State.PreventedIncidents > MAX_int64 - 1 || State.RecentIncidents.Num() > MaximumRecentIncidents)
	{
		OutError = TEXT("Invalid crime state");
		return false;
	}
	int64 PreviousId = 0;
	TSet<uint64> ActiveTargets;
	for (const auto& Incident : State.RecentIncidents)
	{
		if ((!Incident.bTest && !State.IsIntroduced()) || Incident.Id <= PreviousId ||
		    Incident.Id > State.TotalIncidents || Incident.EntityId == 0 ||
		    (!Incident.bTest && Incident.StartStep < State.GraceEndStep) || Incident.StartStep < 0 ||
		    Incident.StartStep > State.LastFixedStepId || Incident.StartStep > MAX_int64 - ArsonEscapeSteps ||
		    (Incident.EndStep <= Incident.StartStep ||
		     Incident.EndStep > Incident.StartStep + (Incident.bArson ? ArsonEscapeSteps : IncidentDurationSteps)) ||
		    static_cast<uint8>(Incident.ArsonOutcome) > 5 ||
		    (!Incident.bArson && (Incident.OffenderCitizenId || Incident.ArrestStationId ||
		                          Incident.CaptureStep != -1 || Incident.ArsonOutcome != ECityArsonOutcome::Pending)) ||
		    (Incident.CaptureStep != -1 &&
		     (Incident.CaptureStep < Incident.StartStep || Incident.CaptureStep > State.LastFixedStepId)) ||
		    ((Incident.ArsonOutcome == ECityArsonOutcome::Detained ||
		      Incident.ArsonOutcome == ECityArsonOutcome::Released ||
		      Incident.ArsonOutcome == ECityArsonOutcome::ReleasedEarly ||
		      Incident.ArsonOutcome == ECityArsonOutcome::JailFull) &&
		     (!Incident.OffenderCitizenId || !Incident.ArrestStationId || Incident.CaptureStep < 0)))
		{
			OutError = TEXT("Invalid crime incident");
			return false;
		}
		if (Incident.EndStep > State.LastFixedStepId)
		{
			if (ActiveTargets.Contains(Incident.EntityId) || ActiveTargets.Num() >= MaximumActiveIncidents)
			{
				OutError = TEXT("Too many or duplicated active crime incidents");
				return false;
			}
			ActiveTargets.Add(Incident.EntityId);
		}
		PreviousId = Incident.Id;
	}
	return true;
}

int32 FCityCrimeSimulation::JailCapacity(const FCityDevelopmentSnapshot& Development, const uint64 StationId)
{
	const auto* State = Development.Find(StationId);
	if (State && State->Stage == ECityDevelopmentStage::Construction)
		return 0;
	return State && State->DensityLevel != ECityDensityLevel::Low ? 8 : 4;
}

const TCHAR* FCityCrimeSimulation::OutcomeName(const ECityArsonOutcome Outcome)
{
	switch (Outcome)
	{
	case ECityArsonOutcome::Escaped:
		return TEXT("Escaped");
	case ECityArsonOutcome::JailFull:
		return TEXT("Caught; jail full, released");
	case ECityArsonOutcome::Detained:
		return TEXT("Detained for reform");
	case ECityArsonOutcome::ReleasedEarly:
		return TEXT("Released: station lost before reform completed");
	case ECityArsonOutcome::Released:
		return TEXT("Released after reform");
	default:
		return TEXT("Offender at large; prompt police response can catch them");
	}
}

bool FCityCrimeSimulation::LinkArson(const uint64 EntityId, const FCityPopulationSnapshot& Population,
                                     FCityFireSimulation& Fire)
{
	auto* Incident = Snapshot.RecentIncidents.FindByPredicate(
	    [&](const auto& I) { return I.EntityId == EntityId && I.EndStep > Snapshot.LastFixedStepId && !I.bArson; });
	if (!Incident || !Fire.GetSnapshot().IsBurning(EntityId))
		return false;
	TArray<uint64> Eligible;
	for (const auto& Citizen : Population.Citizens)
	{
		if (Citizen.IsDetained() || Citizen.ReformedUntilStep > Snapshot.LastFixedStepId)
			continue;
		if (Snapshot.RecentIncidents.ContainsByPredicate(
		        [&](const auto& I)
		        {
			        return I.OffenderCitizenId == Citizen.Id && I.ArsonOutcome == ECityArsonOutcome::Pending &&
			               I.EndStep > Snapshot.LastFixedStepId;
		        }))
			continue;
		Eligible.Add(Citizen.Id);
	}
	Eligible.Sort();
	if (Eligible.IsEmpty() && !Incident->bTest)
		return false;
	Incident->bArson = true;
	Incident->OffenderCitizenId = Eligible.IsEmpty() ? 0 : Eligible[(NextRandom() >> 16) % Eligible.Num()];
	Incident->EndStep = Incident->StartStep + ArsonEscapeSteps;
	Snapshot.LastArsonStep = Incident->StartStep;
	++Snapshot.TotalArsons;
	Fire.MarkArson(EntityId, Incident->OffenderCitizenId);
	return true;
}

void FCityCrimeSimulation::RefreshCustody(const FCityPlacementModel& City, const FCityEconomyTuning& Tuning,
                                          FCityPopulationSimulation& Population, const FCityFireSimulation& Fire)
{
	const int64 Step = Snapshot.LastFixedStepId;
	const int64 Month = FMath::Max(1, Tuning.FixedStepsPerMonth);
	// Release survives incident-history eviction. Station loss releases safely; outages retain custody.
	for (auto& Citizen : Population.Snapshot.Citizens)
	{
		if (!Citizen.IsDetained())
			continue;
		const auto* Damage = Fire.GetSnapshot().DamagedBuildings.Find(Citizen.JailStationId);
		if (Citizen.JailReleaseStep > Step && City.GetEntities().Contains(Citizen.JailStationId) &&
		    (!Damage || Damage->Damage < 100))
			continue;
		const bool Reformed = Citizen.JailReleaseStep <= Step;
		Citizen.JailStationId = 0;
		Citizen.JailReleaseStep = -1;
		if (Reformed)
			Citizen.ReformedUntilStep = Step + 6 * Month;
		Citizen.UnemployedSinceStep = Step;
		Citizen.CurrentLocationEntityId = Citizen.HomeEntityId;
		Citizen.Schedule = ECityCitizenSchedule::Home;
		Citizen.NextActionStep = Step + 1;
		for (auto& I : Snapshot.RecentIncidents)
			if (I.OffenderCitizenId == Citizen.Id && I.ArsonOutcome == ECityArsonOutcome::Detained)
				I.ArsonOutcome = Reformed ? ECityArsonOutcome::Released : ECityArsonOutcome::ReleasedEarly;
	}
}

void FCityCrimeSimulation::AdvanceArson(const FCityPlacementModel& City, const FCityDevelopmentSnapshot& Development,
                                        const FCityEconomyTuning& Tuning, const FCityServiceSnapshot& Services,
                                        const bool bUnpaid, FCityPopulationSimulation& Population,
                                        FCityFireSimulation& Fire)
{
	const int64 Step = Snapshot.LastFixedStepId;
	const int64 Month = FMath::Max(1, Tuning.FixedStepsPerMonth);
	RefreshCustody(City, Tuning, Population, Fire);
	for (auto& Incident : Snapshot.RecentIncidents)
	{
		// A limited subset of newly committed crimes escalates, with a three-month city cooldown.
		if (!Incident.bTest && !Incident.bArson && Incident.StartStep == Step &&
		    (Snapshot.LastArsonStep < 0 || Step - Snapshot.LastArsonStep >= 3 * Month) &&
		    static_cast<int32>((NextRandom() >> 16) % 100) < FMath::Clamp(Snapshot.BaseRisk / 4, 2, 25))
		{
			const auto* Target = City.GetEntities().Find(Incident.EntityId);
			const bool HasOffender = Population.Snapshot.Citizens.ContainsByPredicate(
			    [&](const auto& C) { return !C.IsDetained() && C.ReformedUntilStep <= Step; });
			FString Status;
			auto CandidateFire = Fire;
			if (Target && HasOffender && Development.IsOperational(Incident.EntityId) &&
			    FCityFireSimulation::IsFireTarget(Target->DefinitionId, Tuning) &&
			    CandidateFire.StartArsonIgnition(Incident.EntityId, Status))
			{
				// StartTest validates the shared fire caps; this ignition is a real event.
				if (LinkArson(Incident.EntityId, Population.GetSnapshot(), CandidateFire))
					Fire = MoveTemp(CandidateFire);
			}
		}
		if (!Incident.bArson || Incident.ArsonOutcome != ECityArsonOutcome::Pending)
			continue;
		auto* Citizen = Population.Snapshot.Citizens.FindByPredicate([&](const auto& C)
		                                                             { return C.Id == Incident.OffenderCitizenId; });
		if (!Citizen || Citizen->IsDetained())
		{
			if (Step >= Incident.EndStep)
				Incident.ArsonOutcome = ECityArsonOutcome::Escaped;
			continue;
		}
		const auto* Response = Population.Snapshot.TransitTrips.FindByPredicate(
		    [&](const auto& T)
		    { return T.Purpose == ECityTripPurpose::PoliceResponse && T.IncidentId == Incident.Id && T.bCompleted; });
		if (!Response || bUnpaid || Services.GetFailure(Response->StationEntityId) != ECityOperationFailure::None ||
		    Services.PublicStaffPercent.FindRef(Response->StationEntityId) <= 0)
		{
			if (Step >= Incident.StartStep + ArsonEscapeSteps)
				Incident.ArsonOutcome = ECityArsonOutcome::Escaped;
			continue;
		}
		if (Step >= Incident.StartStep + ArsonEscapeSteps || (NextRandom() >> 16) % 100 >= 75)
		{
			Incident.ArsonOutcome = ECityArsonOutcome::Escaped;
			continue;
		}
		Incident.ArrestStationId = Response->StationEntityId;
		Incident.CaptureStep = Step;
		++Snapshot.TotalArrests;
		const int32 Occupied =
		    Population.Snapshot.Citizens
		        .FilterByPredicate([&](const auto& C) { return C.JailStationId == Response->StationEntityId; })
		        .Num();
		if (Occupied >= JailCapacity(Development, Response->StationEntityId))
		{
			Incident.ArsonOutcome = ECityArsonOutcome::JailFull;
			continue;
		}
		Incident.ArsonOutcome = ECityArsonOutcome::Detained;
		Citizen->JailStationId = Response->StationEntityId;
		Citizen->JailReleaseStep = Step + 3 * Month;
		Citizen->WorkplaceEntityId = 0;
		Citizen->bSkilledJob = false;
		Citizen->UnemployedSinceStep = -1;
		Citizen->TripPurpose = ECityTripPurpose::None;
		Citizen->TripDestinationEntityId = Citizen->PendingShopEntityId = 0;
		Citizen->CurrentLocationEntityId = Citizen->JailStationId;
		Citizen->ConsumptionNeed = 0;
		Citizen->Schedule = ECityCitizenSchedule::Home;
		Citizen->Activity = ECityCitizenActivity::Home;
	}
}

bool FCityCrimeSimulation::RestoreState(const FCityCrimeSnapshot& State, const int64 MaximumStep, FString& OutError)
{
	if (!ValidateState(State, MaximumStep, OutError))
		return false;
	Snapshot = State;
	return true;
}

#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityTechnology.h"
#include "Simulation/CityCommercialAi.h"
#include "Simulation/CityHappinessSimulation.h"
#include "Simulation/CityHousingMigration.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Performance/CityEvaluationTiming.h"

#include "Commands/CityPlacementModel.h"
#include "Simulation/CityEconomySimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityDevelopmentSimulation.h"

FCityWorkforceEducationSnapshot FCityPopulationSnapshot::BuildWorkforceEducation() const
{
	FCityWorkforceEducationSnapshot Result;
	for (const FCityCitizenRecord& Citizen : Citizens)
	{
		if (Citizen.WorkplaceEntityId == 0)
			continue;
		switch (Citizen.Education)
		{
		case ECityEducationLevel::None:
			++Result.Uneducated;
			break;
		case ECityEducationLevel::Basic:
			++Result.Basic;
			break;
		case ECityEducationLevel::Secondary:
			++Result.Secondary;
			break;
		case ECityEducationLevel::Higher:
			++Result.Higher;
			break;
		}
	}
	return Result;
}

int32 FCityPopulationSnapshot::Workers(const uint64 EntityId) const
{
	int32 Count = 0;
	for (const auto& Citizen : Citizens)
		Count += Citizen.WorkplaceEntityId == EntityId ? 1 : 0;
	return Count;
}
int32 FCityPopulationSnapshot::ActiveWorkers(const uint64 EntityId) const
{
	int32 Count = 0;
	for (const auto& C : Citizens)
		Count += C.WorkplaceEntityId == EntityId && !C.bOnMedicalLeave && !C.IsDetained() ? 1 : 0;
	return Count;
}
int32 FCityPopulationSnapshot::StaffPercent(const uint64 EntityId, const int32 RequiredJobs) const
{
	const auto* Employer =
	    PublicEmployers.FindByPredicate([&](const auto& E) { return E.EntityId == EntityId; });
	if (!Employer || Employer->JobCapacity <= 0)
		return 0;
	const int32 Staffing = FMath::Min(100, ActiveWorkers(EntityId) * 100 /
	                                           (RequiredJobs > 0 ? RequiredJobs : Employer->JobCapacity));
	return Employer->PayrollDue > 0
	           ? static_cast<int32>(Staffing * Employer->PayrollPaid / Employer->PayrollDue)
	           : Staffing;
}
int32 FCityPopulationSimulation::PublicJobs(const FName Id, const FCityEconomyTuning& Tuning)
{
	if (FCityTechnology::IsFacility(Id))
		return Id == TEXT("DigitalExchange") ? 4 : 6;
	if (Id == TEXT("College"))
		return 6;
	if (Id == TEXT("WasteDepot") || Id == TEXT("Recycling"))
		return 4;
	if (Id == TEXT("Hospital"))
		return 6; // Starter fallback; catalog stages author actual vacancies.
	if (FCityBuildingDefinition::IsSchoolFacility(Id))
		return FMath::Clamp(Tuning.SchoolStaff, 1, 20);
	if (Id == TEXT("Police") || Id == TEXT("Fire"))
		return FMath::Clamp(Tuning.DepartmentStaff, 1, 20);
	if (Id == Tuning.PowerDefinitionId || Id == Tuning.WaterTreatmentDefinitionId ||
	    FCityBuildingDefinition::IsAlternativeUtility(Id))
		return FMath::Clamp(Tuning.UtilityStaff, 1, 20);
	return 0;
}

bool FCityPopulationSimulation::IsHome(const FName DefinitionId)
{
	return DefinitionId == TEXT("Home");
}
bool FCityPopulationSimulation::IsBusiness(const FName DefinitionId)
{
	return DefinitionId == TEXT("Shop") || DefinitionId == TEXT("Factory");
}

void FCityPopulationSimulation::Reset()
{
	Snapshot = FCityPopulationSnapshot();
}

void FCityPopulationSimulation::Reconcile(const FCityPlacementModel& City,
                                          const FCityEconomySnapshot& Economy,
                                          const FCityEconomyTuning& Tuning,
                                          const FCityServiceSnapshot* Services, const bool bPrepareQueries)
{
	double PhaseStart = ForecastTiming ? FPlatformTime::Seconds() : 0;
	TArray<uint64> Homes;
	TArray<uint64> Businesses;
	for (const TPair<uint64, FCityPlacedEntity>& Pair : City.GetEntities())
	{
		if (IsHome(Pair.Value.DefinitionId))
			Homes.Add(Pair.Key);
		else if (IsBusiness(Pair.Value.DefinitionId))
			Businesses.Add(Pair.Key);
	}
	Homes.Sort();
	Businesses.Sort();

	TMap<uint64, FCityHouseholdRecord> ExistingHouseholds;
	for (const auto& Record : Snapshot.Households)
		ExistingHouseholds.Add(Record.Id, Record);
	TMap<uint64, FCityBusinessLedgerRecord> ExistingBusinesses;
	for (const auto& Record : Snapshot.Businesses)
		ExistingBusinesses.Add(Record.EntityId, Record);
	for (const auto& Business : Snapshot.Businesses)
		if (!City.GetEntities().Contains(Business.EntityId))
			Snapshot.DiscardedGoods += Business.Inventory;
	Snapshot.Businesses.Reset();
	Snapshot.Households.Reset();

	const auto Admissions =
	    Services ? FCityHousingMigration::AdmissionPlan(Snapshot, *Services) : FCityHousingAdmissionPlan();
	TMap<uint64, int32> Vacancies;
	for (const uint64 HomeId : Homes)
		Vacancies.Add(HomeId, Services ? (Admissions.ReservedHomes.Contains(HomeId)
		                                      ? 0
		                                      : Services->EntityResidents.FindRef(HomeId))
		                               : FMath::Max(1, Tuning.ResidentsPerOccupiedHome));
	// Keep lifetime identities and wallets. New capacity admits newcomers only
	// after accounting for every existing citizen, including homeless citizens.
	const int32 TargetResidents =
	    FMath::Max(Snapshot.Citizens.Num(),
	               Services ? FMath::Min(Admissions.TargetResidents, Economy.Residents) : Economy.Residents);
	while (Snapshot.Citizens.Num() < TargetResidents &&
	       Snapshot.Citizens.Num() < City.GetGrid().GetDimensions().GetCellCount() * 128)
	{
		auto& Citizen = Snapshot.Citizens.Emplace_GetRef();
		Citizen.Id = Snapshot.NextCitizenId++;
		const uint64 Percent = FMath::Clamp(Tuning.NewArrivalUneducatedPercent, 0, 100);
		// Evenly spread the quota over 100 arrivals; no extra random/save state.
		const uint64 Carry = ((Citizen.Id - 1) % 100) * Percent % 100;
		Citizen.Education = Carry + Percent >= 100 ? ECityEducationLevel::None : ECityEducationLevel::Basic;
		Citizen.NextActionStep = FCityCalendar::NextMorning(Economy.FixedStepId, Citizen.Id, Tuning.FixedStepsPerMonth);
		Citizen.UnemployedSinceStep = Economy.FixedStepId;
		Citizen.LastEducationMonth = Economy.LastBilledMonthId;
		Citizen.LastHappinessMonth = Economy.LastBilledMonthId;
	}
	Snapshot.Citizens.Sort([](const auto& A, const auto& B) { return A.Id < B.Id; });

	TMap<uint64, FCityPublicEmployerRecord> OldPublic;
	for (const auto& Employer : Snapshot.PublicEmployers)
		OldPublic.Add(Employer.EntityId, Employer);
	Snapshot.PublicEmployers.Reset();
	if (Services)
	{
		TArray<uint64> PublicIds;
		Services->PublicJobs.GetKeys(PublicIds);
		PublicIds.Sort();
		for (const uint64 Id : PublicIds)
		{
			auto Employer = OldPublic.FindRef(Id);
			Employer.EntityId = Id;
			Employer.JobCapacity = Services->PublicJobs.FindRef(Id);
			Snapshot.PublicEmployers.Add(Employer);
		}
	}
	int32 PublicCapacity = 0;
	for (const auto& E : Snapshot.PublicEmployers)
		PublicCapacity += E.JobCapacity;
	int32 RemainingJobs = FMath::Max(0, Economy.Jobs - PublicCapacity);
	int32 RemainingCommercial = FMath::Max(0, Economy.RetailCapacity);
	int32 RemainingIndustrial = FMath::Max(0, Economy.GoodsProduced);
	for (const uint64 BusinessId : Businesses)
	{
		const FCityPlacedEntity* Entity = City.GetEntities().Find(BusinessId);
		const bool bIndustrial = Entity && Entity->DefinitionId == TEXT("Factory");
		FCityBusinessLedgerRecord Business = ExistingBusinesses.FindRef(BusinessId);
		Business.EntityId = BusinessId;
		Business.bIndustrial = bIndustrial;
		Business.JobCapacity =
		    FMath::Min(RemainingJobs, Services ? Services->EntityJobs.FindRef(BusinessId)
		                                       : (bIndustrial ? Tuning.JobsPerOperationalIndustry
		                                                      : Tuning.JobsPerOperationalShop));
		RemainingJobs -= Business.JobCapacity;
		Business.SkilledMinimumEducation = Services && Services->EntitySkilledMinimumEducation.Contains(BusinessId)
		    ? Services->EntitySkilledMinimumEducation.FindRef(BusinessId) : 2;
		Business.SkilledJobCapacity =
		    Services ? FMath::Clamp(Services->EntitySkilledJobs.FindRef(BusinessId), 0, Business.JobCapacity)
		             : 0;
		// Economy values are city-wide totals. Assign the remaining total once
		// across the matching ledgers instead of copying it into every business.
		// Copying the aggregate made multi-shop/factory cities multiply their
		// recorded capacity and output, breaking the reconciliation invariant.
		Business.PendingOrders = Services ? Services->EntityOutput.FindRef(BusinessId)
		                                  : (bIndustrial ? RemainingIndustrial : RemainingCommercial);
		if (bIndustrial)
		{
			RemainingIndustrial -= Business.PendingOrders;
		}
		else
		{
			RemainingCommercial -= Business.PendingOrders;
		}
		if (Business.OpeningPayrollReserve == 0)
		{
			Business.OpeningPayrollReserve =
			    static_cast<int64>(Business.JobCapacity) * Tuning.HouseholdIncomePerEmployedResident;
			Business.Balance += Business.OpeningPayrollReserve;
		}
		Snapshot.Businesses.Add(Business);
	}

	// Civic vacancies have hiring priority. Retain civic workers first, then
	// offer stable-ID transfers from private jobs; Basic education supports all starters.
	if (ForecastTiming)
	{
		ForecastTiming->ReconcileLedger.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	TMap<uint64, int32> CivicVacancies;
	for (const auto& E : Snapshot.PublicEmployers)
		CivicVacancies.Add(E.EntityId, E.JobCapacity);
	TSet<uint64> PublicWorkers;
	for (auto& Citizen : Snapshot.Citizens)
	{
		if (Citizen.IsDetained())
			continue;
		int32* Slots = CivicVacancies.Find(Citizen.WorkplaceEntityId);
		if (Slots && *Slots > 0 &&
		    CanFillJob(Citizen.Education,
		               static_cast<ECityEducationLevel>(
		                   Services && Services->PublicMinimumEducation.Contains(Citizen.WorkplaceEntityId)
		                       ? Services->PublicMinimumEducation.FindRef(Citizen.WorkplaceEntityId)
		                       : 1)))
		{
			--*Slots;
			PublicWorkers.Add(Citizen.Id);
		}
		else if (Slots ||
		         (Citizen.WorkplaceEntityId && !City.GetEntities().Contains(Citizen.WorkplaceEntityId)))
			Citizen.WorkplaceEntityId = 0;
	}
	// After retention, civic slots only decrease. Each qualification can skip
	// exhausted/ineligible employers without changing the original tie order.
	int32 CivicCursor[4] = {};
	const auto FindCivicVacancy = [&](const ECityEducationLevel Education) {
		if (static_cast<uint8>(Education) > 3)
			return uint64(0);
		const int32 Tier = FMath::Min<int32>(3, static_cast<uint8>(Education));
		for (int32 Index = bPrepareQueries ? CivicCursor[Tier] : 0; Index < Snapshot.PublicEmployers.Num();
		     ++Index)
		{
			const auto& Employer = Snapshot.PublicEmployers[Index];
			if (bPrepareQueries)
				CivicCursor[Tier] = Index + 1;
			if (CivicVacancies.FindRef(Employer.EntityId) > 0 &&
			    CanFillJob(Education,
			               static_cast<ECityEducationLevel>(
			                   Services && Services->PublicMinimumEducation.Contains(Employer.EntityId)
			                       ? Services->PublicMinimumEducation.FindRef(Employer.EntityId)
			                       : 1)))
			{
				if (bPrepareQueries)
					CivicCursor[Tier] = Index;
				return Employer.EntityId;
			}
		}
		return uint64(0);
	};
	for (auto& Citizen : Snapshot.Citizens)
	{
		if (Citizen.IsDetained() || Citizen.bOnMedicalLeave)
			continue;
		if (PublicWorkers.Contains(Citizen.Id))
			continue;
		if (const uint64 EmployerId = FindCivicVacancy(Citizen.Education))
		{
			--CivicVacancies.FindChecked(EmployerId);
			Citizen.WorkplaceEntityId = EmployerId;
			PublicWorkers.Add(Citizen.Id);
		}
	}
	TMap<uint64, int32> Skilled, Entry;
	for (const auto& B : Snapshot.Businesses)
	{
		Skilled.Add(B.EntityId, B.SkilledJobCapacity);
		Entry.Add(B.EntityId, B.JobCapacity - B.SkilledJobCapacity);
	}
	// Skilled vacancies only decrease during this pass. Entry vacancies may
	// increase while skilled transfers happen, so their cursor starts afterward.
	const auto FindBusinessVacancy = [&](const TMap<uint64, int32>& Slots, int32& Cursor,
	                                     ECityEducationLevel Education, bool SkilledRole) {
		for (int32 Index = bPrepareQueries ? Cursor : 0; Index < Snapshot.Businesses.Num(); ++Index)
		{
			if (bPrepareQueries)
				Cursor = Index + 1;
			const uint64 Id = Snapshot.Businesses[Index].EntityId;
			if (Slots.FindRef(Id) > 0 && (!SkilledRole ||
			    static_cast<int32>(Education) >= Snapshot.Businesses[Index].SkilledMinimumEducation))
			{
				if (bPrepareQueries)
					Cursor = Index;
				return Id;
			}
		}
		return uint64(0);
	};
	int32 SkilledCursors[4] = {};
	for (auto& C : Snapshot.Citizens)
	{
		if (C.IsDetained())
			continue;
		if (PublicWorkers.Contains(C.Id))
		{
			C.bSkilledJob = false;
			continue;
		}
		const auto* Employer = Snapshot.Businesses.FindByPredicate([&](const auto& B) { return B.EntityId == C.WorkplaceEntityId; });
		int32* Slots = (C.bSkilledJob ? Skilled : Entry).Find(C.WorkplaceEntityId);
		if (Slots && *Slots > 0 &&
		    CanFillJob(C.Education,
		               C.bSkilledJob && Employer ? static_cast<ECityEducationLevel>(Employer->SkilledMinimumEducation) : ECityEducationLevel::None))
			--*Slots;
		else
		{
			C.WorkplaceEntityId = 0;
			C.bSkilledJob = false;
		}
	}
	for (auto& C : Snapshot.Citizens)
	{
		if (C.IsDetained())
			continue;
		if (C.bOnMedicalLeave || PublicWorkers.Contains(C.Id) || C.bSkilledJob ||
		    !CanFillJob(C.Education, ECityEducationLevel::Secondary))
			continue;
		if (const uint64 Id = FindBusinessVacancy(Skilled, SkilledCursors[static_cast<int32>(C.Education)], C.Education, true))
		{
			if (C.WorkplaceEntityId && Entry.Contains(C.WorkplaceEntityId))
				++Entry.FindChecked(C.WorkplaceEntityId);
			C.WorkplaceEntityId = Id;
			C.bSkilledJob = true;
			--Skilled.FindChecked(Id);
		}
	}
	int32 EntryCursor = 0;
	for (auto& C : Snapshot.Citizens)
	{
		if (C.IsDetained())
			continue;
		if (!C.bOnMedicalLeave && !C.WorkplaceEntityId)
			if (const uint64 Id = FindBusinessVacancy(Entry, EntryCursor, C.Education, false))
			{
				C.WorkplaceEntityId = Id;
				--Entry.FindChecked(Id);
			}
		if (C.WorkplaceEntityId)
			C.UnemployedSinceStep = -1;
		else if (C.UnemployedSinceStep < 0)
			C.UnemployedSinceStep = Economy.FixedStepId;
	}
	if (ForecastTiming)
	{
		ForecastTiming->ReconcileHiring.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	for (auto& Citizen : Snapshot.Citizens)
	{
		if (Citizen.IsDetained())
		{
			if (auto* Slots = Vacancies.Find(Citizen.HomeEntityId))
				*Slots = FMath::Max(0, *Slots - 1);
			else
				Citizen.HomeEntityId = Citizen.HouseholdId = 0;
			continue;
		}
		int32* Available = Vacancies.Find(Citizen.HomeEntityId);
		// Security is assessed from actual needs. Neither a lost job nor a temporary
		// utility outage erases an existing physical home; migration owns hardship.
		if (Available && Citizen.HomeEntityId != 0)
			*Available = FMath::Max(0, *Available - 1);
		else
		{
			// Demolition removes the physical home, preserving the citizen and wallet.
			Citizen.HomeEntityId = Citizen.HouseholdId = 0;
			Citizen.CurrentLocationEntityId = Citizen.TripDestinationEntityId = Citizen.PendingShopEntityId =
			    0;
			Citizen.TripPurpose = ECityTripPurpose::None;
			Citizen.Activity = ECityCitizenActivity::Home;
			Citizen.Schedule = ECityCitizenSchedule::Home;
			Citizen.bShopBeforeWork = false;
		}
	}
	TSet<uint64> CreatedHouseholds; // Request-local membership; no cross-step state.
	int32 HomeCursor = 0;
	int32 ResidentIndex = 0;
	for (auto& Citizen : Snapshot.Citizens)
	{
		if (Citizen.IsHomeless() && !Citizen.IsDetained())
			for (int32 Index = bPrepareQueries ? HomeCursor : 0; Index < Homes.Num(); ++Index)
			{
				const uint64 HomeId = Homes[Index];
				if (bPrepareQueries)
					HomeCursor = Index + 1;
				if (Vacancies.FindRef(HomeId) > 0)
				{
					if (bPrepareQueries)
						HomeCursor = Index;
					--Vacancies.FindChecked(HomeId);
					Citizen.HomeEntityId = Citizen.HouseholdId = HomeId;
					Citizen.CurrentLocationEntityId = HomeId;
					Citizen.NextActionStep = FCityCalendar::NextMorning(Economy.FixedStepId, Citizen.Id, Tuning.FixedStepsPerMonth);
					break;
				}
			}
		Citizen.ConsumptionNeed = Citizen.IsDetained()                                                 ? 0
		                          : ResidentIndex++ % FMath::Max(1, Tuning.ResidentsPerGoodsUnit) == 0 ? 1
		                                                                                               : 0;
		if (Citizen.HomeEntityId &&
		    (bPrepareQueries ? !CreatedHouseholds.Contains(Citizen.HomeEntityId)
		                     : !Snapshot.Households.ContainsByPredicate(
		                           [&](const auto& H) { return H.Id == Citizen.HomeEntityId; })))
		{
			auto Household = ExistingHouseholds.FindRef(Citizen.HomeEntityId);
			Household.Id = Household.HomeEntityId = Citizen.HomeEntityId;
			Snapshot.Households.Add(Household);
			if (bPrepareQueries)
				CreatedHouseholds.Add(Citizen.HomeEntityId);
		}
	}
	Snapshot.Households.Sort([](const auto& A, const auto& B) { return A.Id < B.Id; });
	RefreshHouseholdSupplies(Economy.LastBilledMonthId);
	for (auto& B : Snapshot.Businesses)
		B.bHasReceivedGoods |= B.Inventory > 0;

	PrunePurchaseReservations(City);
	if (Snapshot.bUsesPurchaseReservations)
		RefreshHouseholdBalances();
	if (ForecastTiming)
		ForecastTiming->ReconcileHousing.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
}

void FCityPopulationSimulation::QueueConstructionArrival(const uint64 DestinationEntityId,
                                                         const int64 FixedStepId)
{
	if (DestinationEntityId == 0)
		return;
	for (const FCityTransitTripRecord& Trip : Snapshot.TransitTrips)
		if (!Trip.bCompleted && Trip.Purpose == ECityTripPurpose::MoveInArrival &&
		    Trip.DestinationEntityId == DestinationEntityId)
			return;
	FCityTransitTripRecord& Trip = Snapshot.TransitTrips.Emplace_GetRef();
	Trip.Id = Snapshot.NextTransitTripId++;
	Trip.DestinationEntityId = DestinationEntityId;
	Trip.Purpose = ECityTripPurpose::MoveInArrival;
	Trip.VehicleType = ECityTripVehicleType::FreightTruck;
	Trip.StartStep = FixedStepId;
	// This is deliberately long enough to read in-headset.  The board maps the
	// fixed-step interval across the entire road route, so it reaches the lot
	// instead of cycling through only the first few road cells.
	Trip.ArrivalStep = FixedStepId + ConstructionArrivalSteps;
	UE_LOG(LogTemp, Display, TEXT("CITYSIM_TRANSIT_QUEUED Id=%llu Destination=%llu Start=%lld Arrival=%lld"),
	       Trip.Id, Trip.DestinationEntityId, Trip.StartStep, Trip.ArrivalStep);
}

void FCityPopulationSimulation::AdvanceTransitToStep(const int64 FixedStepId)
{
	for (FCityTransitTripRecord& Trip : Snapshot.TransitTrips)
		if (!Trip.bCompleted && Trip.Purpose == ECityTripPurpose::MoveInArrival &&
		    FixedStepId >= Trip.ArrivalStep)
			Trip.bCompleted = true;
}

void FCityPopulationSimulation::CompleteTransitTrip(const uint64 TripId, const int64 FixedStepId)
{
	for (FCityTransitTripRecord& Trip : Snapshot.TransitTrips)
	{
		if (Trip.Id != TripId || Trip.bCompleted)
			continue;
		Trip.bCompleted = true;
		Trip.ArrivalStep = FMath::Max(Trip.StartStep, FixedStepId);
		if (Trip.Purpose == ECityTripPurpose::OpeningDelivery)
		{
			for (FCityBusinessLedgerRecord& Business : Snapshot.Businesses)
				if (Business.EntityId == Trip.DestinationEntityId)
					Business.bInitialFreightReceived = true;
		}
		if (Trip.Purpose == ECityTripPurpose::FreightDelivery ||
		    Trip.Purpose == ECityTripPurpose::FreightExport)
		{
			FCityBusinessLedgerRecord* Origin = Snapshot.Businesses.FindByPredicate(
			    [&](const FCityBusinessLedgerRecord& B) { return B.EntityId == Trip.OriginEntityId; });
			FCityBusinessLedgerRecord* Destination = Snapshot.Businesses.FindByPredicate(
			    [&](const FCityBusinessLedgerRecord& B) { return B.EntityId == Trip.DestinationEntityId; });
			if (Origin && Trip.Purpose == ECityTripPurpose::FreightExport)
			{
				Origin->ExportedGoods += Trip.Quantity;
				Snapshot.ExportsThisMonth += Trip.Quantity;
				Origin->Balance +=
				    Trip.Quantity * FreightUnitPrice; // External buyer pays on arrival at the entrance.
			}
			else if (Origin && Destination)
			{
				const int32 Accepted = FMath::Min(Trip.Quantity, MAX_int32 - Destination->Inventory);
				Destination->Inventory += Accepted;
				Destination->bHasReceivedGoods |= Accepted > 0;
				const int64 Payment = Accepted * FreightUnitPrice;
				Destination->Balance -= Payment;
				Origin->Balance += Payment;
				Snapshot.DiscardedGoods += Trip.Quantity - Accepted; // Explicit warehouse overflow.
			}
			else
				Snapshot.DiscardedGoods += Trip.Quantity;
		}
		return;
	}
}

void FCityPopulationSimulation::QueueBusinessOpenings(const FCityDevelopmentSnapshot& Development,
	                                                      const int64 FixedStepId, const int32 StepsPerDay)
{
	// Reconcile keeps ledgers in entity-ID order, making trip IDs deterministic.
	for (FCityBusinessLedgerRecord& Business : Snapshot.Businesses)
	{
		if (Business.bInitialFreightReceived || !Development.IsOperational(Business.EntityId))
			continue;
		if (Business.OpeningGraceUntilStep < 0)
			Business.OpeningGraceUntilStep = FixedStepId + 3LL * FMath::Max(1, StepsPerDay);
		if (Snapshot.TransitTrips.ContainsByPredicate([&](const FCityTransitTripRecord& Trip) {
			    return Trip.DestinationEntityId == Business.EntityId &&
			           Trip.Purpose == ECityTripPurpose::OpeningDelivery;
		    }))
			continue;
		FCityTransitTripRecord& Trip = Snapshot.TransitTrips.Emplace_GetRef();
		Trip.Id = Snapshot.NextTransitTripId++;
		Trip.DestinationEntityId = Business.EntityId;
		Trip.Purpose = ECityTripPurpose::OpeningDelivery;
		Trip.VehicleType = ECityTripVehicleType::FreightTruck;
		Trip.Quantity = 1;
		Trip.StartStep = FixedStepId;
		Trip.ArrivalStep = FixedStepId; // Arrival is committed only by traffic movement.
	}
}

void FCityPopulationSimulation::MarkExistingBusinessesOpenForScenario()
{
	for (FCityBusinessLedgerRecord& Business : Snapshot.Businesses)
		Business.bInitialFreightReceived = true;
}

void FCityPopulationSimulation::PruneTransitTrips(const FCityPlacementModel& City)
{
	Snapshot.TransitTrips.RemoveAll([&](const FCityTransitTripRecord& Trip) {
		if (Trip.bCompleted && !Trip.IsWasteJourney() && !Trip.IsHealthJourney() &&
		    !(Trip.IsEmergency() && Trip.Purpose != ECityTripPurpose::EmergencyReturn))
			return true;
		const bool bOriginExists =
		    Trip.OriginEntityId == 0 || City.GetEntities().Contains(Trip.OriginEntityId);
		const bool bDestinationExists =
		    Trip.DestinationEntityId == 0 || City.GetEntities().Contains(Trip.DestinationEntityId);
		if (bOriginExists && bDestinationExists)
			return false;
		if (Trip.Purpose == ECityTripPurpose::FreightDelivery ||
		    Trip.Purpose == ECityTripPurpose::FreightExport)
		{
			FCityBusinessLedgerRecord* Origin =
			    bOriginExists ? Snapshot.Businesses.FindByPredicate([&](const FCityBusinessLedgerRecord& B) {
				    return B.EntityId == Trip.OriginEntityId;
			    })
			                  : nullptr;
			const int32 Returned = Origin ? FMath::Min(Trip.Quantity, MAX_int32 - Origin->Inventory) : 0;
			if (Origin)
				Origin->Inventory += Returned;
			Snapshot.DiscardedGoods += Trip.Quantity - Returned;
		}
		return true;
	});
}

void FCityPopulationSimulation::QueueFreight(const int64 FixedStepId)
{
	TMap<uint64, int32> Incoming;
	TSet<uint64> BusyFactories;
	for (const FCityTransitTripRecord& Trip : Snapshot.TransitTrips)
	{
		if (Trip.bCompleted)
			continue;
		if (Trip.Purpose == ECityTripPurpose::FreightDelivery)
			Incoming.FindOrAdd(Trip.DestinationEntityId) += Trip.Quantity;
		if (Trip.Purpose == ECityTripPurpose::FreightDelivery ||
		    Trip.Purpose == ECityTripPurpose::FreightExport)
			BusyFactories.Add(Trip.OriginEntityId);
	}
	int32 Dispatches = 0;
	for (FCityBusinessLedgerRecord& Factory : Snapshot.Businesses)
	{
		if (Dispatches >= 16)
			break;
		if (!Factory.bIndustrial || !Factory.bInitialFreightReceived || Factory.PendingOrders <= 0 ||
		    Factory.Inventory <= 0 || BusyFactories.Contains(Factory.EntityId))
			continue;
		uint64 Destination = 0;
		int32 Quantity = FMath::Min(FreightLoadSize, Factory.Inventory);
		for (const FCityBusinessLedgerRecord& Shop : Snapshot.Businesses)
		{
			if (Shop.bIndustrial || !Shop.bInitialFreightReceived || Shop.PendingOrders <= 0)
				continue;
			const int64 Needed =
			    static_cast<int64>(Shop.PendingOrders) - Shop.Inventory - Incoming.FindRef(Shop.EntityId);
			if (Needed <= 0)
				continue;
			Destination = Shop.EntityId;
			Quantity = FMath::Min(Quantity, static_cast<int32>(Needed));
			Incoming.FindOrAdd(Destination) += Quantity;
			break;
		}
		FCityTransitTripRecord& Trip = Snapshot.TransitTrips.Emplace_GetRef();
		Trip.Id = Snapshot.NextTransitTripId++;
		Trip.OriginEntityId = Factory.EntityId;
		Trip.DestinationEntityId = Destination; // Zero is the existing city-entrance portal.
		Trip.Purpose = Destination ? ECityTripPurpose::FreightDelivery : ECityTripPurpose::FreightExport;
		Trip.VehicleType = ECityTripVehicleType::FreightTruck;
		Trip.Quantity = Quantity;
		Trip.StartStep = Trip.ArrivalStep = FixedStepId;
		Factory.Inventory -= Quantity; // Ownership moves into the saved cargo record at dispatch.
		++Dispatches;
	}
}

bool FCityPopulationSimulation::CanFillJob(const ECityEducationLevel Education,
                                           const ECityEducationLevel Required)
{
	return static_cast<uint8>(Education) <= 3 && static_cast<uint8>(Required) <= 3 && Education >= Required;
}

int64 FCityPopulationSimulation::MonthlySalaryLimit(const ECityEducationLevel Education,
                                                    const FCityEconomyTuning& Tuning)
{
	// Prototype salary ceilings in game currency; school progression is Education.
	const int32 Factors[] = {1, 2, 3, 4};
	return static_cast<int64>(FMath::Max(0, Tuning.HouseholdIncomePerEmployedResident)) *
	       Factors[FMath::Min<int32>(3, static_cast<uint8>(Education))];
}

int32 FCityPopulationSimulation::StaffedOutput(const int32 Capacity, const int32 Jobs, const int32 Workers)
{
	if (Capacity <= 0 || Jobs <= 0 || Workers <= 0)
		return 0;
	return static_cast<int32>(static_cast<int64>(Capacity) * FMath::Min(Workers, Jobs) / Jobs);
}

int32 FCityPopulationSimulation::ForecastGoodsSales(
    const FCityPlacementModel& City, const FCityEconomySnapshot& Economy, const FCityEconomyTuning& Tuning,
    const FCityServiceSnapshot& Services, const FCityPopulationSnapshot& Population, int64* OutPayrollPaid,
    int32* OutGoodsProduced, int64* OutPublicPayroll, int32* OutEmployed, FCityForecastTiming* Timing,
    const bool bPrepareQueries, int64* OutGoodsSpending)
{
	double PhaseStart = Timing ? FPlatformTime::Seconds() : 0;
	if (Timing)
	{
		Timing->PreparedQueries += bPrepareQueries ? 1 : 0;
		Timing->ReferenceQueries += bPrepareQueries ? 0 : 1;
	}
	// Use the same employment/capacity assignment as settlement, including newly occupied homes.
	FCityPopulationSimulation Forecast;
	Forecast.ForecastTiming = Timing;
	Forecast.Snapshot = Population;
	if (Timing)
	{
		Timing->Copy.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	Forecast.Reconcile(City, Economy, Tuning, &Services, bPrepareQueries);
	if (Timing)
	{
		Timing->Reconcile.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	// Settle a private copy so projected sales use exactly the same household,
	// stock and retail-capacity rules as the subsequent authoritative month.
	FCityEconomySnapshot Projection = Economy;
	Projection.GoodsSold = MAX_int32;
	Forecast.SettleMonth(Projection, Tuning, true, true, MAX_int64, bPrepareQueries);
	if (Timing)
	{
		Timing->Settlement.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	const FCityPopulationAggregate Aggregate = Forecast.BuildAggregate(Tuning);
	if (OutPayrollPaid)
		*OutPayrollPaid = Aggregate.HouseholdIncome;
	if (OutGoodsProduced)
		*OutGoodsProduced = Aggregate.GoodsProduced;
	if (OutEmployed)
		*OutEmployed = Aggregate.EmployedResidents;
	if (OutPublicPayroll)
	{
		*OutPublicPayroll = 0;
		for (const auto& E : Forecast.Snapshot.PublicEmployers)
			*OutPublicPayroll += E.PayrollDue;
	}
	if (Timing)
		Timing->Aggregate.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
	int32 ReservedGoods = 0;
	if (OutGoodsSpending) *OutGoodsSpending = 0;
	for (const auto& Order : Forecast.Snapshot.PurchaseReservations)
	{
		ReservedGoods += Order.Quantity;
		if (OutGoodsSpending) *OutGoodsSpending += Order.Price;
	}
	return ReservedGoods;
}

void FCityPopulationSimulation::AdvanceToMonth(const FCityPlacementModel& City,
                                               const FCityEconomySnapshot& Economy,
                                               const FCityEconomyTuning& Tuning,
                                               const FCityServiceSnapshot* Services,
                                               const bool bUseFreightInventory, const bool bReservePurchases,
                                               const bool bPrepareQueries, FCityForecastTiming* Timing)
{
	TGuardValue<FCityForecastTiming*> TimingScope(ForecastTiming, Timing);
	const double ReconcileStart = Timing ? FPlatformTime::Seconds() : 0;
	Reconcile(City, Economy, Tuning, Services, bPrepareQueries);
	if (Timing)
		Timing->Reconcile.Add((FPlatformTime::Seconds() - ReconcileStart) * 1000);
	Snapshot.bUsesPurchaseReservations = bReservePurchases;
	if (Economy.LastBilledMonthId <= Snapshot.LastSettledMonthId)
		return;
	const double SettlementStart = Timing ? FPlatformTime::Seconds() : 0;
	SettleMonth(Economy, Tuning, bUseFreightInventory, bReservePurchases, Economy.LastPublicPayrollPaid,
	            bPrepareQueries);
	if (Timing)
		Timing->Settlement.Add((FPlatformTime::Seconds() - SettlementStart) * 1000);
	Snapshot.LastSettledMonthId = Economy.LastBilledMonthId;
}

void FCityPopulationSimulation::SettleCommercialAi(const FCityServiceSnapshot& Services)
{
    FCityCommercialAi::Settle(Snapshot, &Services, Snapshot.LastSettledMonthId);
}

void FCityPopulationSimulation::SettleMonth(const FCityEconomySnapshot& Economy,
                                            const FCityEconomyTuning& Tuning, const bool bUseFreightInventory,
                                            const bool bReservePurchases, int64 PublicPayrollBudget,
                                            const bool bPrepareQueries)
{
	double PhaseStart = ForecastTiming ? FPlatformTime::Seconds() : 0;
	// Group once in existing citizen order. The lists die at the end of settlement;
	// qualifications, leave, detention, wallets and payroll remain live reads.
	TMap<uint64, TArray<int32>> EmployerCitizens;
	if (bPrepareQueries)
		for (int32 Index = 0; Index < Snapshot.Citizens.Num(); ++Index)
			EmployerCitizens.FindOrAdd(Snapshot.Citizens[Index].WorkplaceEntityId).Add(Index);
	const auto ForWorkers = [&](const uint64 EmployerId, auto&& Action) {
		if (bPrepareQueries)
		{
			if (const auto* Members = EmployerCitizens.Find(EmployerId))
				for (const int32 Index : *Members)
					Action(Snapshot.Citizens[Index]);
		}
		else
			for (auto& Citizen : Snapshot.Citizens)
				if (Citizen.WorkplaceEntityId == EmployerId)
					Action(Citizen);
	};
	Snapshot.bUsesPurchaseReservations = bReservePurchases;
	Snapshot.LastMonthExports = Snapshot.ExportsThisMonth;
	Snapshot.ExportsThisMonth = 0;
	if (bReservePurchases)
	{
		Snapshot.LastMonthSales = Snapshot.SalesThisMonth;
		Snapshot.LastMonthSpending = Snapshot.SpendingThisMonth;
		Snapshot.SalesThisMonth = 0;
		Snapshot.SpendingThisMonth = 0;
	}
	for (auto& Employer : Snapshot.PublicEmployers)
	{
		const auto* Members = EmployerCitizens.Find(Employer.EntityId);
		const int32 Count =
		    bPrepareQueries ? (Members ? Members->Num() : 0) : Snapshot.Workers(Employer.EntityId);
		Employer.PayrollDue =
		    static_cast<int64>(Count) * FMath::Max(0, Tuning.HouseholdIncomePerEmployedResident);
		Employer.PayrollPaid = FMath::Min(Employer.PayrollDue, PublicPayrollBudget);
		PublicPayrollBudget -= Employer.PayrollPaid;
		int32 WorkerIndex = 0;
		if (Count > 0)
			ForWorkers(Employer.EntityId, [&](auto& Citizen) {
				Citizen.AvailableMoney +=
				    Employer.PayrollPaid / Count + (WorkerIndex++ < Employer.PayrollPaid % Count ? 1 : 0);
			});
	}
	if (ForecastTiming)
	{
		ForecastTiming->PublicPayroll.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	TMap<uint64, FCityBusinessLedgerRecord*> Businesses;
	for (FCityBusinessLedgerRecord& Business : Snapshot.Businesses)
	{
		Businesses.Add(Business.EntityId, &Business);
		Business.CompletedSales = bReservePurchases ? Business.SalesThisMonth : 0;
		if (bReservePurchases)
			Business.SalesThisMonth = 0;
		Business.PayrollDue = Business.PayrollPaid = 0;
		Business.ProducedGoods = 0;
	}
	TMap<uint64, int32> Workers;
	for (const FCityCitizenRecord& Citizen : Snapshot.Citizens)
		if (!Citizen.bOnMedicalLeave && Businesses.Contains(Citizen.WorkplaceEntityId))
			++Workers.FindOrAdd(Citizen.WorkplaceEntityId);
	TMap<uint64, int64> WageDue;
	TMap<uint64, int64> RemainingWages;
	for (const auto& C : Snapshot.Citizens)
		if (Businesses.Contains(C.WorkplaceEntityId))
			WageDue.FindOrAdd(C.WorkplaceEntityId) +=
			    FMath::Min(MonthlySalaryLimit(C.Education, Tuning),
			               static_cast<int64>(FMath::Max(0, Tuning.HouseholdIncomePerEmployedResident)) *
			                   (C.bSkilledJob ? 2 : 1));
	for (FCityBusinessLedgerRecord& Business : Snapshot.Businesses)
	{
		if (Business.bIndustrial)
		{
			Business.ProducedGoods =
			    bUseFreightInventory
			        ? StaffedOutput(Business.PendingOrders, Business.JobCapacity,
			                        Workers.FindRef(Business.EntityId))
			        : Business.PendingOrders; // Legacy aggregate-only fixtures have no freight economy.
			const int32 Stored = FMath::Min(Business.ProducedGoods, MAX_int32 - Business.Inventory);
			Business.Inventory += Stored;
			Snapshot.DiscardedGoods += Business.ProducedGoods - Stored;
		}
		Business.PayrollDue = WageDue.FindRef(Business.EntityId);
		Business.PayrollPaid = FMath::Min(Business.PayrollDue, FMath::Max<int64>(0, Business.Balance));
		Business.Balance -= Business.PayrollPaid;
		int64 Assigned = 0;
		if (Business.PayrollDue > 0)
			ForWorkers(Business.EntityId, [&](const auto& C) {
				const int64 Due =
				    FMath::Min(MonthlySalaryLimit(C.Education, Tuning),
				               static_cast<int64>(FMath::Max(0, Tuning.HouseholdIncomePerEmployedResident)) *
				                   (C.bSkilledJob ? 2 : 1));
				Assigned += Business.PayrollPaid * Due / Business.PayrollDue;
			});
		RemainingWages.Add(Business.EntityId, Business.PayrollPaid - Assigned);
	}
	for (FCityCitizenRecord& Citizen : Snapshot.Citizens)
	{
		if (Citizen.IsDetained())
			continue;
		if (FCityBusinessLedgerRecord* const* Employer = Businesses.Find(Citizen.WorkplaceEntityId))
		{
			const int64 Due =
			    FMath::Min(MonthlySalaryLimit(Citizen.Education, Tuning),
			               static_cast<int64>(FMath::Max(0, Tuning.HouseholdIncomePerEmployedResident)) *
			                   (Citizen.bSkilledJob ? 2 : 1));
			int64 Wage =
			    (*Employer)->PayrollDue > 0 ? (*Employer)->PayrollPaid * Due / (*Employer)->PayrollDue : 0;
			if (Wage < Due && RemainingWages.FindRef(Citizen.WorkplaceEntityId) > 0)
			{
				++Wage;
				--RemainingWages.FindChecked(Citizen.WorkplaceEntityId);
			}

			Citizen.AvailableMoney += Wage;
			if (!bUseFreightInventory)
			{
				Citizen.Activity = ECityCitizenActivity::Work;
				Citizen.TripPurpose = ECityTripPurpose::Commute;
				Citizen.TripVehicleType =
				    Citizen.Id % 3 == 0 ? ECityTripVehicleType::Car : ECityTripVehicleType::Walk;
				Citizen.TripDestinationEntityId = Citizen.WorkplaceEntityId;
			}
		}
	}
	if (ForecastTiming)
	{
		ForecastTiming->PrivatePayroll.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		PhaseStart = FPlatformTime::Seconds();
	}
	if (bReservePurchases)
	{
		for (auto& Household : Snapshot.Households)
		{
			Household.CompletedPurchases = Household.PurchasesThisMonth;
			Household.PurchasesThisMonth = 0;
		}
		ReservePurchases(Tuning);
		if (ForecastTiming)
		{
			ForecastTiming->Purchases.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
			PhaseStart = FPlatformTime::Seconds();
		}
		RefreshHouseholdBalances();
		if (ForecastTiming)
			ForecastTiming->HouseholdBalances.Add((FPlatformTime::Seconds() - PhaseStart) * 1000);
		return;
	}
	// Wallets remain the saved source of money; the household balance is a
	// derived total. Spend members' funds in stable citizen order without
	// copying money into a second account or borrowing from another home.
	TMap<uint64, TArray<FCityCitizenRecord*>> HouseholdMembers;
	TMap<uint64, int64> HouseholdFunds;
	for (FCityCitizenRecord& Member : Snapshot.Citizens)
	{
		HouseholdMembers.FindOrAdd(Member.HouseholdId).Add(&Member);
		HouseholdFunds.FindOrAdd(Member.HouseholdId) += FMath::Max<int64>(0, Member.AvailableMoney);
	}
	const int64 Price = FMath::Max<int64>(0, Tuning.HouseholdIncomePerGoodsPurchase);
	int32 RemainingSales = FMath::Max(0, Economy.GoodsSold);
	TMap<uint64, int32> PurchasesByHousehold;
	for (FCityCitizenRecord& Citizen : Snapshot.Citizens)
	{
		if (Citizen.IsDetained())
			continue;
		if (RemainingSales <= 0 || Citizen.ConsumptionNeed <= 0 ||
		    HouseholdFunds.FindRef(Citizen.HouseholdId) < Price)
			continue;
		FCityBusinessLedgerRecord* Retailer = nullptr;
		for (FCityBusinessLedgerRecord& Business : Snapshot.Businesses)
		{
			if (!Business.bIndustrial && Business.CompletedSales < Business.PendingOrders &&
			    (!bUseFreightInventory || Business.Inventory > 0))
			{
				Retailer = &Business;
				break;
			}
		}
		if (!Retailer)
			break;
		int64 Unpaid = Price;
		for (FCityCitizenRecord* Member : HouseholdMembers.FindChecked(Citizen.HouseholdId))
		{
			const int64 Contribution = FMath::Min(Unpaid, FMath::Max<int64>(0, Member->AvailableMoney));
			Member->AvailableMoney -= Contribution;
			Unpaid -= Contribution;
			if (Unpaid == 0)
				break;
		}
		HouseholdFunds.FindChecked(Citizen.HouseholdId) -= Price;
		Retailer->Balance += Price;
		++Retailer->CompletedSales;
		if (bUseFreightInventory)
			--Retailer->Inventory;
		++PurchasesByHousehold.FindOrAdd(Citizen.HouseholdId);
		if (bUseFreightInventory)
			Citizen.PendingShopEntityId = Retailer->EntityId;
		else
		{
			Citizen.Activity = ECityCitizenActivity::Shop;
			Citizen.TripPurpose = ECityTripPurpose::Purchase;
			Citizen.TripVehicleType = ECityTripVehicleType::Walk;
			Citizen.TripDestinationEntityId = Retailer->EntityId;
		}
		--RemainingSales;
	}
	for (FCityHouseholdRecord& Household : Snapshot.Households)
	{
		Household.Balance = 0;
		Household.ConsumptionOrders = 0;
		Household.CompletedPurchases = PurchasesByHousehold.FindRef(Household.Id);
		for (const FCityCitizenRecord& Citizen : Snapshot.Citizens)
		{
			if (Citizen.HouseholdId != Household.Id)
				continue;
			Household.Balance += Citizen.AvailableMoney;
			Household.ConsumptionOrders += Citizen.ConsumptionNeed;
		}
	}
}

FCityPopulationAggregate FCityPopulationSimulation::BuildAggregate(const FCityEconomyTuning& Tuning) const
{
	FCityPopulationAggregate Result;
	Result.Residents = Snapshot.Citizens.Num();
	for (const FCityCitizenRecord& Citizen : Snapshot.Citizens)
	{
		if (Citizen.IsHomeless())
			++Result.HomelessResidents;
		else
			++Result.HousedResidents;
		++Result.EducationCounts[static_cast<uint8>(Citizen.Education)];
		if (Citizen.WorkplaceEntityId != 0)
			++Result.EmployedResidents;
		else if (!Citizen.IsDetained())
			++Result.UnemployedResidents;
		Result.HouseholdGoodsDemand += Citizen.ConsumptionNeed;
	}
	for (const FCityBusinessLedgerRecord& Business : Snapshot.Businesses)
	{
		Result.Jobs += Business.JobCapacity;
	}
	for (const auto& E : Snapshot.PublicEmployers)
	{
		Result.Jobs += E.JobCapacity;
		Result.HouseholdIncome += E.PayrollPaid;
	}
	for (const FCityBusinessLedgerRecord& Business : Snapshot.Businesses)
		Result.HouseholdIncome += Business.PayrollPaid;
	for (const FCityBusinessLedgerRecord& Business : Snapshot.Businesses)
	{
		if (Business.bIndustrial)
			Result.GoodsProduced += Business.ProducedGoods;
		else
		{
			Result.RetailCapacity += Business.PendingOrders;
			Result.GoodsSold += Business.CompletedSales;
		}
	}
	Result.HouseholdSpending = static_cast<int64>(Result.GoodsSold) * Tuning.HouseholdIncomePerGoodsPurchase;
	if (Snapshot.bUsesPurchaseReservations)
	{
		Result.GoodsSold = Snapshot.LastMonthSales;
		Result.HouseholdSpending = Snapshot.LastMonthSpending;
	}
	return Result;
}

bool FCityPopulationSimulation::MatchesEconomy(const FCityEconomySnapshot& Economy,
                                               const FCityEconomyTuning& Tuning) const
{
	const FCityPopulationAggregate Aggregate = BuildAggregate(Tuning);
	return Aggregate.Residents == Economy.Residents && Aggregate.Jobs == Economy.Jobs &&
	       Aggregate.EmployedResidents == Economy.EmployedResidents &&
	       Aggregate.HouseholdGoodsDemand == Economy.HouseholdGoodsDemand &&
	       Aggregate.GoodsProduced == Economy.GoodsProduced &&
	       Aggregate.RetailCapacity == Economy.RetailCapacity && Aggregate.GoodsSold == Economy.GoodsSold &&
	       Aggregate.HouseholdIncome == Economy.HouseholdIncome &&
	       Aggregate.HouseholdSpending == Economy.HouseholdSpending;
}

bool FCityPopulationSimulation::MatchesSettledEconomy(const FCityEconomySnapshot& Economy,
                                                      const FCityEconomyTuning& Tuning) const
{
	FCityEconomySnapshot Expected = Economy;
	Expected.GoodsProduced = 0;
	TMap<uint64, int32> ActiveWorkers;
	for (const auto& Citizen : Snapshot.Citizens)
		if (Citizen.WorkplaceEntityId && !Citizen.bOnMedicalLeave && !Citizen.IsDetained())
			++ActiveWorkers.FindOrAdd(Citizen.WorkplaceEntityId);
	for (const auto& Business : Snapshot.Businesses)
		if (Business.bIndustrial)
			Expected.GoodsProduced += StaffedOutput(Business.PendingOrders, Business.JobCapacity,
			                                        ActiveWorkers.FindRef(Business.EntityId));
	// Validate finite production independently of its ledger, while retaining every
	// existing population, payroll, completed-sales and spending comparison.
	return MatchesEconomy(Expected, Tuning);
}

bool FCityPopulationSimulation::RestoreState(const FCityPopulationSnapshot& InSnapshot, FString& OutError)
{
	if (!FCityHousingMigration::Validate(InSnapshot, OutError))
		return false;
	TSet<uint64> CitizenIds;
	TSet<uint64> HouseholdIds;
	TSet<uint64> BusinessIds;
	TSet<uint64> TransitTripIds;
	if (InSnapshot.ExportsThisMonth < 0 || InSnapshot.LastMonthExports < 0 || InSnapshot.DiscardedGoods < 0 ||
	    InSnapshot.LastSettledMonthId < 0 || InSnapshot.NextCitizenId == 0 ||
	    InSnapshot.NextTransitTripId == 0)
	{
		OutError = TEXT("Invalid population clock");
		return false;
	}
	for (const FCityCitizenRecord& Record : InSnapshot.Citizens)
		if (Record.Id == 0 || (Record.HouseholdId == 0) != (Record.HomeEntityId == 0) ||
		    Record.UnemployedSinceStep < -1 || static_cast<uint8>(Record.Education) > 3 ||
		    Record.EducationMonths < 0 || Record.EducationMonths > 120 || Record.LastEducationMonth < 0 ||
		    Record.LastPolicyGrant < 0 || Record.LastPolicyGrant > 7 || Record.LastPolicyGrantMonth < 0 ||
		    Record.LastPolicyGrantMonth > InSnapshot.LastSettledMonthId ||
		    (Record.LastPolicyGrantMonth == 0 && Record.LastPolicyGrant != 0) ||
		    (Record.bSkilledJob &&
		     (!Record.WorkplaceEntityId || Record.Education < ECityEducationLevel::Secondary)) ||
		    Record.NextActionStep < 0 ||
		    static_cast<uint8>(Record.Schedule) > static_cast<uint8>(ECityCitizenSchedule::ToHome) ||
		    Record.Id >= InSnapshot.NextCitizenId || CitizenIds.Contains(Record.Id))
		{
			OutError = TEXT("Invalid citizen record");
			return false;
		}
		else
			CitizenIds.Add(Record.Id);
	for (const FCityCitizenRecord& Record : InSnapshot.Citizens)
	{
		if (!FCityHappinessEvaluator::ValidateCitizen(Record, OutError))
			return false;
		const bool bTravelling = Record.Schedule == ECityCitizenSchedule::ToWork ||
		                         Record.Schedule == ECityCitizenSchedule::ToShop ||
		                         Record.Schedule == ECityCitizenSchedule::ToHome;
		const ECityTripPurpose Expected =
		    Record.Schedule == ECityCitizenSchedule::ToWork   ? ECityTripPurpose::Commute
		    : Record.Schedule == ECityCitizenSchedule::ToShop ? ECityTripPurpose::Purchase
		                                                      : ECityTripPurpose::ReturnHome;
		if ((bTravelling && (!Record.CurrentLocationEntityId || !Record.TripDestinationEntityId ||
		                     Record.TripPurpose != Expected)) ||
		    ((Record.Schedule == ECityCitizenSchedule::Work ||
		      Record.Schedule == ECityCitizenSchedule::Shop) &&
		     (!Record.CurrentLocationEntityId || Record.TripPurpose != ECityTripPurpose::None)))
		{
			OutError = TEXT("Invalid citizen schedule");
			return false;
		}
	}
	for (const FCityHouseholdRecord& Record : InSnapshot.Households)
		if (Record.Id == 0 || Record.HomeEntityId == 0 || HouseholdIds.Contains(Record.Id) ||
		    Record.SupplyUnits < 0 || Record.SupplyDay < -1 ||
		    (Record.bUsesStoredSupplies && Record.SupplyDay < 0) ||
		    Record.LastEssentialPurchaseDay < -1 || Record.LastComfortDay < -7 ||
		    Record.ComfortTier < 0 || Record.ComfortTier > 2)
		{
			OutError = TEXT("Invalid household record");
			return false;
		}
		else
			HouseholdIds.Add(Record.Id);
	for (const FCityBusinessLedgerRecord& Record : InSnapshot.Businesses)
		if (Record.EntityId == 0 || Record.JobCapacity < 0 || Record.SkilledJobCapacity < 0 ||
		    Record.SkilledJobCapacity > Record.JobCapacity || Record.SkilledMinimumEducation < 2 ||
		    Record.SkilledMinimumEducation > 3 || Record.AiFeePaid < 0 ||
		    (Record.AiFeePaid != 0 && Record.AiFeePaid != FCityCommercialAi::MonthlyFee) ||
		    Record.AiPaidMonth < 0 || Record.AiPaidMonth > FCityCommercialAi::BillingPeriod(InSnapshot.LastSettledMonthId) ||
		    ((Record.AiFeePaid == 0) != (Record.AiPaidMonth == 0)) ||
		    (Record.bIndustrial && Record.AiFeePaid != 0) ||
            ((Record.AiFeePaid == 0) != (Record.AiCenterAtPayment == 0)) ||
            Record.AiJobsAtPayment < 0 || Record.AiSpecialistsAtPayment < 0 ||
            Record.AiSpecialistsAtPayment > Record.AiJobsAtPayment ||
            (Record.AiFeePaid > 0 && Record.AiJobsAtPayment < 2) || Record.Inventory < 0 ||
		    Record.PendingOrders < 0 || Record.CompletedSales < 0 || Record.ExportedGoods < 0 ||
		    Record.ProducedGoods < 0 || Record.PayrollDue < 0 || Record.PayrollPaid < 0 ||
		    Record.PayrollPaid > Record.PayrollDue || Record.RetailTier < 0 || Record.RetailTier > 2 ||
		    Record.OpeningGraceUntilStep < -1 || BusinessIds.Contains(Record.EntityId))
		{
			OutError = TEXT("Invalid business ledger");
			return false;
		}
		else
			BusinessIds.Add(Record.EntityId);
	TSet<uint64> PublicIds;
	for (const auto& E : InSnapshot.PublicEmployers)
		if (!E.EntityId || E.JobCapacity < 0 || E.JobCapacity > 20 || E.PayrollDue < 0 || E.PayrollPaid < 0 ||
		    E.PayrollPaid > E.PayrollDue || PublicIds.Contains(E.EntityId))
		{
			OutError = TEXT("Invalid public payroll ledger");
			return false;
		}
		else
			PublicIds.Add(E.EntityId);
	TMap<uint64, int32> SkilledWorkers;
	for (const auto& C : InSnapshot.Citizens)
		if (C.bSkilledJob)
			++SkilledWorkers.FindOrAdd(C.WorkplaceEntityId);
	for (const auto& Count : SkilledWorkers)
	{
		const auto* B =
		    InSnapshot.Businesses.FindByPredicate([&](const auto& V) { return V.EntityId == Count.Key; });
		if (!B || Count.Value > B->SkilledJobCapacity)
		{
			OutError = TEXT("Invalid skilled role capacity");
			return false;
		}
	}
    for (const auto& C : InSnapshot.Citizens)
        if (C.bSkilledJob)
        {
            const auto* Employer = InSnapshot.Businesses.FindByPredicate([&](const auto& V) { return V.EntityId == C.WorkplaceEntityId; });
            if (!Employer || static_cast<int32>(C.Education) < Employer->SkilledMinimumEducation)
            { OutError = TEXT("Unqualified specialist role"); return false; }
        }
	TSet<uint64> MedicalPatients;
	for (const FCityTransitTripRecord& Record : InSnapshot.TransitTrips)
	{
		if (Record.Id == 0 || Record.Id >= InSnapshot.NextTransitTripId ||
		    (Record.DestinationEntityId == 0 && Record.Purpose != ECityTripPurpose::FreightExport &&
		     Record.Purpose != ECityTripPurpose::HealthPickup) ||
		    Record.ArrivalStep < Record.StartStep || Record.Quantity < 0 ||
		    TransitTripIds.Contains(Record.Id))
		{
			OutError = TEXT("Invalid transit trip");
			return false;
		}
		TransitTripIds.Add(Record.Id);
		if (Record.IsWasteJourney())
		{
			if (!Record.OriginEntityId || !Record.DestinationEntityId || !Record.StationEntityId ||
			    Record.Quantity || Record.PatientCitizenId || Record.IncidentId ||
			    Record.VehicleType != ECityTripVehicleType::GarbageTruck ||
			    (Record.Purpose == ECityTripPurpose::WasteReturn &&
			     Record.DestinationEntityId != Record.StationEntityId))
			{
				OutError = TEXT("Invalid waste journey");
				return false;
			}
		}
		else if (Record.IsHealthJourney())
		{
			if (MedicalPatients.Contains(Record.PatientCitizenId) || !Record.PatientCitizenId ||
			    !CitizenIds.Contains(Record.PatientCitizenId) || !Record.StationEntityId || Record.Quantity ||
			    Record.IncidentId ||
			    (Record.Purpose == ECityTripPurpose::HealthWalkIn ? Record.VehicleType != ECityTripVehicleType::Walk
			     : Record.Purpose == ECityTripPurpose::HealthDriveIn ? Record.VehicleType != ECityTripVehicleType::Car
			     : Record.Purpose == ECityTripPurpose::HealthReturnHome ? (Record.VehicleType != ECityTripVehicleType::Walk && Record.VehicleType != ECityTripVehicleType::Car)
			     : Record.VehicleType != ECityTripVehicleType::Ambulance) ||
			    ((Record.Purpose == ECityTripPurpose::HealthPickup || Record.Purpose == ECityTripPurpose::HealthReturnHome)
			         ? Record.OriginEntityId != Record.StationEntityId
			         : Record.DestinationEntityId != Record.StationEntityId))
			{
				OutError = TEXT("Invalid hospital journey");
				return false;
			}
			MedicalPatients.Add(Record.PatientCitizenId);
		}
		else if (Record.IsEmergency())
		{
			if (!Record.StationEntityId || Record.IncidentId <= 0 || !Record.OriginEntityId ||
			    !Record.DestinationEntityId || Record.Quantity != 0 ||
			    (Record.Purpose == ECityTripPurpose::EmergencyReturn &&
			     Record.DestinationEntityId != Record.StationEntityId) ||
			    (Record.Purpose == ECityTripPurpose::PoliceResponse &&
			     Record.OriginEntityId != Record.StationEntityId) ||
			    (Record.VehicleType != ECityTripVehicleType::PoliceCar &&
			     Record.VehicleType != ECityTripVehicleType::FireEngine) ||
			    (Record.Purpose == ECityTripPurpose::FireResponse &&
			     Record.VehicleType != ECityTripVehicleType::FireEngine) ||
			    (Record.Purpose == ECityTripPurpose::PoliceResponse &&
			     Record.VehicleType != ECityTripVehicleType::PoliceCar))
			{
				OutError = TEXT("Invalid emergency trip");
				return false;
			}
			// Upgraded stations dispatch multiple vehicles. Staffing/fleet limits
			// govern dispatch, while existing trips can outlive a staffing reduction.
		}

		if (Record.Purpose == ECityTripPurpose::FreightDelivery ||
		    Record.Purpose == ECityTripPurpose::FreightExport)
		{
			const FCityBusinessLedgerRecord* Origin = InSnapshot.Businesses.FindByPredicate(
			    [&](const FCityBusinessLedgerRecord& B) { return B.EntityId == Record.OriginEntityId; });
			const FCityBusinessLedgerRecord* Destination = InSnapshot.Businesses.FindByPredicate(
			    [&](const FCityBusinessLedgerRecord& B) { return B.EntityId == Record.DestinationEntityId; });
			if (!Origin || !Origin->bIndustrial || Record.Quantity <= 0 ||
			    Record.Quantity > FreightLoadSize ||
			    Record.VehicleType != ECityTripVehicleType::FreightTruck ||
			    (Record.Purpose == ECityTripPurpose::FreightExport
			         ? Record.DestinationEntityId != 0
			         : (!Destination || Destination->bIndustrial)))
			{
				OutError = TEXT("Invalid freight cargo");
				return false;
			}
		}
		if (Record.Purpose == ECityTripPurpose::OpeningDelivery &&
		    (Record.OriginEntityId != 0 || Record.Quantity != 1 ||
		     Record.VehicleType != ECityTripVehicleType::FreightTruck ||
		     !BusinessIds.Contains(Record.DestinationEntityId)))
		{
			OutError = TEXT("Invalid business opening delivery");
			return false;
		}
	}
	if (!ValidatePurchaseReservations(InSnapshot, OutError))
		return false;
	Snapshot = InSnapshot;
	return true;
}

void FCityPopulationSimulation::SettlePolicies(FCityPolicyState& Policies, FCityPlacementModel& City,
                                               int64 Month, const FCityServiceSnapshot& Services,
                                               const FCityEconomyTuning& Tuning, bool EducationAvailable)
{
	if (Policies.SettleMonth(Month, City, Snapshot, Services, Tuning, EducationAvailable))
		RefreshHouseholdBalances();
}

void FCityPopulationSimulation::AdvanceEducation(const int64 Month, const FCityServiceSnapshot& Services,
                                                 const FCityEconomyTuning& Tuning)
{
	for (auto& C : Snapshot.Citizens)
	{
		if (C.IsDetained() || C.bOnMedicalLeave)
			continue;
		if (Month <= C.LastEducationMonth)
			continue;
		C.LastEducationMonth = Month;
		if (C.IsDetained() || C.Education >= ECityEducationLevel::Higher ||
		    (C.Education >= ECityEducationLevel::Secondary
		         ? !Services.HigherEducationCitizenIds.Contains(C.Id)
		         : !Services.EducationCitizenIds.Contains(C.Id)))
			continue;
		if (++C.EducationMonths >= FMath::Clamp(Tuning.SchoolMonthsPerLevel, 1, 120))
		{
			C.Education = static_cast<ECityEducationLevel>(static_cast<uint8>(C.Education) + 1);
			C.EducationMonths = 0;
		}
	}
}

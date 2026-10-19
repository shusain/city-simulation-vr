#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityWasteSimulation.h"
#include "Simulation/CityHealthSimulation.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityEnvironmentSimulation.h"
#include "Commands/CityPlacementModel.h"

void FCityPopulationSimulation::SyncHealthAvailability(const FCityHealthSnapshot& Health, const bool bPrepareQueries)
{
	// Call-scoped indexes: illness/recovery and arrivals can change between the
	// three care phases of a day. Never retain an availability cache across them.
	TSet<uint64> OnLeave, AvailableBuyers;
	const bool bIndexBuyers = bPrepareQueries && !Snapshot.PurchaseReservations.IsEmpty();
	if (bPrepareQueries)
		for (const auto& P : Health.Patients)
			if (P.InterruptsWork())
				OnLeave.Add(P.CitizenId);
	for (auto& C : Snapshot.Citizens)
	{
		const auto* Patient = bPrepareQueries ? nullptr : Health.Find(C.Id);
		const bool Leave = bPrepareQueries ? OnLeave.Contains(C.Id) : Patient && Patient->InterruptsWork();
		if (Leave && !C.bOnMedicalLeave)
		{
			// Cancel intentions without spending wallets or discarding partial study.
			C.TripPurpose = ECityTripPurpose::None;
			C.TripDestinationEntityId = 0;
			C.PendingShopEntityId = 0;
			C.Schedule = ECityCitizenSchedule::Home;
			C.Activity = ECityCitizenActivity::Home;
			C.bShopBeforeWork = false;
		}
		if (!Leave && C.bOnMedicalLeave)
		{
			C.NextActionStep = Health.LastStep + 1;
		}
		C.bOnMedicalLeave = Leave;
		if (bIndexBuyers && !Leave)
			AvailableBuyers.Add(C.Id);
	}
	// Unfulfilled purchases release their stock/funding locks, never money.
	Snapshot.PurchaseReservations.RemoveAll(
	    [&](const auto& Order)
	    {
		    if (bPrepareQueries)
			    return !AvailableBuyers.Contains(Order.BuyerId);
		    const auto* C = Snapshot.Citizens.FindByPredicate([&](const auto& X) { return X.Id == Order.BuyerId; });
		    return !C || C->bOnMedicalLeave;
	    });
}

void FCityPopulationSimulation::UpdateHospitalCare(
    FCityHealthSimulation& Health, const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
    const FCityRoadNetworkSnapshot& Network, const FCityServiceSnapshot& Services, const int64 Step,
    const int32 StepsPerMonth, const uint32 Seed, const bool bUnpaidUpkeep, const bool bAdvance,
    const FCityEnvironmentSnapshot* Environment, const FCityWasteSnapshot* Waste, const bool bPrepareQueries)
{
	FCityHealthContext Context;
	TMap<uint64, FCityCitizenRecord*> Citizens;
	if (bPrepareQueries)
	{
		Citizens.Reserve(Snapshot.Citizens.Num());
		for (auto& C : Snapshot.Citizens)
			Citizens.Add(C.Id, &C);
	}
	const auto FindCitizen = [&](uint64 Id) -> FCityCitizenRecord*
	{
		if (bPrepareQueries)
		{
			const auto* C = Citizens.Find(Id);
			return C ? *C : nullptr;
		}
		return Snapshot.Citizens.FindByPredicate([Id](const auto& C) { return C.Id == Id; });
	};
	TArray<uint64> Hospitals;
	bool Police = false, Fire = false;
	for (const auto& E : City.GetEntities())
	{
		if (E.Value.DefinitionId == TEXT("Hospital"))
			Hospitals.Add(E.Key);
		if (Services.GetFailure(E.Key) == ECityOperationFailure::None && Services.PublicStaffPercent.FindRef(E.Key) > 0)
		{
			Police |= E.Value.DefinitionId == TEXT("Police");
			Fire |= E.Value.DefinitionId == TEXT("Fire");
		}
	}
	Hospitals.Sort();
	Context.bSafetyEstablished = Police && Fire;
	// Only the advancing phase consumes exposure. The two arrival/scheduling
	// phases need current hospitals and citizens, but no exposure rebuild.
	TMap<uint64, int32> HomeRisk;
	if (bAdvance || !bPrepareQueries)
	{
		for (const auto& C : Snapshot.Citizens)
		{
			Context.SecondaryResidents += C.Education >= ECityEducationLevel::Secondary ? 1 : 0;
			const int32* PreparedRisk = bPrepareQueries ? HomeRisk.Find(C.HomeEntityId) : nullptr;
			int32 Risk = PreparedRisk ? *PreparedRisk : 0;
			if (!PreparedRisk)
			{
				const bool Outage =
				    C.HomeEntityId && Services.GetFailure(C.HomeEntityId) != ECityOperationFailure::None;
				const auto* Home = City.GetEntities().Find(C.HomeEntityId);
				const int32 Pollution =
				    Home && Environment ? FMath::Clamp(Environment->GetPollution(Home->Anchor) / 20, 0, 12) : 0;
				Risk = (Outage ? 4 : 0) + Pollution +
				       (Waste ? FMath::Min(8, Waste->Pressure(C.HomeEntityId, StepsPerMonth)) : 0);
				if (bPrepareQueries)
					HomeRisk.Add(C.HomeEntityId, Risk);
			}
			Context.Citizens.Add({C.Id, Risk, C.IsDetained()});
		}
	}
	for (const auto Id : Hospitals)
	{
		const int32 Capacity = Services.HospitalCapacities.FindRef(Id);
		Context.Hospitals.Add({Id, Capacity, !bUnpaidUpkeep && Capacity > 0});
	}
	if (bAdvance)
	{
		const int64 Elapsed = FMath::Max<int64>(0, Step - Health.GetSnapshot().LastStep);
		// Sick leave cannot count as the year of unemployment that removes a home.
		for (auto& C : Snapshot.Citizens)
			if (C.bOnMedicalLeave && C.UnemployedSinceStep >= 0)
				C.UnemployedSinceStep += Elapsed;
		FCityHealthTuning T;
		T.RandomSeed = Seed;
		T.CheckIntervalSteps = FMath::Max(1, StepsPerMonth);
		T.GraceSteps = T.CheckIntervalSteps * 6;
		T.MildTreatmentSteps = T.CheckIntervalSteps;
		T.SevereTreatmentSteps = T.CheckIntervalSteps * 2;
		Health.AdvanceToStep(Step, Context, T);
	}
	auto Route = [&](uint64 From, uint64 To, ECityRoadAccess Access, TArray<FCityCellCoord>& Cells)
	{
		FCityCellCoord A, B;
		return FCityTrafficSimulation::FindPortal(City, Catalog, From, A) &&
		       FCityTrafficSimulation::FindPortal(City, Catalog, To, B) && Network.FindRoute(A, B, Access, Cells);
	};
	// Consume actual arrivals, never planned deadlines. Mild visits release clinic slots
	// on dispensing medicine and retain their personal mode for the real homeward leg.
	Snapshot.TransitTrips.RemoveAll([&](FCityTransitTripRecord& Trip)
	{
		if (!Trip.IsHealthJourney()) return false;
		const auto* P = Health.GetSnapshot().Find(Trip.PatientCitizenId);
		auto* C = FindCitizen(Trip.PatientCitizenId);
		const auto* H = Context.Hospitals.FindByPredicate([&](const auto& X) { return X.EntityId == Trip.StationEntityId; });
		const bool Returning = Trip.Purpose == ECityTripPurpose::HealthReturnHome;
		if (!P || !C || C->IsDetained() || P->HospitalId != Trip.StationEntityId || !H || (!Returning && !H->bOperational))
		{
			if (P && P->HospitalId == Trip.StationEntityId) Health.ReleaseCare(P->CitizenId);
			return true;
		}
		TArray<FCityCellCoord> Cells;
		if (!Route(Trip.OriginEntityId, Trip.DestinationEntityId,
		           Trip.VehicleType == ECityTripVehicleType::Walk ? ECityRoadAccess::Pedestrian : ECityRoadAccess::Car, Cells))
		{
			// Medicine is already dispensed. A blocked return waits/retries without
			// occupying a clinic/ward place or progressing home recovery.
			if (Returning) return false;
			Health.ReleaseCare(P->CitizenId);return true;
		}
		if (!Trip.bCompleted) return false;
		if (Trip.Purpose == ECityTripPurpose::HealthPickup)
		{
			Trip.OriginEntityId = Trip.DestinationEntityId;Trip.DestinationEntityId = Trip.StationEntityId;
			Trip.Purpose = ECityTripPurpose::HealthAdmission;
		}
		else if (Returning)
		{
			if (Trip.DestinationEntityId == C->HomeEntityId && Health.ConfirmHome(P->CitizenId)) C->CurrentLocationEntityId = C->HomeEntityId;
			return true;
		}
		else
		{
			if (!Health.ConfirmArrival(P->CitizenId, Trip.StationEntityId)) return true;
			C->CurrentLocationEntityId = Trip.StationEntityId;
			if (P->Illness == ECityIllness::Severe) return true;
			Trip.OriginEntityId = Trip.StationEntityId;Trip.DestinationEntityId = C->HomeEntityId;
			Trip.Purpose = ECityTripPurpose::HealthReturnHome;
		}
		Trip.bCompleted = false;Trip.StartStep = Trip.ArrivalStep = Step;
		return false;
	});
	// Pruned outward trips release finite reservations; a missing return is rebuilt.
	const auto Patients = Health.GetSnapshot().Patients;
	TSet<uint64> JourneyPatients;
	for (const auto& T : Snapshot.TransitTrips) if (T.IsHealthJourney()) JourneyPatients.Add(T.PatientCitizenId);
	for (const auto& P : Patients)
	{
		if (JourneyPatients.Contains(P.CitizenId)) continue;
		if (P.UsesOutpatientPlace() || P.Care == ECityHealthCare::AwaitingAmbulance) Health.ReleaseCare(P.CitizenId);
		else if (P.Care == ECityHealthCare::ReturningHome)
		{
			const auto* C = FindCitizen(P.CitizenId);
			if (!C || !C->HomeEntityId) continue;
			if (C->CurrentLocationEntityId == C->HomeEntityId) { Health.ConfirmHome(P.CitizenId);continue; }
			auto& T = Snapshot.TransitTrips.Emplace_GetRef();T.Id = Snapshot.NextTransitTripId++;
			T.PatientCitizenId=P.CitizenId;T.StationEntityId=P.HospitalId;
			T.OriginEntityId=C->CurrentLocationEntityId;T.DestinationEntityId=C->HomeEntityId;
			T.Purpose=ECityTripPurpose::HealthReturnHome;T.VehicleType=ECityTripVehicleType::Car;T.StartStep=T.ArrivalStep=Step;
		}
	}
	TMap<uint64, int32> Busy, Beds, Visits;
	for (const auto& T : Snapshot.TransitTrips) if (T.IsHealthJourney() && T.VehicleType == ECityTripVehicleType::Ambulance) ++Busy.FindOrAdd(T.StationEntityId);
	for (const auto& P : Health.GetSnapshot().Patients)
	{
		if (P.UsesBed()) ++Beds.FindOrAdd(P.HospitalId);
		if (P.UsesOutpatientPlace()) ++Visits.FindOrAdd(P.HospitalId);
	}
	int32 Searches = 0;
	const auto Pending = Health.GetSnapshot().Patients;
	// Rotate the bounded search so unreachable early IDs cannot starve later patients.
	for (int32 I=0;I<Pending.Num() && Searches<64;++I)
	{
		const auto& P=Pending[(Step % FMath::Max(1,Pending.Num())+I)%Pending.Num()];
		if (P.Care != ECityHealthCare::None) continue;
		const auto* C=FindCitizen(P.CitizenId);if (!C || C->IsDetained()) continue;
		const bool Mild=P.Illness==ECityIllness::Mild;
		if (Mild && (!C->HomeEntityId || !City.GetEntities().Contains(C->HomeEntityId))) continue;
		const uint64 Origin=C->CurrentLocationEntityId && City.GetEntities().Contains(C->CurrentLocationEntityId) ? C->CurrentLocationEntityId : C->HomeEntityId;
		uint64 Best=0;int32 BestDistance=MAX_int32,BestMode=MAX_int32;
		ECityHealthCare BestCare=ECityHealthCare::None;
		for (uint64 Id:Hospitals)
		{
			if (++Searches>64) break;
			const auto* H=Context.Hospitals.FindByPredicate([&](const auto& X){return X.EntityId==Id;});
			if (!H || !H->bOperational || (Mild ? Visits.FindRef(Id) : Beds.FindRef(Id))>=H->Capacity) continue;
			TArray<FCityCellCoord> Outward,Homeward;
			ECityHealthCare Care=ECityHealthCare::AwaitingAmbulance;int32 Mode=0;
			if (Mild)
			{
				if (Route(Origin,Id,ECityRoadAccess::Pedestrian,Outward) && Outward.Num()-1<=FCityHealthSimulation::MaximumWalkingDistance &&
				    Route(Id,C->HomeEntityId,ECityRoadAccess::Pedestrian,Homeward) && Homeward.Num()-1<=FCityHealthSimulation::MaximumWalkingDistance)
					Care=ECityHealthCare::WalkIn;
				else
				{
					if (!Route(Origin,Id,ECityRoadAccess::Car,Outward) || !Route(Id,C->HomeEntityId,ECityRoadAccess::Car,Homeward)) continue;
					Care=ECityHealthCare::DriveIn;Mode=1;
				}
			}
			else if (Busy.FindRef(Id)>=Services.HospitalFleetCapacities.FindRef(Id) || !Route(Id,Origin,ECityRoadAccess::Car,Outward) || !Route(Origin,Id,ECityRoadAccess::Car,Homeward)) continue;
			const int32 Distance=Outward.Num()-1;
			if (!Best || Mode<BestMode || (Mode==BestMode && (Distance<BestDistance || (Distance==BestDistance && Id<Best))))
			{Best=Id;BestMode=Mode;BestDistance=Distance;BestCare=Care;}
		}
		if (!Best || !Health.ReserveCare(P.CitizenId,Best,BestCare,BestDistance,Context)) continue;
		auto& Trip=Snapshot.TransitTrips.Emplace_GetRef();Trip.Id=Snapshot.NextTransitTripId++;
		Trip.PatientCitizenId=P.CitizenId;Trip.StationEntityId=Best;
		Trip.OriginEntityId=Mild ? Origin : Best;Trip.DestinationEntityId=Mild ? Best : Origin;
		Trip.Purpose=Mild ? (BestCare==ECityHealthCare::WalkIn ? ECityTripPurpose::HealthWalkIn : ECityTripPurpose::HealthDriveIn) : ECityTripPurpose::HealthPickup;
		Trip.VehicleType=Mild ? (BestCare==ECityHealthCare::WalkIn ? ECityTripVehicleType::Walk : ECityTripVehicleType::Car) : ECityTripVehicleType::Ambulance;
		Trip.StartStep=Trip.ArrivalStep=Step;
		if (Mild) ++Visits.FindOrAdd(Best);else {++Beds.FindOrAdd(Best);++Busy.FindOrAdd(Best);}
	}
	SyncHealthAvailability(Health.GetSnapshot(),bPrepareQueries);
}

#include "Simulation/CityPopulationSimulation.h"
#include "Simulation/CityFireSimulation.h"
#include "Simulation/CityCrimeSimulation.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Commands/CityPlacementModel.h"

void FCityPopulationSimulation::UpdateEmergencyResponses(const FCityPlacementModel& City,
                                                         const UCityBuildingCatalog* Catalog,
                                                         const FCityRoadNetworkSnapshot& Network,
                                                         const FCityFireSnapshot& Fire, const FCityCrimeSnapshot& Crime,
                                                         const FCityServiceSnapshot& Services, const int64 Step,
                                                         const bool bUnpaidUpkeep)
{
	// A crew retains its identity through scene-to-scene travel and the return.
	TSet<int64> ClaimedFires;
	for (const auto& T : Snapshot.TransitTrips)
		if (T.Purpose == ECityTripPurpose::FireResponse)
			ClaimedFires.Add(T.IncidentId);
	for (auto& Trip : Snapshot.TransitTrips)
	{
		if (Trip.IsHealthJourney() || !Trip.IsEmergency() || Trip.Purpose == ECityTripPurpose::EmergencyReturn)
			continue;
		const bool bActive = Trip.Purpose == ECityTripPurpose::FireResponse
		                         ? Fire.RecentIncidents.ContainsByPredicate(
		                               [&](const auto& I) { return I.Id == Trip.IncidentId && I.ResolvedStep < 0; })
		                         : Crime.RecentIncidents.ContainsByPredicate(
		                               [&](const auto& I) { return I.Id == Trip.IncidentId && I.EndStep > Step; });
		if (!bActive && Trip.bCompleted)
		{
			if (Trip.Purpose == ECityTripPurpose::FireResponse && !bUnpaidUpkeep &&
			    Services.GetFailure(Trip.StationEntityId) == ECityOperationFailure::None &&
			    Services.PublicStaffPercent.FindRef(Trip.StationEntityId) > 0)
			{
				FCityCellCoord Start;
				const FCityFireIncident* Next = nullptr;
				int32 BestDistance = NearbyFireResponseHops + 1;
				if (FCityTrafficSimulation::FindPortal(City, Catalog, Trip.DestinationEntityId, Start))
					for (const auto& I : Fire.RecentIncidents)
					{
						if (I.ResolvedStep >= 0 || ClaimedFires.Contains(I.Id))
							continue;
						FCityCellCoord End;
						TArray<FCityCellCoord> Route;
						if (!FCityTrafficSimulation::FindPortal(City, Catalog, I.EntityId, End) ||
						    !Network.FindRoute(Start, End, ECityRoadAccess::Car, Route))
							continue;
						const int32 Distance = FMath::Max(0, Route.Num() - 1);
						if (Distance < BestDistance || (Next && Distance == BestDistance && I.Id < Next->Id))
						{
							Next = &I;
							BestDistance = Distance;
						}
					}
				if (Next)
				{
					ClaimedFires.Remove(Trip.IncidentId);
					ClaimedFires.Add(Next->Id);
					Trip.OriginEntityId = Trip.DestinationEntityId;
					Trip.DestinationEntityId = Next->EntityId;
					Trip.IncidentId = Next->Id;
					Trip.bCompleted = false;
					Trip.StartStep = Trip.ArrivalStep = Step;
					continue;
				}
			}
			Trip.OriginEntityId = Trip.DestinationEntityId;
			Trip.DestinationEntityId = Trip.StationEntityId;
			Trip.Purpose = ECityTripPurpose::EmergencyReturn;
			Trip.bCompleted = false;
			Trip.StartStep = Trip.ArrivalStep = Step;
		}
		// An outbound crew finishes the road journey even if the incident ends
		// before arrival, then seeks another nearby fire or returns.
	}
	TMap<uint64, int32> Busy;
	int32 ResponseCount = 0;
	for (const auto& Trip : Snapshot.TransitTrips)
		if (Trip.IsEmergency())
		{
			++Busy.FindOrAdd(Trip.StationEntityId);
			++ResponseCount;
		}
	TArray<uint64> Stations;
	Services.PublicJobs.GetKeys(Stations);
	Stations.Sort();
	auto Dispatch = [&](const int64 IncidentId, const uint64 Target, const bool bFire)
	{
		if (ResponseCount >= 16 || bUnpaidUpkeep)
			return;
		for (const auto& Trip : Snapshot.TransitTrips)
			if (Trip.IsEmergency() && Trip.IncidentId == IncidentId &&
			    Trip.VehicleType == (bFire ? ECityTripVehicleType::FireEngine : ECityTripVehicleType::PoliceCar))
				return;
		FCityCellCoord End;
		if (!FCityTrafficSimulation::FindPortal(City, Catalog, Target, End))
			return;
		uint64 Best = 0;
		int32 BestDistance = MAX_int32;
		for (const uint64 Id : Stations)
		{
			const auto* Entity = City.GetEntities().Find(Id);
			const int32 Fleet =
			    Services.StationFleetCapacities.Contains(Id) ? Services.StationFleetCapacities.FindRef(Id) : 1;
			const int32 StaffedFleet = FMath::Max(1, Fleet * Services.PublicStaffPercent.FindRef(Id) / 100);
			if (!Entity || Entity->DefinitionId != (bFire ? TEXT("Fire") : TEXT("Police")) ||
			    Busy.FindRef(Id) >= StaffedFleet || Services.GetFailure(Id) != ECityOperationFailure::None ||
			    Services.PublicStaffPercent.FindRef(Id) <= 0)
				continue;
			FCityCellCoord Start;
			TArray<FCityCellCoord> Route;
			if (!FCityTrafficSimulation::FindPortal(City, Catalog, Id, Start) ||
			    !Network.FindRoute(Start, End, ECityRoadAccess::Car, Route))
				continue;
			const int32 Distance = FMath::Max(0, Route.Num() - 1);
			if (Distance < BestDistance)
			{
				Best = Id;
				BestDistance = Distance;
			}
		}
		if (!Best)
			return;
		auto& Trip = Snapshot.TransitTrips.Emplace_GetRef();
		Trip.Id = Snapshot.NextTransitTripId++;
		Trip.OriginEntityId = Trip.StationEntityId = Best;
		Trip.DestinationEntityId = Target;
		Trip.IncidentId = IncidentId;
		Trip.Purpose = bFire ? ECityTripPurpose::FireResponse : ECityTripPurpose::PoliceResponse;
		Trip.VehicleType = bFire ? ECityTripVehicleType::FireEngine : ECityTripVehicleType::PoliceCar;
		Trip.StartStep = Trip.ArrivalStep = Step;
		++Busy.FindOrAdd(Best);
		++ResponseCount;
	};
	for (const auto& I : Fire.RecentIncidents)
		if (I.ResolvedStep < 0)
			Dispatch(I.Id, I.EntityId, true);
	for (const auto& I : Crime.RecentIncidents)
		if (I.EndStep > Step)
			Dispatch(I.Id, I.EntityId, false);
}

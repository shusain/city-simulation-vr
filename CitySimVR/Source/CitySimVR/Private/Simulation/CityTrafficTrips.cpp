#include "Simulation/CityTrafficSimulation.h"

#include "Commands/CityPlacementModel.h"
#include "Simulation/CityBuildingDefinition.h"

bool FCityTrafficSimulation::FindPortal(const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
                                        const uint64 EntityId, FCityCellCoord& OutCell)
{
	if (EntityId == 0)
	{
		OutCell = City.GetEntranceCell();
		return City.IsRoad(OutCell);
	}
	const FCityPlacedEntity* Entity = City.GetEntities().Find(EntityId);
	if (!Entity)
		return false;
	const FCityBuildingDefinition* Definition = Catalog ? Catalog->FindDefinition(Entity->DefinitionId) : nullptr;
	const int32 Sides = Definition ? Definition->EntranceSides : 15;
	TArray<FCityCellCoord> Candidates;
	for (int32 Side = 0; Side < 4; ++Side)
	{
		if ((Sides & (1 << Side)) == 0)
			continue;
		const int32 WorldSide = (Side + Entity->QuarterTurnsClockwise) % 4;
		const bool bHorizontal = WorldSide == 0 || WorldSide == 2;
		for (int32 Offset = 0; Offset < (bHorizontal ? Entity->FootprintSize.X : Entity->FootprintSize.Y); ++Offset)
		{
			const FCityCellCoord Cell =
			    bHorizontal ? FCityCellCoord(Entity->Anchor.X + Offset,
			                                 Entity->Anchor.Y + (WorldSide == 0 ? -1 : Entity->FootprintSize.Y))
			                : FCityCellCoord(Entity->Anchor.X + (WorldSide == 1 ? Entity->FootprintSize.X : -1),
			                                 Entity->Anchor.Y + Offset);
			if (City.SupportsBuildingFrontage(Cell) && City.IsRoadConnected(Cell))
				Candidates.Add(Cell);
		}
	}
	Candidates.Sort([](const FCityCellCoord& A, const FCityCellCoord& B)
	                { return A.Y == B.Y ? A.X < B.X : A.Y < B.Y; });
	if (Candidates.IsEmpty())
		return false;
	OutCell = Candidates[0];
	return true;
}

void FCityTrafficSimulation::ReconcileNetwork(const FCityRoadNetworkSnapshot& Network)
{
	if ((!Snapshot.Vehicles.IsEmpty() || !Snapshot.WaitingDepartures.IsEmpty()) &&
	    Snapshot.NetworkFingerprint != GetNetworkFingerprint(Network))
	{
		// Cancel obsolete paths without reporting arrivals. Sources retry from
		// their portals on the next step; no stale edge ID reaches the renderer.
		Snapshot.Vehicles.Reset();
		Snapshot.WaitingDepartures.Reset();
		Snapshot.LaneLoads.Reset();
		Snapshot.NetworkFingerprint = 0;
	}
}

bool FCityTrafficSimulation::BuildRoute(const FCityRoadNetworkSnapshot& Network, const FCityCellCoord& Start,
                                        const FCityCellCoord& End, FCityTrafficVehicleState& Vehicle)
{
	const ECityRoadAccess Access = Vehicle.VehicleType == ECityTripVehicleType::Walk ? ECityRoadAccess::Pedestrian
	                               : (Vehicle.VehicleType == ECityTripVehicleType::FreightTruck ||
	                                  Vehicle.VehicleType == ECityTripVehicleType::GarbageTruck)
	                                   ? ECityRoadAccess::Freight
	                                   : ECityRoadAccess::Car;
	TArray<FCityCellCoord> Cells;
	if (!Network.FindRoute(Start, End, Access, Cells) || Cells.Num() < 2)
		return false;
	FCityFrameTimingScope ConversionScope(Network.RoutingTiming ? &Network.RoutingTiming->Conversion : nullptr);
	Vehicle.Route.Reset();
	Vehicle.RouteLegIndex = 0;
	Vehicle.SpeedUnitsPerStep = 0;
	Vehicle.bCompleted = false;
	Vehicle.CompletedStep = 0;
	for (int32 CellIndex = 0; CellIndex + 1 < Cells.Num(); ++CellIndex)
	{
		int32 Index = 0;
		bool bReverse = false;
		const FCityRoadEdge* Found =
		    Network.FindRouteSegment(Cells[CellIndex], Cells[CellIndex + 1], Access, Index, bReverse);
		if (!Found)
			return false;
		if (Vehicle.Route.IsEmpty() || Vehicle.Route.Last().EdgeId != Found->Id ||
		    Vehicle.Route.Last().bReverse != bReverse)
		{
			const int32 Lanes = bReverse ? Found->ReverseLanes : Found->ForwardLanes;
			const uint64 Identity = Vehicle.SourceId ? Vehicle.SourceId : Vehicle.Id;
			const int32 Lane = Vehicle.VehicleType == ECityTripVehicleType::Walk || Vehicle.bEmergencyResponding
			                       ? 0 : static_cast<int32>(Identity % Lanes);
			FCityTrafficRouteLeg Leg{Found->Id, bReverse, Lane};
			if (!Vehicle.Route.IsEmpty() && !FindConnector(Vehicle.Route.Last(), Leg, Network))
				return false;
			Vehicle.Route.Add(Leg);
		}
		if (CellIndex == 0)
			Vehicle.StartDistanceUnits = Vehicle.DistanceUnits = Index * TrafficUnitsPerCell;
		Vehicle.EndDistanceUnits = (Index + 1) * TrafficUnitsPerCell;
	}
	return !Vehicle.Route.IsEmpty();
}

void FCityTrafficSimulation::UpdateTrips(const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
                                         const FCityPopulationSnapshot& Population,
                                         const FCityRoadNetworkSnapshot& Network, const int64 Step)
{
	ReconcileNetwork(Network);
	FCityFrameTimingScope UpdateScope(Network.RoutingTiming ? &Network.RoutingTiming->TrafficUpdate : nullptr);
	TMap<uint64, const FCityCitizenRecord*> Citizens;
	TMap<uint64, const FCityTransitTripRecord*> Deliveries;
	for (const FCityCitizenRecord& Citizen : Population.Citizens)
		Citizens.Add(Citizen.Id, &Citizen);
	for (const FCityTransitTripRecord& Trip : Population.TransitTrips)
		if (!Trip.bCompleted || Trip.IsWasteJourney() ||
		    (Trip.IsEmergency() && Trip.Purpose != ECityTripPurpose::EmergencyReturn))
			Deliveries.Add(Trip.Id, &Trip);
	auto Obsolete = [&](const FCityTrafficVehicleState& Vehicle)
	    {
		    if (Vehicle.SourceId == 0)
			    return true;
		    if ((Vehicle.DestinationEntityId != 0 && !City.GetEntities().Contains(Vehicle.DestinationEntityId)) ||
		        (Vehicle.OriginEntityId != 0 && !City.GetEntities().Contains(Vehicle.OriginEntityId)))
			    return true;
		    if (Vehicle.bTransitTrip)
		    {
			    const auto* const* Trip = Deliveries.Find(Vehicle.SourceId);
			    return !Trip || (*Trip)->OriginEntityId != Vehicle.OriginEntityId ||
			           (*Trip)->DestinationEntityId != Vehicle.DestinationEntityId;
		    }
		    const FCityCitizenRecord* const* Citizen = Citizens.Find(Vehicle.SourceId);
		    return !Citizen || (*Citizen)->TripPurpose == ECityTripPurpose::None ||
		           ((*Citizen)->CurrentLocationEntityId ? (*Citizen)->CurrentLocationEntityId
		                                                : (*Citizen)->HomeEntityId) != Vehicle.OriginEntityId ||
		           (*Citizen)->TripDestinationEntityId != Vehicle.DestinationEntityId;
	    };
	Snapshot.Vehicles.RemoveAll(Obsolete);
	Snapshot.WaitingDepartures.RemoveAll(Obsolete);
	// Bounded route-search work and one record per source. Rotating the start
	// gives queued citizens a chance even when earlier portals remain blocked.
	auto Schedule = [&](const uint64 SourceId, const uint64 Origin, const uint64 Destination,
	                    const ECityTripVehicleType Type, const bool bTransit, const bool bLegacyLoop = false,
	                    const bool bResponding = false, const int64 RequestedStep = 0)
	{
		const uint64 Id = SourceId * 2 + (bTransit ? 1 : 0);
		if (Snapshot.WaitingDepartures.ContainsByPredicate([&](const auto& V) { return V.Id == Id; }))
			return;
		const int32 ExistingIndex =
		    Snapshot.Vehicles.IndexOfByPredicate([&](const FCityTrafficVehicleState& V) { return V.Id == Id; });
		FCityTrafficVehicleState Vehicle;
		if (ExistingIndex != INDEX_NONE)
		{
			const FCityTrafficVehicleState& Existing = Snapshot.Vehicles[ExistingIndex];
			if (!Existing.bCompleted || bTransit || !bLegacyLoop || Step < Existing.CompletedStep + 2)
				return;
			Vehicle.bReturning = !Existing.bReturning;
		}
		else if (Snapshot.Vehicles.Num() + Snapshot.WaitingDepartures.Num() >= 2048)
			return;
		Vehicle.Id = Id;
		Vehicle.SourceId = SourceId;
		Vehicle.OriginEntityId = Origin;
		Vehicle.DestinationEntityId = Destination;
		Vehicle.VehicleType = Type;
		Vehicle.bTransitTrip = bTransit;
		Vehicle.bEmergencyResponding = bResponding;
		Vehicle.DepartureRequestedStep = FMath::Clamp<int64>(RequestedStep, 0, Snapshot.FixedStepId);
		Vehicle.PedestrianTrack = Type == ECityTripVehicleType::Walk ? SourceId % 2 : 0;
		Vehicle.LengthUnits = Type == ECityTripVehicleType::Walk ? 80
		                      : (Type == ECityTripVehicleType::FreightTruck ||
		                         Type == ECityTripVehicleType::GarbageTruck || Type == ECityTripVehicleType::FireEngine)
		                          ? 550
		                          : 350;
		FCityCellCoord Start, End;
		if (!FindPortal(City, Catalog, Vehicle.bReturning ? Destination : Origin, Start) ||
		    !FindPortal(City, Catalog, Vehicle.bReturning ? Origin : Destination, End))
			return;
		// Adjacent portals need no road movement. Complete once, still observing
		// source lifecycle and the same dwell clock as longer journeys.
		if (Start == End)
		{
			for (const FCityRoadEdge& Edge : Network.Edges)
			{
				const int32 Index = Edge.Cells.IndexOfByKey(Start);
				if (Index == INDEX_NONE)
					continue;
				Vehicle.Route.Add({Edge.Id, false, 0});
				Vehicle.StartDistanceUnits = Vehicle.EndDistanceUnits = Vehicle.DistanceUnits =
				    Index * TrafficUnitsPerCell;
				Vehicle.bCompleted = true;
				Vehicle.CompletedStep = Step;
				break;
			}
			if (Vehicle.Route.IsEmpty())
				return;
		}
		else if (!BuildRoute(Network, Start, End, Vehicle))
			return;
		if (ExistingIndex != INDEX_NONE)
			Snapshot.Vehicles.RemoveAt(ExistingIndex);
		if (Vehicle.bCompleted)
			AddVehicle(Vehicle, Network);
		else
		{
			Snapshot.WaitingDepartures.Add(MoveTemp(Vehicle));
			Snapshot.NetworkFingerprint = GetNetworkFingerprint(Network);
		}
	};
	for (const auto& Trip : Population.TransitTrips)
		if (Trip.IsEmergency() && !Trip.bCompleted && Trip.StartStep <= Step)
			Schedule(Trip.Id, Trip.OriginEntityId, Trip.DestinationEntityId, Trip.VehicleType, true, false,
			         Trip.Purpose != ECityTripPurpose::EmergencyReturn, Trip.StartStep);
	for (const auto& Trip : Population.TransitTrips)
		if (Trip.IsWasteJourney() && !Trip.bCompleted && Trip.StartStep <= Step)
			Schedule(Trip.Id, Trip.OriginEntityId, Trip.DestinationEntityId, Trip.VehicleType, true, false, false, Trip.StartStep);
	for (int32 Index = 0; Index < FMath::Min(16, Population.TransitTrips.Num()); ++Index)
	{
		const FCityTransitTripRecord& Trip =
		    Population.TransitTrips[(Step * 16 + Index) % Population.TransitTrips.Num()];
		if (!Trip.IsEmergency() && !Trip.bCompleted && Trip.StartStep <= Step)
			Schedule(Trip.Id, Trip.OriginEntityId, Trip.DestinationEntityId, Trip.VehicleType, true, false, false, Trip.StartStep);
	}
	for (int32 Index = 0; Index < FMath::Min(CitizenRouteBudget, Population.Citizens.Num()); ++Index)
	{
		const FCityCitizenRecord& Citizen =
		    Population.Citizens[(Step * CitizenRouteBudget + Index) % Population.Citizens.Num()];
		if (Citizen.TripPurpose != ECityTripPurpose::None)
			Schedule(
			    Citizen.Id, Citizen.CurrentLocationEntityId ? Citizen.CurrentLocationEntityId : Citizen.HomeEntityId,
			    Citizen.TripDestinationEntityId, Citizen.TripVehicleType, false, Citizen.CurrentLocationEntityId == 0,
			    false, Citizen.NextActionStep);
	}
	AdmitDepartures(Network);
	RebuildLaneLoads(Network);
}

bool FCityTrafficSimulation::SamplePosition(const FCityTrafficVehicleState& Vehicle,
                                            const FCityTrafficVehicleState* Previous,
                                            const FCityRoadNetworkSnapshot& Network, const double Alpha,
                                            FVector& OutGridPosition, FVector& OutDirection)
{
	if (!Vehicle.Route.IsValidIndex(Vehicle.RouteLegIndex))
		return false;
	for (const FCityTrafficRouteLeg& Leg : Vehicle.Route)
		if (!ValidateLeg(Leg, Network))
			return false;
	int32 LegIndex = Vehicle.RouteLegIndex;
	double Distance = Vehicle.DistanceUnits;
	if (Previous && Previous->Route.IsValidIndex(Previous->RouteLegIndex) && !Previous->bCompleted &&
	    Previous->bReturning == Vehicle.bReturning && Previous->OriginEntityId == Vehicle.OriginEntityId &&
	    Previous->DestinationEntityId == Vehicle.DestinationEntityId &&
	    Previous->StartDistanceUnits == Vehicle.StartDistanceUnits &&
	    Previous->EndDistanceUnits == Vehicle.EndDistanceUnits && Previous->Route.Num() == Vehicle.Route.Num() &&
	    Previous->RouteLegIndex <= LegIndex)
	{
		bool bSameRoute = true;
		for (int32 Index = 0; Index < Vehicle.Route.Num(); ++Index)
		{
			const FCityTrafficRouteLeg& A = Vehicle.Route[Index];
			const FCityTrafficRouteLeg& B = Previous->Route[Index];
			bSameRoute &= A.EdgeId == B.EdgeId && A.bReverse == B.bReverse;
		}
		if (bSameRoute)
		{
			double Travel = Distance - Previous->DistanceUnits;
			for (int32 Index = Previous->RouteLegIndex; Index < LegIndex; ++Index)
				Travel += GetEdgeLengthUnits(Network.Edges[Vehicle.Route[Index].EdgeId]);
			LegIndex = Previous->RouteLegIndex;
			Distance = Previous->DistanceUnits + FMath::Max(0.0, Travel) * FMath::Clamp(Alpha, 0.0, 1.0);
			while (LegIndex < Vehicle.RouteLegIndex &&
			       Distance >= GetEdgeLengthUnits(Network.Edges[Vehicle.Route[LegIndex].EdgeId]))
			{
				Distance -= GetEdgeLengthUnits(Network.Edges[Vehicle.Route[LegIndex].EdgeId]);
				++LegIndex;
			}
		}
	}
	const FCityTrafficRouteLeg& Leg = Vehicle.Route[LegIndex];
	if (!ValidateLeg(Leg, Network))
		return false;
	const FCityRoadEdge& Edge = Network.Edges[Leg.EdgeId];
	if (Edge.Cells.Num() < 2)
		return false;
	const double CellDistance =
	    FMath::Clamp(Distance / TrafficUnitsPerCell, 0.0, static_cast<double>(Edge.Cells.Num() - 1));
	const int32 Index = FMath::Min(FMath::FloorToInt(CellDistance), Edge.Cells.Num() - 2);
	const FCityCellCoord& A = Edge.Cells[Leg.bReverse ? Edge.Cells.Num() - 1 - Index : Index];
	const FCityCellCoord& B = Edge.Cells[Leg.bReverse ? Edge.Cells.Num() - 2 - Index : Index + 1];
	OutGridPosition = FMath::Lerp(FVector(A.X, A.Y, A.RoadLevel), FVector(B.X, B.Y, B.RoadLevel), CellDistance - Index);
	OutDirection = FVector(B.X - A.X, B.Y - A.Y, 0).GetSafeNormal();
	return true;
}

// The narrow dirt street uses its curb margin; the motor silhouettes remain
// clear of the sidewalk's pedestrian center. Other roads have more carriageway.
double FCityTrafficSimulation::SampleMotorLaneOffsetCm(const FCityTrafficVehicleState& Vehicle,
    const FCityTrafficVehicleState* Previous, const double Alpha, const FName RoadDefinitionId)
{
	auto Offset = [&](const FCityTrafficVehicleState& V) {
		const int32 Lane = V.Route.IsValidIndex(V.RouteLegIndex) ? V.Route[V.RouteLegIndex].LaneIndex : 0;
		if (V.bYielding)
			return RoadDefinitionId == TEXT("DirtRoad") ? 160.0 : RoadDefinitionId == TEXT("Avenue") ? 355.0
			       : RoadDefinitionId == TEXT("Highway") ? 415.0 : 195.0;
		if (V.bEmergencyResponding && RoadDefinitionId != TEXT("Highway") && RoadDefinitionId != TEXT("Avenue"))
			return V.bCompleted ? 85.0 : 0.0;
		return 85.0 + Lane * 170.0;
	};
	const bool Same = Previous && Previous->OriginEntityId == Vehicle.OriginEntityId &&
	                  Previous->DestinationEntityId == Vehicle.DestinationEntityId &&
	                  Previous->StartDistanceUnits == Vehicle.StartDistanceUnits;
	const double T = FMath::Clamp(Alpha, 0.0, 1.0);
	return FMath::Lerp(Same ? Offset(*Previous) : Offset(Vehicle), Offset(Vehicle), T * T * (3.0 - 2.0 * T));
}

double FCityTrafficSimulation::SamplePedestrianOffsetCm(const FCityTrafficVehicleState& Vehicle,
    const FCityTrafficVehicleState* Previous, const double Alpha, const double WidthCm)
{
	const double T = FMath::Clamp(Alpha, 0.0, 1.0);
	const double Track = FMath::Lerp(Previous ? double(Previous->PedestrianTrack) : double(Vehicle.PedestrianTrack),
	                                double(Vehicle.PedestrianTrack), T * T * (3.0 - 2.0 * T));
	return (Track - .5) * WidthCm * .5;
}

FVector FCityTrafficSimulation::SampleMotorHeading(const FCityTrafficVehicleState& Vehicle,
    const FCityTrafficVehicleState* Previous, const double Alpha, const FName RoadId,
    const FVector& Forward, const double TravelCm)
{
	if (!Previous || TravelCm < 1.0) return Forward;
	const double Delta = SampleMotorLaneOffsetCm(Vehicle, Previous, 1.0, RoadId) -
	                     SampleMotorLaneOffsetCm(Vehicle, Previous, 0.0, RoadId);
	const double T = FMath::Clamp(Alpha, 0.0, 1.0);
	return (Forward * TravelCm + FVector(-Forward.Y, Forward.X, 0) * Delta * 6.0 * T * (1.0 - T)).GetSafeNormal();
}

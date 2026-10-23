#include "Simulation/CityTrafficSimulation.h"
#include "Simulation/CityPedestrianPaths.h"

namespace
{
bool HasPassingLanes(const FCityRoadEdge& Edge)
{
	return Edge.ForwardLanes > 1 && Edge.ReverseLanes > 1;
}

uint64 LaneKey(const int32 EdgeId, const bool bReverse, const int32 LaneIndex)
{
	return (static_cast<uint64>(static_cast<uint32>(EdgeId)) << 32) |
	       (static_cast<uint64>(bReverse ? 1u : 0u) << 31) | static_cast<uint32>(LaneIndex);
}

uint64 ConnectorReservationKey(const FCityLaneConnector& Connector, const int32 Lane)
{
	return (static_cast<uint64>(static_cast<uint32>(Connector.NodeId)) << 32) |
	       (static_cast<uint64>(static_cast<uint16>(Connector.OutgoingEdgeId)) << 16) |
	       static_cast<uint16>(Lane);
}
} // namespace

void FCityTrafficSimulation::Reset()
{
	Snapshot = FCityTrafficSnapshot();
	CitizenRouteBudget = 32;
}

uint64 FCityTrafficSimulation::GetNetworkFingerprint(const FCityRoadNetworkSnapshot& Network)
{
	// Explicit integer mixing is stable across platforms and ignores unrelated
	// placement revisions. Edge IDs alone are not stable after a road edit.
	uint64 Hash = 14695981039346656037ull;
	auto Mix = [&](const uint64 Value) { Hash = (Hash ^ Value) * 1099511628211ull; };
	Mix(Network.Nodes.Num());
	for (const FCityRoadNode& Node : Network.Nodes)
	{
		Mix(Node.Id);
		Mix(Node.Cell.X);
		Mix(Node.Cell.Y);
	}
	Mix(Network.Edges.Num());
	for (const FCityRoadEdge& Edge : Network.Edges)
	{
		Mix(Edge.Id);
		Mix(Edge.StartNodeId);
		Mix(Edge.EndNodeId);
		Mix(Edge.ForwardLanes);
		Mix(Edge.ReverseLanes);
		Mix(Edge.SpeedUnitsPerStep);
		Mix(Edge.Capacity);
		Mix(static_cast<uint8>(Edge.Access));
		Mix(static_cast<uint8>(Edge.RoadClass));
		Mix(Edge.Cells.Num());
		for (const FCityCellCoord& Cell : Edge.Cells)
		{
			Mix(Cell.X);
			Mix(Cell.Y);
		}
	}
	// Extra lane connectors are derived from the already hashed lane counts;
	// preserve physical network identity for existing current-format routes.
	int32 BaseConnectorCount = 0;
	for (const auto& C : Network.Connectors)
		if (C.IncomingLane == 0 && C.OutgoingLane == 0) ++BaseConnectorCount;
	Mix(BaseConnectorCount);
	for (const FCityLaneConnector& Connector : Network.Connectors)
	{
		if (Connector.IncomingLane || Connector.OutgoingLane) continue;
		Mix(Connector.NodeId);
		Mix(Connector.IncomingEdgeId);
		Mix(Connector.OutgoingEdgeId);
		Mix(Connector.IncomingLane);
		Mix(Connector.OutgoingLane);
		Mix(static_cast<uint8>(Connector.Turn));
	}
	// Keep the existing all-ground hash stable. Layered geometry gets an explicit
	// suffix for the layered geometry.
	for (const auto& Edge : Network.Edges)
		for (const auto& Cell : Edge.Cells)
			if (Cell.RoadLevel)
			{
				Mix(0x524F41444C455645ull);
				Mix(Edge.Id);
				Mix(Cell.X);
				Mix(Cell.Y);
				Mix(Cell.RoadLevel);
			}
	return Hash;
}

bool FCityTrafficSimulation::RestoreState(const FCityTrafficSnapshot& State,
                                          const FCityRoadNetworkSnapshot& Network,
                                          const int64 MaximumFixedStepId, FString& OutError)
{
	if (State.FixedStepId < 0 || State.FixedStepId > MaximumFixedStepId ||
	    ((!State.Vehicles.IsEmpty() || !State.WaitingDepartures.IsEmpty()) &&
	     State.NetworkFingerprint != GetNetworkFingerprint(Network)) ||
	    State.WaitingDepartures.Num() + State.Vehicles.Num() > 2048)
	{
		OutError = TEXT("Traffic clock or road-network fingerprint is invalid");
		return false;
	}
	FCityTrafficSimulation Candidate;
	for (const FCityTrafficVehicleState& Vehicle : State.Vehicles)
	{
		if (Vehicle.CompletedStep > State.FixedStepId)
		{
			OutError = TEXT("Traffic completion is ahead of its clock");
			return false;
		}
		if (!Candidate.AddVehicle(Vehicle, Network, &OutError))
			return false;
	}
	TSet<uint64> WaitingIds;
	for (const auto& Departure : State.WaitingDepartures)
	{
		FCityTrafficSimulation Empty;
		if (Departure.SourceId == 0 || Departure.bCompleted || Departure.bYielding ||
		    Departure.RouteLegIndex != 0 || Departure.DistanceUnits != Departure.StartDistanceUnits ||
		    Departure.WalkDistanceUnits != 0 ||
		    Departure.SpeedUnitsPerStep != 0 || Departure.CompletedStep != 0 ||
		    Departure.DepartureRequestedStep < 0 || Departure.DepartureRequestedStep > State.FixedStepId ||
		    WaitingIds.Contains(Departure.Id) ||
		    Candidate.Snapshot.Vehicles.ContainsByPredicate([&](const auto& V) { return V.Id == Departure.Id; }) ||
		    !Empty.AddVehicle(Departure, Network, &OutError))
		{
			OutError = TEXT("Invalid waiting traffic departure");
			return false;
		}
		WaitingIds.Add(Departure.Id);
	}
	Candidate.Snapshot.WaitingDepartures = State.WaitingDepartures;
	Candidate.Snapshot.FixedStepId = State.FixedStepId;
	Candidate.Snapshot.NetworkFingerprint = State.Vehicles.IsEmpty() && State.WaitingDepartures.IsEmpty()
	                                            ? 0 : State.NetworkFingerprint;
	// Lane loads are derived, never trusted from a save.
	Snapshot = MoveTemp(Candidate.Snapshot);
	OutError.Reset();
	return true;
}

bool FCityTrafficSimulation::AdvanceToStep(const FCityRoadNetworkSnapshot& Network, const int64 FixedStepId,
                                           FString& OutError)
{
	if (FixedStepId < Snapshot.FixedStepId || FixedStepId < 0 ||
	    ((!Snapshot.Vehicles.IsEmpty() || !Snapshot.WaitingDepartures.IsEmpty()) &&
	     Snapshot.NetworkFingerprint != GetNetworkFingerprint(Network)))
	{
		OutError = TEXT("Traffic cannot rewind or advance routes on a changed road network");
		return false;
	}
	if (Snapshot.Vehicles.IsEmpty() && Snapshot.WaitingDepartures.IsEmpty())
		Snapshot.FixedStepId = FixedStepId;
	while (Snapshot.FixedStepId < FixedStepId)
		AdvanceOneStep(Network, Snapshot.FixedStepId + 1);
	OutError.Reset();
	return true;
}

int32 FCityTrafficSimulation::GetEdgeLengthUnits(const FCityRoadEdge& Edge)
{
	return FMath::Max(1, Edge.Cells.Num() - 1) * TrafficUnitsPerCell;
}

int32 FCityTrafficSimulation::GetSpeedLimitUnitsPerStep(const FCityRoadEdge& Edge)
{
	return FMath::Max(1, Edge.SpeedUnitsPerStep) * TrafficUnitsPerCell;
}

bool FCityTrafficSimulation::ValidateLeg(const FCityTrafficRouteLeg& Leg,
                                         const FCityRoadNetworkSnapshot& Network)
{
	if (!Network.Edges.IsValidIndex(Leg.EdgeId) || Leg.LaneIndex < 0)
		return false;
	const FCityRoadEdge& Edge = Network.Edges[Leg.EdgeId];
	return Leg.LaneIndex < (Leg.bReverse ? Edge.ReverseLanes : Edge.ForwardLanes);
}

int32 FCityTrafficSimulation::GetDepartureNode(const FCityTrafficRouteLeg& Leg, const FCityRoadEdge& Edge)
{
	return Leg.bReverse ? Edge.EndNodeId : Edge.StartNodeId;
}

int32 FCityTrafficSimulation::GetArrivalNode(const FCityTrafficRouteLeg& Leg, const FCityRoadEdge& Edge)
{
	return Leg.bReverse ? Edge.StartNodeId : Edge.EndNodeId;
}

const FCityLaneConnector* FCityTrafficSimulation::FindConnector(const FCityTrafficRouteLeg& Incoming,
                                                                const FCityTrafficRouteLeg& Outgoing,
                                                                const FCityRoadNetworkSnapshot& Network)
{
	if (!ValidateLeg(Incoming, Network) || !ValidateLeg(Outgoing, Network))
		return nullptr;
	const FCityRoadEdge& IncomingEdge = Network.Edges[Incoming.EdgeId];
	const FCityRoadEdge& OutgoingEdge = Network.Edges[Outgoing.EdgeId];
	const int32 NodeId = GetArrivalNode(Incoming, IncomingEdge);
	if (NodeId != GetDepartureNode(Outgoing, OutgoingEdge))
		return nullptr;
	return Network.Connectors.FindByPredicate([&](const FCityLaneConnector& Connector) {
		return Connector.NodeId == NodeId && Connector.IncomingEdgeId == Incoming.EdgeId &&
		       Connector.OutgoingEdgeId == Outgoing.EdgeId && Connector.IncomingLane == Incoming.LaneIndex &&
		       Connector.OutgoingLane == Outgoing.LaneIndex;
	});
}

bool FCityTrafficSimulation::AddVehicle(const FCityTrafficVehicleState& Vehicle,
                                        const FCityRoadNetworkSnapshot& Network, FString* OutError)
{
	const uint64 Fingerprint = GetNetworkFingerprint(Network);
	if (!Snapshot.Vehicles.IsEmpty() && Snapshot.NetworkFingerprint != Fingerprint)
	{
		if (OutError)
			*OutError = TEXT("Traffic routes belong to a different road network");
		return false;
	}
	if (Vehicle.Id == 0 || Vehicle.Route.IsEmpty() || Vehicle.RouteLegIndex < 0 ||
	    (Vehicle.SourceId != 0 && (Vehicle.SourceId > MAX_uint64 / 2 ||
	                               Vehicle.Id != Vehicle.SourceId * 2 + (Vehicle.bTransitTrip ? 1 : 0))) ||
	    Vehicle.RouteLegIndex >= Vehicle.Route.Num() || Vehicle.DistanceUnits < 0 ||
	    Vehicle.SpeedUnitsPerStep < 0 || Vehicle.LengthUnits <= 0 ||
	    Vehicle.PedestrianTrack < 0 || Vehicle.PedestrianTrack > 1 ||
	    (Vehicle.VehicleType != ECityTripVehicleType::Walk && Vehicle.PedestrianTrack != 0) ||
	    (Vehicle.bEmergencyResponding && (Vehicle.VehicleType < ECityTripVehicleType::PoliceCar ||
	                                      Vehicle.VehicleType > ECityTripVehicleType::Ambulance)) ||
	    (Vehicle.bYielding &&
	     (Vehicle.VehicleType == ECityTripVehicleType::Walk || Vehicle.bCompleted ||
	      Vehicle.SpeedUnitsPerStep > (Vehicle.bEmergencyResponding ? 0 : AccelerationUnitsPerStep))) ||
	    Snapshot.Vehicles.ContainsByPredicate(
	        [&](const FCityTrafficVehicleState& Existing) { return Existing.Id == Vehicle.Id; }))
	{
		if (OutError)
			*OutError = TEXT("Traffic vehicle has invalid or duplicate identity/state");
		return false;
	}
	for (int32 LegIndex = 0; LegIndex < Vehicle.Route.Num(); ++LegIndex)
	{
		if (!ValidateLeg(Vehicle.Route[LegIndex], Network) ||
		    (LegIndex > 0 && !FindConnector(Vehicle.Route[LegIndex - 1], Vehicle.Route[LegIndex], Network)))
		{
			if (OutError)
				*OutError = TEXT("Traffic vehicle route has an invalid lane or connector");
			return false;
		}
	}
	if (Vehicle.DistanceUnits >
	    GetEdgeLengthUnits(Network.Edges[Vehicle.Route[Vehicle.RouteLegIndex].EdgeId]))
	{
		if (OutError)
			*OutError = TEXT("Traffic vehicle starts beyond its route edge");
		return false;
	}
	const FCityTrafficRouteLeg& CurrentLeg = Vehicle.Route[Vehicle.RouteLegIndex];
	const FCityRoadEdge& CurrentEdge = Network.Edges[CurrentLeg.EdgeId];
	const int32 LastLength = GetEdgeLengthUnits(Network.Edges[Vehicle.Route.Last().EdgeId]);
	if (Vehicle.StartDistanceUnits < 0 ||
	    Vehicle.StartDistanceUnits > GetEdgeLengthUnits(Network.Edges[Vehicle.Route[0].EdgeId]) ||
	    Vehicle.EndDistanceUnits < -1 || Vehicle.EndDistanceUnits > LastLength || Vehicle.CompletedStep < 0 ||
	    static_cast<uint8>(Vehicle.VehicleType) > static_cast<uint8>(ECityTripVehicleType::GarbageTruck) ||
	    (Vehicle.RouteLegIndex == 0 && Vehicle.DistanceUnits < Vehicle.StartDistanceUnits) ||
	    (Vehicle.RouteLegIndex == Vehicle.Route.Num() - 1 && Vehicle.EndDistanceUnits >= 0 &&
	     Vehicle.DistanceUnits > Vehicle.EndDistanceUnits) ||
	    (Vehicle.Route.Num() == 1 && Vehicle.EndDistanceUnits >= 0 &&
	     Vehicle.EndDistanceUnits < Vehicle.StartDistanceUnits))
	{
		if (OutError)
			*OutError = TEXT("Invalid traffic endpoint, mode, or completion clock");
		return false;
	}
	const ECityRoadAccess Access =
	    Vehicle.VehicleType == ECityTripVehicleType::Walk           ? ECityRoadAccess::Pedestrian
	    : Vehicle.VehicleType == ECityTripVehicleType::FreightTruck ? ECityRoadAccess::Freight
	                                                                : ECityRoadAccess::Car;
	for (const FCityTrafficRouteLeg& Leg : Vehicle.Route)
	{
		if (!EnumHasAnyFlags(Network.Edges[Leg.EdgeId].Access, Access))
		{
			if (OutError)
				*OutError = TEXT("Traffic mode is not allowed on its route");
			return false;
		}
	}
	if (Vehicle.SpeedUnitsPerStep > (Vehicle.VehicleType == ECityTripVehicleType::Walk
	                                     ? 500
	                                     : GetSpeedLimitUnitsPerStep(CurrentEdge)) ||
	    (Vehicle.bCompleted &&
	     (Vehicle.RouteLegIndex != Vehicle.Route.Num() - 1 ||
	      Vehicle.DistanceUnits !=
	          (Vehicle.EndDistanceUnits >= 0 ? Vehicle.EndDistanceUnits : GetEdgeLengthUnits(CurrentEdge)))))
	{
		if (OutError)
			*OutError = TEXT("Traffic speed or completed-route position is invalid");
		return false;
	}
	if (!Vehicle.WalkPath.IsEmpty())
	{
		if (Vehicle.WalkPath.Num() > 32768) { if (OutError) *OutError = TEXT("Pedestrian path too long"); return false; }
		for (const auto& P : Vehicle.WalkPath)
			if (P.X < 0 || P.Y < 0 || P.X > 1000000 || P.Y > 1000000 || P.Z < 0 || P.Z > 1)
			{ if (OutError) *OutError = TEXT("Invalid pedestrian path coordinate"); return false; }
		TSet<FCityCellCoord> Walkable;
		for (const auto& E : Network.Edges) if (EnumHasAnyFlags(E.Access, ECityRoadAccess::Pedestrian))
			for (const auto& C : E.Cells) Walkable.Add(C);
		for (int32 I = 1; I < Vehicle.WalkPath.Num(); ++I)
		{
			const auto A = Vehicle.WalkPath[I-1], B = Vehicle.WalkPath[I];
			bool LevelJoin = A.Z == B.Z;
			if (!LevelJoin)
			{
				const FCityCellCoord CA(A.X/1000,A.Y/1000,A.Z), CB(B.X/1000,B.Y/1000,B.Z);
				for (const auto& E : Network.Edges) if (EnumHasAnyFlags(E.Access, ECityRoadAccess::Pedestrian))
					for (int32 J=1; J<E.Cells.Num(); ++J)
						LevelJoin |= (E.Cells[J-1]==CA && E.Cells[J]==CB)||(E.Cells[J-1]==CB && E.Cells[J]==CA);
			}
			if (FMath::Abs(A.X-B.X) > 2000 || FMath::Abs(A.Y-B.Y) > 2000 || !LevelJoin ||
			    (I < Vehicle.WalkPath.Num()-1 && !Walkable.Contains(FCityCellCoord(B.X/1000,B.Y/1000,B.Z))))
			{ if (OutError) *OutError = TEXT("Pedestrian path leaves its walkable road level"); return false; }
		}
		const int32 Total = CityPedestrianPaths::Length(Vehicle.WalkPath);
		if (Vehicle.VehicleType != ECityTripVehicleType::Walk || Vehicle.WalkPath.Num() < 2 ||
		    Vehicle.WalkDistanceUnits < 0 || Vehicle.WalkDistanceUnits > Total ||
		    Vehicle.bCompleted != (Vehicle.WalkDistanceUnits == Total))
		{ if (OutError) *OutError = TEXT("Invalid pedestrian path progress"); return false; }
		if (!Vehicle.bCompleted && !CityPedestrianPaths::HasRoom(Vehicle, Vehicle.WalkDistanceUnits, Snapshot.Vehicles))
		{ if (OutError) *OutError = TEXT("Pedestrian departure is occupied"); return false; }
	}
	for (const FCityTrafficVehicleState& Existing : Snapshot.Vehicles)
	{
		if (!Vehicle.WalkPath.IsEmpty() || !Existing.WalkPath.IsEmpty() || Vehicle.bCompleted || Existing.bCompleted ||
		    (Vehicle.VehicleType == ECityTripVehicleType::Walk && Vehicle.PedestrianTrack != Existing.PedestrianTrack) ||
		    (Vehicle.VehicleType == ECityTripVehicleType::Walk) !=
		        (Existing.VehicleType == ECityTripVehicleType::Walk))
			continue;
		const FCityTrafficRouteLeg& ExistingLeg = Existing.Route[Existing.RouteLegIndex];
		if (ExistingLeg.EdgeId != CurrentLeg.EdgeId || ExistingLeg.bReverse != CurrentLeg.bReverse ||
		    ExistingLeg.LaneIndex != CurrentLeg.LaneIndex)
			continue;
		// A responding vehicle passes a stopped curbside vehicle in a separate
		// lateral track. Ordinary vehicles still keep their longitudinal queue.
		if ((Vehicle.bEmergencyResponding && !Vehicle.bYielding && !Existing.bEmergencyResponding &&
		     Existing.bYielding) ||
		    (Existing.bEmergencyResponding && !Existing.bYielding && !Vehicle.bEmergencyResponding &&
		     Vehicle.bYielding))
			continue;
		const bool bExistingAhead = Existing.DistanceUnits >= Vehicle.DistanceUnits;
		const int64 Separation =
		    FMath::Abs(static_cast<int64>(Existing.DistanceUnits) - Vehicle.DistanceUnits);
		if (Separation < static_cast<int64>(bExistingAhead ? Existing.LengthUnits : Vehicle.LengthUnits) +
		                     FollowingGap(Vehicle))
		{
			if (OutError)
				*OutError = TEXT("Traffic vehicles violate the same-lane safe gap");
			return false;
		}
	}
	Snapshot.Vehicles.Add(Vehicle);
	Snapshot.NetworkFingerprint = Fingerprint;
	Snapshot.Vehicles.Sort(
	    [](const FCityTrafficVehicleState& A, const FCityTrafficVehicleState& B) { return A.Id < B.Id; });
	RebuildLaneLoads(Network);
	return true;
}

void FCityTrafficSimulation::AdvanceOneStep(const FCityRoadNetworkSnapshot& Network, const int64 FixedStepId)
{
	FCityFrameTimingScope AdvanceScope(Network.RoutingTiming ? &Network.RoutingTiming->TrafficAdvance
	                                                         : nullptr);
	if (FixedStepId != Snapshot.FixedStepId + 1 ||
	    ((!Snapshot.Vehicles.IsEmpty() || !Snapshot.WaitingDepartures.IsEmpty()) &&
	     Snapshot.NetworkFingerprint != GetNetworkFingerprint(Network)))
		return;
	AdmitDepartures(Network);
	auto Initial = Snapshot.Vehicles;
	auto FreeTrack = [&](const FCityTrafficVehicleState& V, const int32 Track, const bool Pedestrian) {
		const auto& Leg = V.Route[V.RouteLegIndex];
		for (const auto* Records : {&Initial, &Snapshot.Vehicles})
			for (const auto& Other : *Records)
			{
				if (Other.Id == V.Id || Other.bCompleted ||
				    (Other.VehicleType == ECityTripVehicleType::Walk) != Pedestrian) continue;
				const auto& OtherLeg = Other.Route[Other.RouteLegIndex];
				if (OtherLeg.EdgeId != Leg.EdgeId || OtherLeg.bReverse != Leg.bReverse ||
				    (Pedestrian ? Other.PedestrianTrack : OtherLeg.LaneIndex) != Track) continue;
				const int32 D = Other.DistanceUnits - V.DistanceUnits;
				const int32 Margin = Pedestrian ? 500 : FMath::Max(V.SpeedUnitsPerStep, Other.SpeedUnitsPerStep);
				if (FMath::Abs(D) < (D >= 0 ? Other.LengthUnits : V.LengthUnits) + FollowingGap(V) + Margin)
					return false;
			}
		return true;
	};
	for (auto& V : Snapshot.Vehicles)
	{
		if (V.bCompleted || V.Route.IsEmpty() || !V.WalkPath.IsEmpty()) continue;
		auto& Leg = V.Route[V.RouteLegIndex];
		if (V.VehicleType == ECityTripVehicleType::Walk)
		{
			const bool Blocked = Initial.ContainsByPredicate([&](const auto& Other) {
				if (Other.bCompleted || Other.VehicleType != ECityTripVehicleType::Walk || Other.PedestrianTrack != V.PedestrianTrack) return false;
				const auto& L = Other.Route[Other.RouteLegIndex];
				return L.EdgeId == Leg.EdgeId && L.bReverse == Leg.bReverse && Other.DistanceUnits > V.DistanceUnits &&
				       Other.DistanceUnits - Other.LengthUnits - PedestrianGapUnits - V.DistanceUnits < 500;
			});
			if (Blocked && FreeTrack(V, 1 - V.PedestrianTrack, true)) V.PedestrianTrack = 1 - V.PedestrianTrack;
		}
		else if (!V.bEmergencyResponding && HasPassingLanes(Network.Edges[Leg.EdgeId]) && Leg.LaneIndex == 0)
		{
			const bool Response = Initial.ContainsByPredicate([&](const auto& E) {
				if (!E.bEmergencyResponding || E.bCompleted) return false;
				int32 Ahead = -E.DistanceUnits;
				for (int32 I = E.RouteLegIndex; I < E.Route.Num() && Ahead <= 12000; ++I)
				{
					const auto& R = E.Route[I];
					if (R.EdgeId == Leg.EdgeId && R.bReverse == Leg.bReverse && Ahead + V.DistanceUnits >= -E.LengthUnits && Ahead + V.DistanceUnits <= 12000) return true;
					Ahead += GetEdgeLengthUnits(Network.Edges[R.EdgeId]);
				}
				return false;
			});
			if (Response && (Leg.bReverse ? Network.Edges[Leg.EdgeId].ReverseLanes : Network.Edges[Leg.EdgeId].ForwardLanes) > 1 && FreeTrack(V, 1, false))
			{ Leg.LaneIndex = 1; V.bYielding = false; }
		}
	}
	TArray<FCityTrafficVehicleState> Previous = Snapshot.Vehicles;
	Previous.Sort(
	    [](const FCityTrafficVehicleState& A, const FCityTrafficVehicleState& B) { return A.Id < B.Id; });
	// Only emergency journeys participate in the look-ahead scan (at most16
	// in normal gameplay). Read one immutable snapshot for order independence.
	TSet<uint64> YieldIds;
	TSet<uint64> CurbClearanceIds;
	TSet<int32> EmergencyNodes;
	for (const auto& Emergency : Previous)
	{
		if (!Emergency.bEmergencyResponding || Emergency.bCompleted ||
		    !Emergency.Route.IsValidIndex(Emergency.RouteLegIndex))
			continue;
		const int32 LookAhead = FMath::Max(
		    6000, GetSpeedLimitUnitsPerStep(Network.Edges[Emergency.Route[Emergency.RouteLegIndex].EdgeId]) +
		              Emergency.LengthUnits + SafeFollowingGapUnits);
		int32 Ahead = -Emergency.DistanceUnits;
		for (int32 I = Emergency.RouteLegIndex; I < Emergency.Route.Num() && Ahead <= LookAhead; ++I)
		{
			const auto& RouteLeg = Emergency.Route[I];
			if (!ValidateLeg(RouteLeg, Network))
				break;
			const auto& RouteEdge = Network.Edges[RouteLeg.EdgeId];
			const int32 Length = I == Emergency.Route.Num() - 1 && Emergency.EndDistanceUnits >= 0
			                         ? Emergency.EndDistanceUnits
			                         : GetEdgeLengthUnits(RouteEdge);
			for (const auto& Candidate : Previous)
			{
				if (Candidate.Id == Emergency.Id || Candidate.bCompleted ||
				    Candidate.VehicleType == ECityTripVehicleType::Walk ||
				    !Candidate.Route.IsValidIndex(Candidate.RouteLegIndex))
					continue;
				const auto& CandidateLeg = Candidate.Route[Candidate.RouteLegIndex];
				if (CandidateLeg.EdgeId != RouteLeg.EdgeId)
					continue;
				if (HasPassingLanes(RouteEdge) &&
				    (CandidateLeg.bReverse != RouteLeg.bReverse || CandidateLeg.LaneIndex > 0)) continue;
				// Opposing responses share one cleared corridor: lower ID passes
				// first while the other crew pulls aside. Same-direction crews queue.
				if (Candidate.bEmergencyResponding &&
				    (Candidate.Id < Emergency.Id || CandidateLeg.bReverse == RouteLeg.bReverse))
					continue;
				const int32 Position = CandidateLeg.bReverse == RouteLeg.bReverse
				                           ? Candidate.DistanceUnits
				                           : GetEdgeLengthUnits(RouteEdge) - Candidate.DistanceUnits;
				const int32 Separation = Ahead + Position;
				if (Position <= Length + Emergency.LengthUnits + SafeFollowingGapUnits &&
				    Separation >= -Emergency.LengthUnits - SafeFollowingGapUnits && Separation <= LookAhead)
				{
					YieldIds.Add(Candidate.Id);
					// Drain the curb queue beyond a multi-lane exit. Stopping it
					// would trap the ordinary cars ahead of the responder in both
					// approach lanes, preventing the responder reaching that exit.
					if (I > Emergency.RouteLegIndex && !Candidate.bEmergencyResponding &&
					    CandidateLeg.bReverse == RouteLeg.bReverse && !HasPassingLanes(RouteEdge) &&
					    HasPassingLanes(Network.Edges[Emergency.Route[Emergency.RouteLegIndex].EdgeId]))
						CurbClearanceIds.Add(Candidate.Id);
				}
			}
			if (I == Emergency.RouteLegIndex && I + 1 < Emergency.Route.Num() &&
			    Length - Emergency.DistanceUnits <=
			        GetSpeedLimitUnitsPerStep(RouteEdge) + SafeFollowingGapUnits)
				EmergencyNodes.Add(GetArrivalNode(RouteLeg, RouteEdge));
			Ahead += Length;
		}
	}
	// A responder must have a free curb slot before yielding. If an ordinary
	// curb queue occupies it, hold the responder in the center and let that queue
	// crawl forward on its separate track until there is room to pull aside.
	TSet<uint64> DeferredYieldIds;
	for (const auto& Emergency : Previous)
	{
		if (!Emergency.bEmergencyResponding || Emergency.bCompleted || !YieldIds.Contains(Emergency.Id) ||
		    !Emergency.Route.IsValidIndex(Emergency.RouteLegIndex))
			continue;
		const auto& Leg = Emergency.Route[Emergency.RouteLegIndex];
		for (const auto& Other : Previous)
		{
			if (Other.Id == Emergency.Id || Other.bCompleted ||
			    Other.VehicleType == ECityTripVehicleType::Walk ||
			    !Other.Route.IsValidIndex(Other.RouteLegIndex) ||
			    (!Other.bEmergencyResponding && !Other.bYielding && !YieldIds.Contains(Other.Id)))
				continue;
			const auto& OtherLeg = Other.Route[Other.RouteLegIndex];
			const int64 Separation = static_cast<int64>(Other.DistanceUnits) - Emergency.DistanceUnits;
			if (OtherLeg.EdgeId == Leg.EdgeId && OtherLeg.bReverse == Leg.bReverse &&
			    OtherLeg.LaneIndex == Leg.LaneIndex &&
			    FMath::Abs(Separation) <
			        (Separation >= 0 ? Other.LengthUnits : Emergency.LengthUnits) + SafeFollowingGapUnits)
				DeferredYieldIds.Add(Emergency.Id);
		}
		if (!DeferredYieldIds.Contains(Emergency.Id))
			continue;
		for (const auto& Other : Previous)
		{
			if (Other.bCompleted || Other.bEmergencyResponding ||
			    Other.VehicleType == ECityTripVehicleType::Walk ||
			    !Other.Route.IsValidIndex(Other.RouteLegIndex) ||
			    (!Other.bYielding && !YieldIds.Contains(Other.Id)))
				continue;
			const auto& OtherLeg = Other.Route[Other.RouteLegIndex];
			if (OtherLeg.EdgeId == Leg.EdgeId && OtherLeg.bReverse == Leg.bReverse &&
			    OtherLeg.LaneIndex == Leg.LaneIndex)
				CurbClearanceIds.Add(Other.Id);
		}
	}
	// Let responders reserve connectors first, retaining stable ID tie-breaking.
	Snapshot.Vehicles.Sort([](const auto& A, const auto& B) {
		return A.bEmergencyResponding != B.bEmergencyResponding ? A.bEmergencyResponding : A.Id < B.Id;
	});
	TSet<uint64> ReservedOutgoingLanes;

	for (FCityTrafficVehicleState& Vehicle : Snapshot.Vehicles)
	{
		if (Vehicle.bCompleted || !Vehicle.Route.IsValidIndex(Vehicle.RouteLegIndex))
			continue;
		if (!Vehicle.WalkPath.IsEmpty())
		{
			const int32 Total = CityPedestrianPaths::Length(Vehicle.WalkPath);
			int32 Move = FMath::Min(500, Total - Vehicle.WalkDistanceUnits);
			// Small swept steps preserve same-track following even at a merging sidewalk.
			int32 Accepted = 0;
			for (int32 D = FMath::Min(50, Move); D <= Move && D > 0; D = FMath::Min(D + 50, Move))
			{
				if (!CityPedestrianPaths::HasRoom(Vehicle, Vehicle.WalkDistanceUnits + D, Snapshot.Vehicles)) break;
				Accepted = D; if (D == Move) break;
			}
			Vehicle.WalkDistanceUnits += Accepted; Vehicle.SpeedUnitsPerStep = Accepted;
			CityPedestrianPaths::UpdateRoadPosition(Vehicle, Network);
			if (Vehicle.WalkDistanceUnits == Total)
			{
				Vehicle.RouteLegIndex = Vehicle.Route.Num() - 1;
				Vehicle.DistanceUnits = Vehicle.EndDistanceUnits;
				Vehicle.bCompleted = true; Vehicle.CompletedStep = FixedStepId;
			}
			continue;
		}
		const FCityTrafficRouteLeg& Leg = Vehicle.Route[Vehicle.RouteLegIndex];
		if (!ValidateLeg(Leg, Network))
		{
			Vehicle.bCompleted = true;
			Vehicle.CompletedStep = FixedStepId;
			Vehicle.SpeedUnitsPerStep = 0;
			continue;
		}
		const FCityRoadEdge& Edge = Network.Edges[Leg.EdgeId];
		const int32 EdgeLength =
		    Vehicle.RouteLegIndex == Vehicle.Route.Num() - 1 && Vehicle.EndDistanceUnits >= 0
		        ? Vehicle.EndDistanceUnits
		        : GetEdgeLengthUnits(Edge);
		const bool bWasYielding = Vehicle.bYielding;
		// A blocked outer-lane merge must keep its inner queue moving. Stopping
		// here can lock both lanes and every responder behind them forever.
		// Continue with normal leader/connector gaps until a safe merge opens;
		// the responder follows this lane rather than passing through it.
		bool bHold = YieldIds.Contains(Vehicle.Id) &&
		             (Vehicle.bEmergencyResponding || !HasPassingLanes(Edge));
		if (DeferredYieldIds.Contains(Vehicle.Id))
		{
			Vehicle.bYielding = false;
			Vehicle.SpeedUnitsPerStep = 0;
			continue;
		}
		bHold |= CurbClearanceIds.Contains(Vehicle.Id);
		if (bWasYielding && !bHold)
		{
			// Wait for the normal safe gap on both sides. Followers also obey
			// the stopped vehicle, so requiring extra space would deadlock a queue.
			for (const auto& Other : Previous)
			{
				if (Other.Id == Vehicle.Id || Other.bCompleted || Other.bYielding ||
				    Other.VehicleType == ECityTripVehicleType::Walk ||
				    !Other.Route.IsValidIndex(Other.RouteLegIndex))
					continue;
				const auto& OtherLeg = Other.Route[Other.RouteLegIndex];
				if (OtherLeg.EdgeId != Leg.EdgeId || OtherLeg.bReverse != Leg.bReverse ||
				    OtherLeg.LaneIndex != Leg.LaneIndex)
					continue;
				const int32 Separation = Other.DistanceUnits - Vehicle.DistanceUnits;
				const int32 Gap =
				    (Separation >= 0 ? Other.LengthUnits : Vehicle.LengthUnits) + SafeFollowingGapUnits;
				if (FMath::Abs(Separation) < Gap)
					bHold = true;
			}
		}
		Vehicle.bYielding = bHold;
		const bool bClearCurb = bHold && bWasYielding && CurbClearanceIds.Contains(Vehicle.Id);
		if (bHold && (bWasYielding || Vehicle.bEmergencyResponding) && !bClearCurb)
		{
			Vehicle.SpeedUnitsPerStep = 0;
			continue;
		}
		const int32 TargetSpeed =
		    Vehicle.VehicleType == ECityTripVehicleType::Walk ? 500 : GetSpeedLimitUnitsPerStep(Edge);
		int32 Movement = FMath::Min(TargetSpeed, Vehicle.SpeedUnitsPerStep + AccelerationUnitsPerStep);
		if (bClearCurb || bHold)
			Movement = FMath::Min(Movement, AccelerationUnitsPerStep);

		int32 LeaderLimit = DepartureStopLimit(Leg, Vehicle.VehicleType == ECityTripVehicleType::Walk,
		                                       Vehicle.DistanceUnits, Vehicle.bEmergencyResponding,
		                                       Vehicle.RouteLegIndex == Vehicle.Route.Num() - 1 ? EdgeLength : MAX_int32, Vehicle.PedestrianTrack);
		if (Vehicle.bEmergencyResponding && HasPassingLanes(Edge))
			for (const auto& Merging : Initial)
			{
				if (Merging.bCompleted || Merging.VehicleType == ECityTripVehicleType::Walk) continue;
				const auto& OldLeg = Merging.Route[Merging.RouteLegIndex];
				const auto* Planned = Previous.FindByPredicate([&](const auto& P) { return P.Id == Merging.Id; });
				if (Planned && OldLeg.EdgeId == Leg.EdgeId && OldLeg.bReverse == Leg.bReverse &&
				    OldLeg.LaneIndex == Leg.LaneIndex && Planned->Route[Planned->RouteLegIndex].LaneIndex != OldLeg.LaneIndex &&
				    Merging.DistanceUnits > Vehicle.DistanceUnits)
					// The responder follows the old lane for this transition step;
					// it may overtake after the right-lane merge has visibly finished.
					LeaderLimit = FMath::Min(LeaderLimit, Merging.DistanceUnits - Merging.LengthUnits -
					                        SafeFollowingGapUnits - Vehicle.DistanceUnits);
			}
		for (const FCityTrafficVehicleState& Candidate : Previous)
		{
			if (Candidate.Id == Vehicle.Id || Candidate.bCompleted ||
			    (Vehicle.VehicleType == ECityTripVehicleType::Walk && Vehicle.PedestrianTrack != Candidate.PedestrianTrack) ||
			    (Candidate.VehicleType == ECityTripVehicleType::Walk) !=
			        (Vehicle.VehicleType == ECityTripVehicleType::Walk) ||
			    !Candidate.Route.IsValidIndex(Candidate.RouteLegIndex))
				continue;
			const FCityTrafficRouteLeg& CandidateLeg = Candidate.Route[Candidate.RouteLegIndex];
			if ((Vehicle.bEmergencyResponding && Candidate.bYielding &&
			     (!Candidate.bEmergencyResponding || CandidateLeg.bReverse != Leg.bReverse)) ||
			    (bClearCurb && Candidate.bEmergencyResponding &&
			     (DeferredYieldIds.Contains(Candidate.Id) ||
			      (!Candidate.bYielding && !YieldIds.Contains(Candidate.Id)))))
				continue;
			if (Vehicle.bEmergencyResponding && !HasPassingLanes(Edge) && CandidateLeg.EdgeId == Leg.EdgeId &&
			    CandidateLeg.bReverse != Leg.bReverse)
			{
				const int32 Separation =
				    GetEdgeLengthUnits(Edge) - Candidate.DistanceUnits - Vehicle.DistanceUnits;
				if (Separation >= 0)
					LeaderLimit = FMath::Min(
					    LeaderLimit, Separation - FMath::Max(Vehicle.LengthUnits, Candidate.LengthUnits) -
					                     SafeFollowingGapUnits);
			}
			if (CandidateLeg.EdgeId == Leg.EdgeId && CandidateLeg.bReverse == Leg.bReverse &&
			    CandidateLeg.LaneIndex == Leg.LaneIndex && Candidate.DistanceUnits > Vehicle.DistanceUnits)
			{
				LeaderLimit = FMath::Min(LeaderLimit, Candidate.DistanceUnits - Candidate.LengthUnits -
				                                          FollowingGap(Vehicle) - Vehicle.DistanceUnits);
			}
		}
		if (!Vehicle.bEmergencyResponding && !bClearCurb &&
		    !HasPassingLanes(Edge) &&
		    Vehicle.VehicleType != ECityTripVehicleType::Walk &&
		    Vehicle.RouteLegIndex + 1 < Vehicle.Route.Num() &&
		    EmergencyNodes.Contains(GetArrivalNode(Leg, Edge)))
			LeaderLimit = FMath::Min(LeaderLimit, EdgeLength - StopLineGapUnits - Vehicle.DistanceUnits);
		Movement = FMath::Clamp(Movement, 0, FMath::Max(0, LeaderLimit));

		if (Vehicle.DistanceUnits + Movement < EdgeLength)
		{
			Vehicle.DistanceUnits += Movement;
			Vehicle.SpeedUnitsPerStep = Movement;
			continue;
		}

		if (Vehicle.RouteLegIndex + 1 >= Vehicle.Route.Num())
		{
			Vehicle.DistanceUnits = EdgeLength;
			Vehicle.SpeedUnitsPerStep = Movement;
			Vehicle.bCompleted = true;
			Vehicle.bYielding = false;
			Vehicle.CompletedStep = FixedStepId;
			continue;
		}

		const FCityTrafficRouteLeg& NextLeg = Vehicle.Route[Vehicle.RouteLegIndex + 1];
		const FCityLaneConnector* Connector = FindConnector(Leg, NextLeg, Network);
		bool bCanEnter = Connector != nullptr;
		int32 OutgoingLeaderLimit = MAX_int32;
		OutgoingLeaderLimit = DepartureStopLimit(NextLeg, Vehicle.VehicleType == ECityTripVehicleType::Walk,
		                                        -1, Vehicle.bEmergencyResponding,
		                                        Vehicle.RouteLegIndex + 2 == Vehicle.Route.Num() && Vehicle.EndDistanceUnits >= 0
		                                            ? Vehicle.EndDistanceUnits : MAX_int32, Vehicle.PedestrianTrack);
		if (OutgoingLeaderLimit < 0)
			bCanEnter = false;
		const uint64 ReservationKey =
		    Connector ? (ConnectorReservationKey(*Connector, NextLeg.LaneIndex) |
		                 (Vehicle.VehicleType == ECityTripVehicleType::Walk ? (1ull << 63) | (uint64(Vehicle.PedestrianTrack) << 30) : 0))
		              : 0;
		if (!Connector || ReservedOutgoingLanes.Contains(ReservationKey))
			bCanEnter = false;
		for (const FCityTrafficVehicleState& Candidate : Previous)
		{
			if (!bCanEnter || Candidate.bCompleted ||
			    (Vehicle.VehicleType == ECityTripVehicleType::Walk && Vehicle.PedestrianTrack != Candidate.PedestrianTrack) ||
			    (Candidate.VehicleType == ECityTripVehicleType::Walk) !=
			        (Vehicle.VehicleType == ECityTripVehicleType::Walk) ||
			    !Candidate.Route.IsValidIndex(Candidate.RouteLegIndex))
				continue;
			const FCityTrafficRouteLeg& CandidateLeg = Candidate.Route[Candidate.RouteLegIndex];
			if ((Vehicle.bEmergencyResponding && Candidate.bYielding &&
			     (!Candidate.bEmergencyResponding || CandidateLeg.bReverse != NextLeg.bReverse)) ||
			    (bClearCurb && Candidate.bEmergencyResponding &&
			     (DeferredYieldIds.Contains(Candidate.Id) ||
			      (!Candidate.bYielding && !YieldIds.Contains(Candidate.Id)))))
				continue;
			if (CandidateLeg.EdgeId == NextLeg.EdgeId && CandidateLeg.bReverse == NextLeg.bReverse &&
			    CandidateLeg.LaneIndex == NextLeg.LaneIndex)
			{
				OutgoingLeaderLimit =
				    FMath::Min(OutgoingLeaderLimit,
				               Candidate.DistanceUnits - Candidate.LengthUnits - FollowingGap(Vehicle));
				if (OutgoingLeaderLimit < 0)
					bCanEnter = false;
			}
		}
		if (!bCanEnter)
		{
			Vehicle.DistanceUnits = FMath::Max(Vehicle.DistanceUnits, EdgeLength - StopLineGapUnits);
			Vehicle.SpeedUnitsPerStep = 0;
			continue;
		}

		ReservedOutgoingLanes.Add(ReservationKey);
		const int32 Remainder = FMath::Max(0, Vehicle.DistanceUnits + Movement - EdgeLength);
		++Vehicle.RouteLegIndex;
		Vehicle.DistanceUnits =
		    FMath::Min3(Remainder,
		                (Vehicle.RouteLegIndex == Vehicle.Route.Num() - 1 && Vehicle.EndDistanceUnits >= 0
		                     ? Vehicle.EndDistanceUnits
		                     : GetEdgeLengthUnits(Network.Edges[NextLeg.EdgeId])),
		                OutgoingLeaderLimit);
		Vehicle.SpeedUnitsPerStep =
		    FMath::Min(Movement, GetSpeedLimitUnitsPerStep(Network.Edges[NextLeg.EdgeId]));
	}
	Snapshot.Vehicles.Sort([](const auto& A, const auto& B) { return A.Id < B.Id; });
	Snapshot.FixedStepId = FixedStepId;
	// Catch gaps on every day, independently of the bounded route-search slice.
	AdmitDepartures(Network);
	RebuildLaneLoads(Network);
}

void FCityTrafficSimulation::AdmitDepartures(const FCityRoadNetworkSnapshot& Network)
{
	Snapshot.WaitingDepartures.Sort([](const auto& A, const auto& B) {
		if (A.bEmergencyResponding != B.bEmergencyResponding)
			return A.bEmergencyResponding;
		return A.DepartureRequestedStep == B.DepartureRequestedStep ? A.Id < B.Id
		                                                         : A.DepartureRequestedStep < B.DepartureRequestedStep;
	});
	for (int32 Index = 0; Index < Snapshot.WaitingDepartures.Num();)
	{
		const auto Departure = Snapshot.WaitingDepartures[Index];
		bool Admitted = false;
		for (int32 Attempt = 0; Attempt < (Departure.VehicleType == ECityTripVehicleType::Walk ? 2 : 1); ++Attempt)
		{
			auto Trial = Departure;
			if (Attempt) Trial.PedestrianTrack = 1 - Trial.PedestrianTrack;
			bool OlderSlot = false;
			for (int32 Older = 0; Older < Index; ++Older)
			{
				const auto& Other = Snapshot.WaitingDepartures[Older];
				const auto& A = Trial.Route[0]; const auto& B = Other.Route[0];
				const int32 D = Other.DistanceUnits - Trial.DistanceUnits;
				if (Trial.WalkPath.IsEmpty() && Other.WalkPath.IsEmpty() && A.EdgeId == B.EdgeId && A.bReverse == B.bReverse && A.LaneIndex == B.LaneIndex &&
				    (Trial.VehicleType == ECityTripVehicleType::Walk) == (Other.VehicleType == ECityTripVehicleType::Walk) &&
				    (Trial.VehicleType != ECityTripVehicleType::Walk || Trial.PedestrianTrack == Other.PedestrianTrack) &&
				    FMath::Abs(D) < (D >= 0 ? Other.LengthUnits : Trial.LengthUnits) + FollowingGap(Trial)) OlderSlot = true;
			}
			if (!OlderSlot && AddVehicle(Trial, Network)) { Admitted = true; break; }
		}
		if (Admitted) Snapshot.WaitingDepartures.RemoveAt(Index); else ++Index;
	}
}

int32 FCityTrafficSimulation::DepartureStopLimit(const FCityTrafficRouteLeg& Leg, const bool bPedestrian,
                                                const int32 Position, const bool bEmergency,
                                                const int32 DestinationDistance, const int32 PedestrianTrack) const
{
	if (bEmergency)
		return MAX_int32;
	// A blocked portal must not hold through-traffic forever when downstream
	// lanes are jammed. Alternate four-day merge/drain windows, staggered by
	// lane; admission still catches natural gaps on every day in both windows.
	const int64 Phase = (Snapshot.FixedStepId + Leg.EdgeId * 3 + (Leg.bReverse ? 2 : 0) +
	                     (bPedestrian ? 1 : 0)) % 8;
	if (Phase >= 4)
		return MAX_int32;
	int32 Limit = MAX_int32;
	for (const auto& Departure : Snapshot.WaitingDepartures)
	{
		const auto& Start = Departure.Route[0];
		if (Start.EdgeId != Leg.EdgeId || Start.bReverse != Leg.bReverse || Start.LaneIndex != Leg.LaneIndex ||
		    (Departure.VehicleType == ECityTripVehicleType::Walk) != bPedestrian ||
		    (bPedestrian && Departure.PedestrianTrack != PedestrianTrack))
			continue;
		// Trips finishing at/before this portal leave the lane on arrival. They
		// must be allowed to finish rather than queue behind their own destination.
		if (DestinationDistance <= Departure.DistanceUnits)
			break;
		const int32 Stop = Departure.DistanceUnits - Departure.LengthUnits - FollowingGap(Departure);
		// Traffic already inside the requested slot must clear it. Hold followers
		// behind it, making a safe gap rather than moving or deleting occupants.
		if (Position < 0)
			Limit = FMath::Min(Limit, Stop);
		else if (Position <= Stop)
			Limit = FMath::Min(Limit, Stop - Position);
		// Only the oldest request on this lane owns a stop line. Reserving all
		// portals together would trap through-traffic between off-road queues.
		break;
	}
	return Limit;
}

void FCityTrafficSimulation::RebuildLaneLoads(const FCityRoadNetworkSnapshot& Network)
{
	Snapshot.LaneLoads.Reset();
	TMap<uint64, int32> LoadIndices;
	TArray<int64> SpeedSums;
	for (const FCityTrafficVehicleState& Vehicle : Snapshot.Vehicles)
	{
		if (Vehicle.bCompleted || !Vehicle.Route.IsValidIndex(Vehicle.RouteLegIndex))
			continue;
		const FCityTrafficRouteLeg& Leg = Vehicle.Route[Vehicle.RouteLegIndex];
		if (!ValidateLeg(Leg, Network))
			continue;
		const uint64 Key = LaneKey(Leg.EdgeId, Leg.bReverse, Leg.LaneIndex) |
		                   (Vehicle.VehicleType == ECityTripVehicleType::Walk ? (1ull << 63) : 0);
		int32* ExistingIndex = LoadIndices.Find(Key);
		if (!ExistingIndex)
		{
			const FCityRoadEdge& Edge = Network.Edges[Leg.EdgeId];
			FCityTrafficLaneLoad& Load = Snapshot.LaneLoads.Emplace_GetRef();
			Load.EdgeId = Leg.EdgeId;
			Load.bPedestrian = Vehicle.VehicleType == ECityTripVehicleType::Walk;
			Load.bReverse = Leg.bReverse;
			Load.LaneIndex = Leg.LaneIndex;
			Load.Capacity =
			    FMath::Max(1, Edge.Capacity / FMath::Max(1, Edge.ForwardLanes + Edge.ReverseLanes));
			const int32 NewIndex = Snapshot.LaneLoads.Num() - 1;
			LoadIndices.Add(Key, NewIndex);
			SpeedSums.Add(0);
			ExistingIndex = LoadIndices.Find(Key);
		}
		FCityTrafficLaneLoad& Load = Snapshot.LaneLoads[*ExistingIndex];
		++Load.VehicleCount;
		SpeedSums[*ExistingIndex] += Vehicle.SpeedUnitsPerStep;
	}
	for (int32 Index = 0; Index < Snapshot.LaneLoads.Num(); ++Index)
	{
		FCityTrafficLaneLoad& Load = Snapshot.LaneLoads[Index];
		Load.AverageSpeedUnitsPerStep =
		    Load.VehicleCount > 0 ? static_cast<int32>(SpeedSums[Index] / Load.VehicleCount) : 0;
		const int32 SpeedLimit =
		    Load.bPedestrian ? 500 : GetSpeedLimitUnitsPerStep(Network.Edges[Load.EdgeId]);
		const int32 DensityPermille = FMath::Clamp((Load.VehicleCount * 1000) / Load.Capacity, 0, 1000);
		const int32 DelayPermille =
		    FMath::Clamp(1000 - (Load.AverageSpeedUnitsPerStep * 1000) / FMath::Max(1, SpeedLimit), 0, 1000);
		Load.CongestionPermille = FMath::Max(DensityPermille, DelayPermille);
	}
}

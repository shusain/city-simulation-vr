#include "Simulation/CityWasteSimulation.h"
#include "Simulation/CityBuildingDefinition.h"
#include "Simulation/CityDevelopmentSimulation.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Simulation/CityTrafficSimulation.h"
#include "Commands/CityPlacementModel.h"

int64 FCityWasteSnapshot::Piled() const
{
	int64 N = 0;
	for (const auto& S : Sites)
		N += S.Units;
	return N;
}
int64 FCityWasteSnapshot::InTrucks() const
{
	int64 N = 0;
	for (const auto& T : Trucks)
		N += T.Cargo;
	return N;
}
int64 FCityWasteSnapshot::InDepots() const
{
	int64 N = 0;
	for (const auto& D : Depots)
		N += D.Stored;
	return N;
}
int32 FCityWasteSnapshot::Pressure(uint64 Id, int32 StepsPerMonth) const
{
	const auto* S = Find(Id);
	return !S || IntroductionStep < 0 ||
	               LastStep - IntroductionStep <
	                   static_cast<int64>(FMath::Max(1, StepsPerMonth)) * FCityWasteTuning::GraceMonths
	           ? 0
	           : FMath::Clamp((S->Units * 100 / S->Capacity - 25) / 5, 0, 15);
}

FCityWasteFacilityPolicy FCityWasteSimulation::Policy(uint64 Id, const FCityPlacementModel& City,
                                                      const UCityBuildingCatalog* Catalog,
                                                      const FCityDevelopmentSnapshot& Development,
                                                      const FCityServiceSnapshot& Services, bool Unpaid)
{
	FCityWasteFacilityPolicy P;
	const auto* E = City.GetEntities().Find(Id);
	const auto* S = Development.Find(Id);
	const auto* D = E && Catalog ? Catalog->FindDefinition(E->DefinitionId) : nullptr;
	const auto* L = D ? D->FindDensityLevel(S ? S->DensityLevel : ECityDensityLevel::Low) : nullptr;
	P.bRecycling = E && E->DefinitionId == TEXT("Recycling");
	const bool Expanded = S && S->DensityLevel != ECityDensityLevel::Low;
	P.Range = L ? L->CivicRoadRange : 64;
	P.Storage = Expanded ? 4800 : 2400;
	P.ProcessingPerStep = Expanded ? 160 : 80;
	P.StaffPercent = E && IsFacility(E->DefinitionId) && !Unpaid && Development.IsOperational(Id) &&
	                         Services.GetFailure(Id) == ECityOperationFailure::None
	                     ? Services.PublicStaffPercent.FindRef(Id)
	                     : 0;
	P.Fleet = L && P.StaffPercent > 0 ? FMath::Max(1, L->CivicResponseVehicles * P.StaffPercent / 100) : 0;
	P.ProcessingPerStep = P.ProcessingPerStep * P.StaffPercent / 100;
	return P;
}

bool FCityWasteSimulation::ValidateState(const FCityWasteSnapshot& S, int64 MaximumStep, FString& Error)
{
	auto Fail = [&]()
	{
		Error = TEXT("Invalid waste state or conservation ledger");
		return false;
	};
	constexpr int64 Limit = MAX_int64 / 16;
	if (!S.Dimensions.IsValid() || S.LastStep < 0 || S.LastStep > MaximumStep || S.IntroductionStep < -1 ||
	    S.IntroductionStep > S.LastStep || !S.NextTruckId || (S.IntroductionStep < 0 && S.Generated != 0) ||
	    S.Generated < 0 || S.Generated > Limit || S.Collected < 0 || S.Collected > S.Generated || S.Disposed < 0 ||
	    S.Disposed > Limit || S.Recycled < 0 || S.Recycled > Limit || S.Overflow < 0 || S.Overflow > Limit ||
	    S.Removed < 0 || S.Removed > Limit || S.Sites.Num() > S.Dimensions.GetCellCount() ||
	    S.Depots.Num() > S.Dimensions.GetCellCount() || S.Trucks.Num() > S.Dimensions.GetCellCount() * 3)
		return Fail();
	TSet<uint64> Sites, Depots, Trucks, Trips, Reserved;
	for (const auto& V : S.Sites)
		if (!V.EntityId || Sites.Contains(V.EntityId) || V.Capacity < 1 || V.Capacity > 100000 || V.Units < 0 ||
		    V.Units > V.Capacity || V.GenerationRemainder < 0 || V.GenerationRemainder > 3650 ||
		    V.LastCollectionStep < 0 || V.LastCollectionStep > S.LastStep)
			return Fail();
		else
			Sites.Add(V.EntityId);
	for (const auto& V : S.Depots)
		if (!V.EntityId || Depots.Contains(V.EntityId) || Sites.Contains(V.EntityId) || V.Stored < 0 ||
		    V.Stored > 4800 || V.Processed < 0 || V.Processed > Limit || V.Recycled < 0 || V.Recycled > V.Processed)
			return Fail();
		else
			Depots.Add(V.EntityId);
	for (const auto& T : S.Trucks)
	{
		if (!T.Id || T.Id >= S.NextTruckId || Trucks.Contains(T.Id) || !Depots.Contains(T.DepotId) ||
		    !T.CurrentEntityId || T.Cargo < 0 || T.Cargo > FCityWasteTuning::TruckCapacity || T.LastWorkStep < 0 ||
		    T.LastWorkStep > S.LastStep || T.Stops.Num() > FCityWasteTuning::MaxStops ||
		    (T.TripId && Trips.Contains(T.TripId)))
			return Fail();
		Trucks.Add(T.Id);
		if (T.TripId)
			Trips.Add(T.TripId);
		for (uint64 Id : T.Stops)
		{
			if (!Sites.Contains(Id) || Reserved.Contains(Id))
				return Fail();
			Reserved.Add(Id);
		}
	}
	int64 Processed = 0, Recycled = 0;
	for (const auto& D : S.Depots)
	{
		if (D.Processed > S.Disposed + S.Recycled - Processed || D.Recycled > S.Recycled - Recycled)
			return Fail();
		Processed += D.Processed;
		Recycled += D.Recycled;
	}
	if (Processed > S.Disposed + S.Recycled || Recycled > S.Recycled ||
	    S.Generated != S.Piled() + S.InTrucks() + S.InDepots() + S.Disposed + S.Recycled + S.Overflow + S.Removed)
		return Fail();
	return true;
}
bool FCityWasteSimulation::RestoreState(const FCityWasteSnapshot& S, FString& Error)
{
	if (!ValidateState(S, S.LastStep, Error))
		return false;
	State = S;
	ReconciledRevision = MAX_uint64;
	return true;
}
bool FCityWasteSimulation::ValidateReferences(const FCityWasteSnapshot& S, const FCityPlacementModel& City,
                                              const FCityPopulationSnapshot& People, FString& Error)
{
	auto Fail = [&]()
	{
		Error = TEXT("Waste entity/truck/trip reference mismatch");
		return false;
	};
	if (S.Dimensions != City.GetGrid().GetDimensions())
		return Fail();
	for (const auto& V : S.Sites)
	{
		const auto* E = City.GetEntities().Find(V.EntityId);
		if (!E ||
		    (E->DefinitionId != TEXT("Home") && E->DefinitionId != TEXT("Shop") && E->DefinitionId != TEXT("Factory")))
			return Fail();
	}
	for (const auto& V : S.Depots)
	{
		const auto* E = City.GetEntities().Find(V.EntityId);
		if (!E || !IsFacility(E->DefinitionId))
			return Fail();
	}
	for (const auto& T : S.Trucks)
	{
		if (!City.GetEntities().Contains(T.CurrentEntityId))
			return Fail();
		if (T.TripId)
		{
			const auto* Trip = People.TransitTrips.FindByPredicate([&](const auto& V) { return V.Id == T.TripId; });
			if (!Trip || !Trip->IsWasteJourney() || Trip->StationEntityId != T.DepotId ||
			    (Trip->bCompleted ? Trip->DestinationEntityId : Trip->OriginEntityId) != T.CurrentEntityId ||
			    Trip->DestinationEntityId != (Trip->Purpose == ECityTripPurpose::WasteReturn ? T.DepotId
			                                  : T.Stops.IsEmpty()                            ? 0
			                                                                                 : T.Stops[0]))
				return Fail();
		}
	}
	for (const auto& Trip : People.TransitTrips)
		if (Trip.IsWasteJourney() && !S.Trucks.ContainsByPredicate([&](const auto& T) { return T.TripId == Trip.Id; }))
			return Fail();
	return true;
}

bool FCityWasteSimulation::AddTestPile(uint64 Id)
{
	auto* S = State.Sites.FindByPredicate([Id](const auto& V) { return V.EntityId == Id; });
	if (!S)
		return false;
	State.Generated += S->Capacity - S->Units;
	S->Units = S->Capacity;
	if (State.IntroductionStep < 0)
		State.IntroductionStep = State.LastStep;
	return true;
}

void FCityWasteSimulation::Update(int64 Step, int32 StepsPerMonth, const FCityPlacementModel& City,
                                  const UCityBuildingCatalog* Catalog, const FCityRoadNetworkSnapshot& Network,
                                  const FCityServiceSnapshot& Services, const FCityDevelopmentSnapshot& Development,
                                  FCityPopulationSnapshot& People, bool Unpaid, const FCityWasteTuning& Tuning)
{
	if (Step < State.LastStep)
		return;
	State.Dimensions = City.GetGrid().GetDimensions();
	ReconciledRevision = City.GetRevision();
	const bool Advance = Step > State.LastStep;
	const int64 Elapsed = Step - State.LastStep;
	State.LastStep = Step;
	StepsPerMonth = FMath::Clamp(StepsPerMonth, 1, 3650);
	auto RemoveTrip = [&](uint64 Id) { People.TransitTrips.RemoveAll([Id](const auto& T) { return T.Id == Id; }); };
	State.Sites.RemoveAll(
	    [&](const auto& S)
	    {
		    if (City.GetEntities().Contains(S.EntityId))
			    return false;
		    State.Removed += S.Units;
		    return true;
	    });
	State.Depots.RemoveAll(
	    [&](const auto& D)
	    {
		    if (City.GetEntities().Contains(D.EntityId))
			    return false;
		    State.Removed += D.Stored;
		    return true;
	    });
	State.Trucks.RemoveAll(
	    [&](const auto& T)
	    {
		    if (City.GetEntities().Contains(T.DepotId))
			    return false;
		    State.Removed += T.Cargo;
		    RemoveTrip(T.TripId);
		    return true;
	    });
	TArray<uint64> Ids;
	City.GetEntities().GetKeys(Ids);
	Ids.Sort();
	TMap<uint64, int32> Residents, Workers;
	for (const auto& C : People.Citizens)
	{
		++Residents.FindOrAdd(C.HomeEntityId);
		if (!C.IsDetained() && !C.bOnMedicalLeave)
			++Workers.FindOrAdd(C.WorkplaceEntityId);
	}
	bool HospitalReady = false;
	for (const auto& H : Services.HospitalCapacities)
		HospitalReady |= H.Value > 0;
	if (State.IntroductionStep < 0 && People.Citizens.Num() >= Tuning.PopulationThreshold && HospitalReady)
		State.IntroductionStep = Step;
	for (uint64 Id : Ids)
	{
		const auto& E = City.GetEntities().FindChecked(Id);
		if (IsFacility(E.DefinitionId))
		{
			if (!State.Depots.ContainsByPredicate([Id](const auto& D) { return D.EntityId == Id; }))
			{
				FCityWasteDepot D;
				D.EntityId = Id;
				State.Depots.Add(D);
			}
			continue;
		}
		if (E.DefinitionId != TEXT("Home") && E.DefinitionId != TEXT("Shop") && E.DefinitionId != TEXT("Factory"))
			continue;
		auto* S = State.Sites.FindByPredicate([Id](const auto& V) { return V.EntityId == Id; });
		if (!S)
		{
			FCityWasteSite V;
			V.EntityId = Id;
			State.Sites.Add(V);
			S = &State.Sites.Last();
		}
		const auto* D = Catalog ? Catalog->FindDefinition(E.DefinitionId) : nullptr;
		const auto* DS = Development.Find(Id);
		const auto* L = D ? D->FindDensityLevel(DS ? DS->DensityLevel : ECityDensityLevel::Low) : nullptr;
		const bool Home = E.DefinitionId == TEXT("Home"), Industry = E.DefinitionId == TEXT("Factory");
		const int32 Nominal = L      ? (Home ? L->ResidentsProvided : L->JobsProvided)
		                      : Home ? 10
		                      : D    ? D->JobsProvided
		                             : 10;
		S->Capacity = FMath::Max(S->Units, FMath::Clamp(FMath::Max(Home       ? 60
		                                                           : Industry ? 240
		                                                                      : 120,
		                                                           Nominal * (Home       ? 6
		                                                                      : Industry ? 24
		                                                                                 : 12)),
		                                                1, 100000));
		if (!Advance || State.IntroductionStep < 0)
			continue;
		const bool Operating = Services.GetFailure(Id) == ECityOperationFailure::None && Development.IsOperational(Id);
		const int32 Monthly = Home        ? Residents.FindRef(Id) * 2
		                      : Operating ? Workers.FindRef(Id) * (Industry ? 8 : 4)
		                                  : 0;
		const int64 Numerator = S->GenerationRemainder + static_cast<int64>(Monthly) * Elapsed;
		const int64 New = Numerator / StepsPerMonth;
		S->GenerationRemainder = Numerator % StepsPerMonth;
		const int32 Accepted = static_cast<int32>(FMath::Min<int64>(New, S->Capacity - S->Units));
		S->Units += Accepted;
		State.Generated += New;
		State.Overflow += New - Accepted;
	}
	State.Sites.Sort([](const auto& A, const auto& B) { return A.EntityId < B.EntityId; });
	State.Depots.Sort([](const auto& A, const auto& B) { return A.EntityId < B.EntityId; });
	auto Route = [&](uint64 From, uint64 To, TArray<FCityCellCoord>& Cells)
	{
		FCityCellCoord A, B;
		return FCityTrafficSimulation::FindPortal(City, Catalog, From, A) &&
		       FCityTrafficSimulation::FindPortal(City, Catalog, To, B) &&
		       Network.FindRoute(A, B, ECityRoadAccess::Freight, Cells);
	};
	TMap<FCityCellCoord, TArray<FCityCellCoord>> Links;
	for (const auto& Edge : Network.Edges)
		if (EnumHasAllFlags(Edge.Access, ECityRoadAccess::Freight))
			for (int32 I = 1; I < Edge.Cells.Num(); ++I)
			{
				Links.FindOrAdd(Edge.Cells[I - 1]).AddUnique(Edge.Cells[I]);
				Links.FindOrAdd(Edge.Cells[I]).AddUnique(Edge.Cells[I - 1]);
			}
	TMap<uint64, FCityCellCoord> Portals;
	for (uint64 Id : Ids)
	{
		FCityCellCoord Cell;
		if (FCityTrafficSimulation::FindPortal(City, Catalog, Id, Cell))
			Portals.Add(Id, Cell);
	}
	TSet<uint64> Reserved;
	for (auto& T : State.Trucks)
	{
		T.Stops.RemoveAll([&](uint64 Id) { return !State.Find(Id); });
		if (!City.GetEntities().Contains(T.CurrentEntityId))
		{
			State.Overflow += T.Cargo;
			T.Cargo = 0;
			RemoveTrip(T.TripId);
			T.TripId = 0;
			T.CurrentEntityId = T.DepotId;
			T.Stops.Reset();
		}
		for (uint64 Id : T.Stops)
			Reserved.Add(Id);
	}
	for (auto& Depot : State.Depots)
	{
		const auto P = Policy(Depot.EntityId, City, Catalog, Development, Services, Unpaid);
		if (Advance)
		{
			const int32 Processed =
			    static_cast<int32>(FMath::Min<int64>(Depot.Stored, static_cast<int64>(P.ProcessingPerStep) * Elapsed));
			const int64 Before = Depot.Processed;
			Depot.Stored -= Processed;
			Depot.Processed += Processed;
			const int64 Recycled = P.bRecycling ? (Depot.Processed * 3 / 5 - Before * 3 / 5) : 0;
			Depot.Recycled += Recycled;
			State.Recycled += Recycled;
			State.Disposed += Processed - Recycled;
		}
		int32 Existing = 0;
		for (const auto& T : State.Trucks)
			Existing += T.DepotId == Depot.EntityId;
		while (Existing < P.Fleet)
		{
			FCityWasteTruck T;
			T.Id = State.NextTruckId++;
			T.DepotId = T.CurrentEntityId = Depot.EntityId;
			State.Trucks.Add(T);
			++Existing;
		}
		TMap<FCityCellCoord, int32> Distance;
		if (const auto* Origin = Portals.Find(Depot.EntityId))
		{
			TArray<FCityCellCoord> Queue;
			Queue.Add(*Origin);
			Distance.Add(*Origin, 0);
			for (int32 Head = 0; Head < Queue.Num(); ++Head)
			{
				const auto Cell = Queue[Head];
				const int32 D = Distance.FindChecked(Cell);
				if (D >= P.Range)
					continue;
				if (const auto* Neighbors = Links.Find(Cell))
					for (const auto& N : *Neighbors)
						if (!Distance.Contains(N))
						{
							Distance.Add(N, D + 1);
							Queue.Add(N);
						}
			}
		}
		int32 Slot = 0;
		for (auto& T : State.Trucks)
		{
			if (T.DepotId != Depot.EntityId)
				continue;
			const bool Enabled = ++Slot <= P.Fleet;
			T.Stops.RemoveAll(
			    [&](uint64 Id)
			    {
				    const auto* Portal = Portals.Find(Id);
				    if (Portal && Distance.Contains(*Portal))
					    return false;
				    Reserved.Remove(Id);
				    return true;
			    });
			auto* Trip = People.TransitTrips.FindByPredicate([&](const auto& V) { return V.Id == T.TripId; });
			if (Trip && Trip->Purpose == ECityTripPurpose::WastePickup &&
			    (T.Stops.IsEmpty() || T.Stops[0] != Trip->DestinationEntityId))
			{
				RemoveTrip(T.TripId);
				T.TripId = 0;
				Trip = nullptr;
			}
			if (Trip && Trip->bCompleted)
			{
				T.CurrentEntityId = Trip->DestinationEntityId;
			}
			if (T.TripId && !Trip)
				T.TripId = 0;
			if (!Enabled)
			{
				RemoveTrip(T.TripId);
				T.TripId = 0;
				for (uint64 Id : T.Stops)
					Reserved.Remove(Id);
				T.Stops.Reset();
				continue;
			}
			TArray<FCityCellCoord> Cells;
			if (Trip && !Route(Trip->OriginEntityId, Trip->DestinationEntityId, Cells))
			{
				RemoveTrip(T.TripId);
				T.TripId = 0;
				Trip = nullptr;
				for (uint64 Id : T.Stops)
					Reserved.Remove(Id);
				T.Stops.Reset();
			}
			if (Trip && !Trip->bCompleted)
				continue;
			if (T.LastWorkStep < Step)
			{
				if (T.CurrentEntityId == T.DepotId && T.Cargo > 0)
				{
					const int32 N =
					    FMath::Min3(T.Cargo, FCityWasteTuning::UnloadPerStep, FMath::Max(0, P.Storage - Depot.Stored));
					T.Cargo -= N;
					Depot.Stored += N;
					T.LastWorkStep = Step;
				}
				else if (!T.Stops.IsEmpty() && T.CurrentEntityId == T.Stops[0])
				{
					auto* S =
					    State.Sites.FindByPredicate([&](const auto& V) { return V.EntityId == T.CurrentEntityId; });
					const int32 N = S ? FMath::Min3(S->Units, FCityWasteTuning::PickupPerStep,
					                                FCityWasteTuning::TruckCapacity - T.Cargo)
					                  : 0;
					if (S)
					{
						S->Units -= N;
						if (N)
							S->LastCollectionStep = Step;
					}
					T.Cargo += N;
					State.Collected += N;
					T.LastWorkStep = Step;
					if (!S || !S->Units || T.Cargo == FCityWasteTuning::TruckCapacity)
					{
						Reserved.Remove(T.Stops[0]);
						T.Stops.RemoveAt(0);
					}
				}
			}
			if (T.CurrentEntityId == T.DepotId && T.Cargo > 0)
				continue;
			if (T.Cargo == FCityWasteTuning::TruckCapacity)
			{
				for (uint64 Id : T.Stops)
					Reserved.Remove(Id);
				T.Stops.Reset();
			}
			if (T.CurrentEntityId == T.DepotId && T.Cargo == 0 && T.Stops.IsEmpty() && Depot.Stored < P.Storage)
			{
				struct FCandidate
				{
					uint64 Id;
					int32 Units, Distance;
				};
				TArray<FCandidate> Candidates;
				for (const auto& S : State.Sites)
					if (S.Units > 0 && !Reserved.Contains(S.EntityId))
					{
						const auto* Portal = Portals.Find(S.EntityId);
						const int32* D = Portal ? Distance.Find(*Portal) : nullptr;
						if (D)
							Candidates.Add({S.EntityId, S.Units, *D});
					}
				Candidates.Sort(
				    [](const auto& A, const auto& B)
				    {
					    return A.Units != B.Units         ? A.Units > B.Units
					           : A.Distance != B.Distance ? A.Distance < B.Distance
					                                      : A.Id < B.Id;
				    });
				uint64 From = T.DepotId;
				int32 Searches = 0;
				for (const auto& C : Candidates)
				{
					if (T.Stops.Num() >= FCityWasteTuning::MaxStops || ++Searches > 8)
						break;
					FCityCellCoord A, B;
					FCityTrafficVehicleState V;
					V.VehicleType = ECityTripVehicleType::GarbageTruck;
					if (Route(From, C.Id, Cells) && FCityTrafficSimulation::FindPortal(City, Catalog, From, A) &&
					    FCityTrafficSimulation::FindPortal(City, Catalog, C.Id, B) &&
					    (A == B || FCityTrafficSimulation::BuildRoute(Network, A, B, V)))
					{
						T.Stops.Add(C.Id);
						Reserved.Add(C.Id);
						From = C.Id;
					}
				}
			}
			// Service continues at the current stop until emptied/full; it is not a timer arrival.
			if (!T.Stops.IsEmpty() && T.CurrentEntityId == T.Stops[0] && T.Cargo < FCityWasteTuning::TruckCapacity)
				continue;
			if (Trip && Trip->bCompleted)
			{
				RemoveTrip(T.TripId);
				T.TripId = 0;
				Trip = nullptr;
			}
			const uint64 To = !T.Stops.IsEmpty() ? T.Stops[0] : T.DepotId;
			if (To == T.CurrentEntityId || !Route(T.CurrentEntityId, To, Cells))
				continue;
			auto& New = People.TransitTrips.Emplace_GetRef();
			New.Id = People.NextTransitTripId++;
			New.StationEntityId = T.DepotId;
			New.OriginEntityId = T.CurrentEntityId;
			New.DestinationEntityId = To;
			New.Purpose = To == T.DepotId ? ECityTripPurpose::WasteReturn : ECityTripPurpose::WastePickup;
			New.VehicleType = ECityTripVehicleType::GarbageTruck;
			New.StartStep = New.ArrivalStep = Step;
			T.TripId = New.Id;
		}
	}
}

void FCityPopulationSimulation::UpdateWasteService(FCityWasteSimulation& Waste, int64 Step, int32 StepsPerMonth,
                                                   const FCityPlacementModel& City, const UCityBuildingCatalog* Catalog,
                                                   const FCityRoadNetworkSnapshot& Network,
                                                   const FCityServiceSnapshot& Services,
                                                   const FCityDevelopmentSnapshot& Development, bool Unpaid)
{
	Waste.Update(Step, StepsPerMonth, City, Catalog, Network, Services, Development, Snapshot, Unpaid);
}

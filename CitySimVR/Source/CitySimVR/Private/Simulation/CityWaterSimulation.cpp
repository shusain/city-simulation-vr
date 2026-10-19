#include "Simulation/CityWaterSimulation.h"
#include "Performance/CityEvaluationTiming.h"
#include "Simulation/CityGrowthSimulation.h"
#include "Containers/Queue.h"

namespace
{
	void FrontageRoads(const FCityPlacementModel& City, const FCityWaterSnapshot& Water, const FCityCellCoord& Anchor,
	                   const FIntPoint& Size, TArray<FCityCellCoord>& Roads)
	{
		City.GetAdjacentRoadCells(Anchor, Size, true, Roads, Water.RulesVersion > 0);
	}
	void Flood(const FCityPlacementModel& City, const TArray<FCityCellCoord>& Starts, TSet<FCityCellCoord>& Out)
	{
		TMap<FCityCellCoord, int32> Distances;
		City.GetRoadReach(Starts, MAX_int32, true, Distances);
		Out.Reset();
		for (const auto& Pair : Distances)
			Out.Add(Pair.Key);
	}
	int32 SourcePressure(const FCityWaterSource& Source, const TArray<FCityCellCoord>& Roads)
	{
		int32 Pressure = 0;
		for (const auto& Road : Roads)
			if (const int32* Hops = Source.RoadDistances.Find(Road))
				Pressure = FMath::Max(Pressure, Source.SourcePressurePercent -
				                                    *Hops * FCityWaterEvaluator::PressureLossPerRoadHop);
		return Pressure;
	}
	int32 FlowLimit(const int32 Demand, const int32 Pressure)
	{
		if (Pressure < FCityWaterEvaluator::MinimumUsablePressurePercent)
			return 0;
		return static_cast<int32>(static_cast<int64>(Demand) *
		                          FMath::Min(Pressure, FCityWaterEvaluator::FullDeliveryPressurePercent) /
		                          FCityWaterEvaluator::FullDeliveryPressurePercent);
	}
	TArray<int32> SourceOrder(const FCityWaterSnapshot& Water, const TArray<FCityCellCoord>& Roads)
	{
		TArray<int32> Order;
		for (int32 I = 0; I < Water.Sources.Num(); ++I)
			if (Water.Sources[I].bOperational && Water.Sources[I].Capacity > Water.Sources[I].Used &&
			    SourcePressure(Water.Sources[I], Roads) >= FCityWaterEvaluator::MinimumUsablePressurePercent)
				Order.Add(I);
		Order.Sort(
		    [&](int32 A, int32 B)
		    {
			    // Head above full delivery cannot improve flow. Keep the stable source
			    // preference there so a stronger new source is not drained by consumers
			    // already fully served by an older source farther from the new branch.
			    const int32 PA = FMath::Min(SourcePressure(Water.Sources[A], Roads),
			                               FCityWaterEvaluator::FullDeliveryPressurePercent);
			    const int32 PB = FMath::Min(SourcePressure(Water.Sources[B], Roads),
			                               FCityWaterEvaluator::FullDeliveryPressurePercent);
			    return PA != PB ? PA > PB : Water.Sources[A].EntityId < Water.Sources[B].EntityId;
		    });
		return Order;
	}
	struct FSourcePressure
	{
		int32 Index;
		int32 Pressure;
	};

	// Rebuilt for each request so preceding consumers' source budgets stay live.
	struct FPreparedWaterQuery
	{
		TArray<FCityCellCoord> Roads;
		TArray<FSourcePressure> Order;
		int32 Pressure = 0;
		FPreparedWaterQuery(const FCityPlacementModel& City, const FCityWaterSnapshot& Water,
		                    const FCityCellCoord& Anchor, const FIntPoint& Size, const bool bOrder = true)
		{
			FrontageRoads(City, Water, Anchor, Size, Roads);
			for (int32 Index = 0; Index < Water.Sources.Num(); ++Index)
			{
				const auto& Source = Water.Sources[Index];
				if (!Source.bOperational || Source.Capacity <= Source.Used)
					continue;
				const int32 Head = SourcePressure(Source, Roads);
				const int32 AvailableHead =
				    Water.RulesVersion == 0 && Source.RoadDistances.Num() > 0
				        ? (Roads.ContainsByPredicate([&](const auto& Road)
				                                     { return Source.RoadDistances.Contains(Road); })
				               ? 100
				               : 0)
				        : Head;
				Pressure = FMath::Max(Pressure, AvailableHead);
				if (bOrder && Head >= FCityWaterEvaluator::MinimumUsablePressurePercent)
					Order.Add({Index, Head});
			}
			Order.Sort(
			    [&](const FSourcePressure& A, const FSourcePressure& B)
			    {
				    const int32 PA = FMath::Min(A.Pressure, FCityWaterEvaluator::FullDeliveryPressurePercent);
				    const int32 PB = FMath::Min(B.Pressure, FCityWaterEvaluator::FullDeliveryPressurePercent);
				    return PA != PB ? PA > PB
				                    : Water.Sources[A.Index].EntityId < Water.Sources[B.Index].EntityId;
			    });
		}
		int32 Available(const FCityWaterSnapshot& Water, const int32 Demand) const
		{
			const int32 Target = FlowLimit(FMath::Max(0, Demand), Pressure);
			int32 Delivered = 0;
			for (const auto& Entry : Order)
			{
				const auto& Source = Water.Sources[Entry.Index];
				Delivered += FMath::Min3(Target - Delivered, FMath::Max(0, Source.Capacity - Source.Used),
				                         FlowLimit(Demand, Entry.Pressure));
				if (Delivered == Target)
					break;
			}
			return Delivered;
		}
	};
	void CountQuery(const FCityWaterSnapshot& Water)
	{
		if (Water.EvaluationTiming)
			++(Water.bPrepareQueries ? Water.EvaluationTiming->PreparedWaterQueries
			                         : Water.EvaluationTiming->ReferenceWaterQueries);
	}
	ECityWaterFailure DeliveryCause(const FCityWaterSnapshot& Water, const TArray<FCityCellCoord>& Roads,
	                                const int32 Requested, const int32 Target, const int32 Delivered)
	{
		if (Delivered >= Requested)
			return ECityWaterFailure::None;
		const bool Active =
		    Roads.ContainsByPredicate([&](const auto& R) { return Water.ActiveNetworkRoads.Contains(R); });
		const bool Potential =
		    Roads.ContainsByPredicate([&](const auto& R) { return Water.PotentialNetworkRoads.Contains(R); });
		// Physical pipe pressure survives budget exhaustion: zero remaining allocation
		// beside a healthy source is a capacity failure, not an excessive path distance.
		const bool UsablePipe = Roads.ContainsByPredicate(
		    [&](const auto& R)
		    { return Water.RoadPressurePercent.FindRef(R) >= FCityWaterEvaluator::MinimumUsablePressurePercent; });
		return !Active ? (Potential ? ECityWaterFailure::SourceOutage : ECityWaterFailure::Disconnected)
		       : (Delivered < Target || (Target == 0 && UsablePipe)) ? ECityWaterFailure::SourceCapacity
		                                                             : ECityWaterFailure::LowPressure;
	}
} // namespace

void FCityWaterEvaluator::BuildNetwork(const FCityPlacementModel& City, FCityWaterSnapshot& Water,
                                       const int32 LegacyHopBudget)
{
	Water.RoadPressurePercent.Reset();
	Water.ActiveNetworkRoads.Reset();
	Water.PotentialNetworkRoads.Reset();
	TArray<FCityCellCoord> AllStarts, ActiveStarts;
	for (auto& Source : Water.Sources)
	{
		Source.Used = 0;
		Source.RoadDistances.Reset();
		const auto* Entity = City.GetEntities().Find(Source.EntityId);
		if (!Entity)
			continue;
		TArray<FCityCellCoord> Starts;
		FrontageRoads(City, Water, Entity->Anchor, Entity->FootprintSize, Starts);
		AllStarts.Append(Starts);
		if (!Source.bOperational || Source.Capacity <= 0)
			continue;
		ActiveStarts.Append(Starts);
		const int32 Budget =
		    Water.RulesVersion == 0 ? LegacyHopBudget : Source.SourcePressurePercent / PressureLossPerRoadHop;
		City.GetRoadReach(Starts, Budget, true, Source.RoadDistances);
		for (const auto& Pair : Source.RoadDistances)
		{
			const int32 Pressure =
			    Water.RulesVersion == 0
			        ? 100
			        : FMath::Max(0, Source.SourcePressurePercent - Pair.Value * PressureLossPerRoadHop);
			Water.RoadPressurePercent.FindOrAdd(Pair.Key) =
			    FMath::Max(Water.RoadPressurePercent.FindRef(Pair.Key), Pressure);
		}
	}
	// Two union floods distinguish a distant low-pressure branch from an inactive source.
	// At most 8192 road addresses are visited in each flood, including raised decks.
	Flood(City, AllStarts, Water.PotentialNetworkRoads);
	Flood(City, ActiveStarts, Water.ActiveNetworkRoads);
}

int32 FCityWaterEvaluator::GetPressure(const FCityPlacementModel& City, const FCityWaterSnapshot& Water,
                                       const FCityCellCoord& Anchor, const FIntPoint& Size)
{
	CountQuery(Water);
	if (Water.bPrepareQueries)
		return FPreparedWaterQuery(City, Water, Anchor, Size, false).Pressure;
	TArray<FCityCellCoord> Roads;
	FrontageRoads(City, Water, Anchor, Size, Roads);
	int32 Pressure = 0;
	for (const auto& Source : Water.Sources)
		if (Source.bOperational && Source.Capacity > Source.Used)
			Pressure = FMath::Max(
			    Pressure,
			    Water.RulesVersion == 0 && Source.RoadDistances.Num() > 0
			        ? (Roads.ContainsByPredicate([&](const auto& R) { return Source.RoadDistances.Contains(R); }) ? 100
			                                                                                                      : 0)
			        : SourcePressure(Source, Roads));
	return Pressure;
}

int32 FCityWaterEvaluator::GetAvailableAt(const FCityPlacementModel& City, const FCityWaterSnapshot& Water,
                                          const FCityCellCoord& Anchor, const FIntPoint& Size, const int32 Demand)
{
	CountQuery(Water);
	if (Water.bPrepareQueries)
		return FPreparedWaterQuery(City, Water, Anchor, Size).Available(Water, Demand);
	TArray<FCityCellCoord> Roads;
	FrontageRoads(City, Water, Anchor, Size, Roads);
	const int32 Target = FlowLimit(FMath::Max(0, Demand), GetPressure(City, Water, Anchor, Size));
	int32 Available = 0;
	for (const int32 Index : SourceOrder(Water, Roads))
	{
		const auto& Source = Water.Sources[Index];
		Available += FMath::Min3(Target - Available, FMath::Max(0, Source.Capacity - Source.Used),
		                         FlowLimit(Demand, SourcePressure(Source, Roads)));
		if (Available == Target)
			break;
	}
	return Available;
}

int32 FCityWaterEvaluator::Allocate(const FCityPlacementModel& City, FCityWaterSnapshot& Water, const uint64 EntityId,
                                    const FCityCellCoord& Anchor, const FIntPoint& Size, const int32 Demand)
{
	const int32 Requested = FMath::Max(0, Demand);
	CountQuery(Water);
	int32 Target = 0;
	TArray<FCityCellCoord> Roads;
	int32 Delivered = 0;
	if (Water.bPrepareQueries)
	{
		FPreparedWaterQuery Query(City, Water, Anchor, Size);
		Roads = MoveTemp(Query.Roads);
		Target = FlowLimit(Requested, Query.Pressure);
		for (const auto& Entry : Query.Order)
		{
			auto& Source = Water.Sources[Entry.Index];
			const int32 Taken = FMath::Min3(Target - Delivered, FMath::Max(0, Source.Capacity - Source.Used),
			                                FlowLimit(Requested, Entry.Pressure));
			Source.Used += Taken;
			Delivered += Taken;
			if (Delivered == Target)
				break;
		}
	}
	else
	{
		Target = FlowLimit(Requested, GetPressure(City, Water, Anchor, Size));
		FrontageRoads(City, Water, Anchor, Size, Roads);
		for (const int32 Index : SourceOrder(Water, Roads))
		{
			auto& Source = Water.Sources[Index];
			const int32 Taken = FMath::Min3(Target - Delivered, FMath::Max(0, Source.Capacity - Source.Used),
			                                FlowLimit(Requested, SourcePressure(Source, Roads)));
			Source.Used += Taken;
			Delivered += Taken;
			if (Delivered == Target)
				break;
		}
	}
	const ECityWaterFailure Cause = DeliveryCause(Water, Roads, Requested, Target, Delivered);
	// Display physical pipe head independently of the remaining allocation budget.
	// A source at its capacity limit still pressurizes its connected pipes.
	int32 PhysicalPressure = 0;
	for (const auto& Road : Roads)
		PhysicalPressure = FMath::Max(PhysicalPressure, Water.RoadPressurePercent.FindRef(Road));
	Water.EntityDemand.Add(EntityId, Requested);
	Water.EntityDelivered.Add(EntityId, Delivered);
	Water.EntityPressurePercent.Add(EntityId, PhysicalPressure);
	Water.EntityCauses.Add(EntityId, Cause);
	return Delivered;
}

ECityWaterFailure FCityWaterEvaluator::EvaluateAt(const FCityPlacementModel& City, const FCityWaterSnapshot& Water,
                                                  const FCityCellCoord& Anchor, const FIntPoint& Size,
                                                  const int32 Demand)
{
	CountQuery(Water);
	if (Water.bPrepareQueries)
	{
		const FPreparedWaterQuery Query(City, Water, Anchor, Size);
		const int32 Requested = FMath::Max(0, Demand);
		return DeliveryCause(Water, Query.Roads, Requested, FlowLimit(Requested, Query.Pressure),
		                     Query.Available(Water, Requested));
	}
	const int32 Requested = FMath::Max(0, Demand);
	const int32 Target = FlowLimit(Requested, GetPressure(City, Water, Anchor, Size));
	TArray<FCityCellCoord> Roads;
	FrontageRoads(City, Water, Anchor, Size, Roads);
	return DeliveryCause(Water, Roads, Requested, Target, GetAvailableAt(City, Water, Anchor, Size, Requested));
}

FString FCityWaterEvaluator::FailureToString(const ECityWaterFailure Cause)
{
	switch (Cause)
	{
	case ECityWaterFailure::None:
		return TEXT("Water supplied");
	case ECityWaterFailure::Disconnected:
		return TEXT("No connected source/pipe path");
	case ECityWaterFailure::LowPressure:
		return TEXT("Low water pressure: source too far by road");
	case ECityWaterFailure::SourceCapacity:
		return TEXT("Connected source supply exhausted");
	case ECityWaterFailure::SourceOutage:
		return TEXT("Connected water source offline");
	default:
		return TEXT("Water unavailable");
	}
}

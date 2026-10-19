#include "Commands/CityPlacementModel.h"
#include "Containers/Queue.h"

void FCityPlacementModel::GetAdjacentRoadCells(const FCityCellCoord& Anchor, const FIntPoint& Size,
                                               const bool bConnectedOnly, TArray<FCityCellCoord>& Out,
                                               const bool bBuildingFrontageOnly) const
{
	const FCityFrontageKey Key{Anchor, Size, bConnectedOnly, bBuildingFrontageOnly};
	if (SpatialCache.bEnabled)
		if (const auto* Cached = SpatialCache.Frontages.Find(Key))
		{
			Out = *Cached;
			++SpatialCache.Stats.FrontageHits;
			return;
		}
	++SpatialCache.Stats.FrontageBuilds;
	TSet<FCityCellCoord> Unique;
	static const FIntPoint Offsets[] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
	for (int32 Y = Anchor.Y; Y < Anchor.Y + Size.Y; ++Y)
		for (int32 X = Anchor.X; X < Anchor.X + Size.X; ++X)
			for (const auto& Offset : Offsets)
			{
				const FCityCellCoord Cell(X + Offset.X, Y + Offset.Y);
				if (IsRoad(Cell) && (!bConnectedOnly || IsRoadConnected(Cell)) &&
				    (!bBuildingFrontageOnly || SupportsBuildingFrontage(Cell)))
					Unique.Add(Cell);
			}
	Out = Unique.Array();
	Out.Sort([](const auto& A, const auto& B) { return A.Y != B.Y ? A.Y < B.Y : A.X < B.X; });
	if (SpatialCache.bEnabled)
	{
		// Moving previews cannot grow this cache without bound.
		if (SpatialCache.Frontages.Num() >= Grid.GetDimensions().GetCellCount() * 4)
			SpatialCache.Frontages.Reset();
		SpatialCache.Frontages.Add(Key, Out);
	}
}

void FCityPlacementModel::GetRoadReach(const TArray<FCityCellCoord>& Starts, const int32 HopBudget,
                                       const bool bConnectedOnly, TMap<FCityCellCoord, int32>& Out) const
{
	FCityRoadReachKey Key;
	Key.HopBudget = HopBudget;
	Key.bConnectedOnly = bConnectedOnly;
	TSet<FCityCellCoord> Unique(Starts);
	Key.Starts = Unique.Array();
	Key.Starts.Sort(
	    [](const auto& A, const auto& B)
	    { return A.RoadLevel != B.RoadLevel ? A.RoadLevel < B.RoadLevel
		         : A.Y != B.Y               ? A.Y < B.Y
		                                    : A.X < B.X; });
	if (SpatialCache.bEnabled)
		if (const auto* Cached = SpatialCache.Reaches.Find(Key))
		{
			Out = *Cached;
			++SpatialCache.Stats.ReachHits;
			return;
		}
	++SpatialCache.Stats.ReachBuilds;
	Out.Reset();
	TQueue<FCityCellCoord> Queue;
	if (HopBudget >= 0)
		for (const auto& Start : Key.Starts)
		{
			Out.Add(Start, 0);
			Queue.Enqueue(Start);
		}
	FCityCellCoord Current;
	while (Queue.Dequeue(Current))
	{
		const int32 Distance = Out.FindChecked(Current);
		if (Distance >= HopBudget)
			continue;
		TArray<FCityCellCoord> Next;
		GetConnectedRoadNeighbors(Current, Next);
		for (const auto& Cell : Next)
			if (IsRoad(Cell) && (!bConnectedOnly || IsRoadConnected(Cell)) && !Out.Contains(Cell))
			{
				Out.Add(Cell, Distance + 1);
				Queue.Enqueue(Cell);
			}
	}
	if (SpatialCache.bEnabled)
	{
		// Bound both large floods and the number of empty/negative queries.
		if (SpatialCache.Reaches.Num() >= Grid.GetDimensions().GetCellCount() / 8 ||
		    SpatialCache.CachedDistances + Out.Num() > Grid.GetDimensions().GetCellCount() * 32)
		{
			SpatialCache.Reaches.Reset();
			SpatialCache.CachedDistances = 0;
		}
		SpatialCache.Reaches.Add(MoveTemp(Key), Out);
		SpatialCache.CachedDistances += Out.Num();
	}
}

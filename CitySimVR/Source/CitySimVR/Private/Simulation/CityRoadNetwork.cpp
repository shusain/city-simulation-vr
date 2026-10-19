#include "Simulation/CityRoadNetwork.h"

#include "Algo/Reverse.h"
#include "Commands/CityPlacementModel.h"

struct FCityRoadRoutingData
{
	struct FLink
	{
		FCityCellCoord Cell;
		int32 Cost;
		ECityRoadAccess Access;
		int32 EdgeIndex, CellIndex;
		bool bReverse;
	};
	TMap<FCityCellCoord, TArray<FLink>> Links;

	explicit FCityRoadRoutingData(const TArray<FCityRoadEdge>& Edges,
	                              ECityRoadAccess RequiredAccess = ECityRoadAccess::None)
	{
		for (int32 EdgeIndex = 0; EdgeIndex < Edges.Num(); ++EdgeIndex)
		{
			const FCityRoadEdge& Edge = Edges[EdgeIndex];
			if (!EnumHasAllFlags(Edge.Access, RequiredAccess))
				continue;
			const int32 Cost = FMath::Max(1, 1000 / FMath::Max(1, Edge.SpeedUnitsPerStep));
			for (int32 Index = 1; Index < Edge.Cells.Num(); ++Index)
			{
				Links.FindOrAdd(Edge.Cells[Index - 1])
				    .Add({Edge.Cells[Index], Cost, Edge.Access, EdgeIndex, Index - 1, false});
				Links.FindOrAdd(Edge.Cells[Index])
				    .Add({Edge.Cells[Index - 1], Cost, Edge.Access, EdgeIndex, Edge.Cells.Num() - 1 - Index, true});
			}
		}
	}
};

namespace
{

	int32 CellKey(const FCityCellCoord& Cell, const FCityGridDimensions& Dimensions)
	{
		return (Cell.Y * Dimensions.GetWidth() + Cell.X) * 2 + Cell.RoadLevel;
	}

	uint64 LinkKey(const FCityCellCoord& A, const FCityCellCoord& B, const FCityGridDimensions& Dimensions)
	{
		const uint32 First = static_cast<uint32>(FMath::Min(CellKey(A, Dimensions), CellKey(B, Dimensions)));
		const uint32 Second = static_cast<uint32>(FMath::Max(CellKey(A, Dimensions), CellKey(B, Dimensions)));
		return (static_cast<uint64>(First) << 32) | Second;
	}

	void RoadNeighbors(const FCityPlacementModel& City, const FCityCellCoord& Cell, TArray<FCityCellCoord>& Out)
	{
		City.GetConnectedRoadNeighbors(Cell, Out);
	}

	bool IsNodeCell(const FCityPlacementModel& City, const FCityCellCoord& Cell)
	{
		TArray<FCityCellCoord> Neighbors;
		RoadNeighbors(City, Cell, Neighbors);
		if (Neighbors.Num() != 2)
			return true;
		if (City.GetRoadDefinitionId(Neighbors[0]) != City.GetRoadDefinitionId(Cell) ||
		    City.GetRoadDefinitionId(Neighbors[1]) != City.GetRoadDefinitionId(Cell))
			return true;
		return Neighbors[0].X != Neighbors[1].X && Neighbors[0].Y != Neighbors[1].Y;
	}

	FCityRoadDefinition ResolveDefinition(const FName Id, const FCityRoadDefinition& Fallback)
	{
		if (Id == TEXT("DirtRoad"))
			return FCityRoadDefinition::Dirt();
		if (Id == TEXT("Avenue"))
			return FCityRoadDefinition::Avenue();
		if (Id == TEXT("Highway"))
			return FCityRoadDefinition::Highway();
		if (Id == TEXT("LocalRoad"))
			return FCityRoadDefinition::Local();
		return Fallback;
	}

	FIntPoint DirectionAtNode(const FCityRoadEdge& Edge, const int32 NodeId, const bool bOutgoing)
	{
		const bool bAtStart = Edge.StartNodeId == NodeId;
		const FCityCellCoord NodeCell = bAtStart ? Edge.Cells[0] : Edge.Cells.Last();
		const FCityCellCoord Adjacent = bAtStart ? Edge.Cells[1] : Edge.Cells[Edge.Cells.Num() - 2];
		const FIntPoint Away(Adjacent.X - NodeCell.X, Adjacent.Y - NodeCell.Y);
		return bOutgoing ? Away : FIntPoint(-Away.X, -Away.Y);
	}

	ECityRoadTurn ClassifyTurn(const FIntPoint& Incoming, const FIntPoint& Outgoing)
	{
		const int32 Cross = Incoming.X * Outgoing.Y - Incoming.Y * Outgoing.X;
		if (Cross > 0)
			return ECityRoadTurn::Left;
		if (Cross < 0)
			return ECityRoadTurn::Right;
		return ECityRoadTurn::Straight;
	}
} // namespace

bool FCityRoadDefinition::IsValid() const
{
	return !Id.IsNone() && ForwardLanes >= 0 && ReverseLanes >= 0 && ForwardLanes + ReverseLanes > 0 &&
	       SpeedUnitsPerStep > 0 && CapacityPerLane > 0 && PlacementCostPerCell >= 0 && MonthlyUpkeepPerCell >= 0;
}

FCityRoadDefinition FCityRoadDefinition::Dirt()
{
	FCityRoadDefinition Value;
	Value.Id = TEXT("DirtRoad");
	Value.RoadClass = ECityRoadClass::Dirt;
	Value.SpeedUnitsPerStep = 2;
	Value.CapacityPerLane = 8;
	Value.PlacementCostPerCell = 4;
	Value.MonthlyUpkeepPerCell = 0;
	return Value;
}

FCityRoadDefinition FCityRoadDefinition::Local()
{
	return FCityRoadDefinition();
}

FCityRoadDefinition FCityRoadDefinition::Avenue()
{
	FCityRoadDefinition Value;
	Value.Id = TEXT("Avenue");
	Value.RoadClass = ECityRoadClass::Avenue;
	Value.ForwardLanes = 2;
	Value.ReverseLanes = 2;
	Value.SpeedUnitsPerStep = 6;
	Value.CapacityPerLane = 30;
	Value.PlacementCostPerCell = 30;
	Value.MonthlyUpkeepPerCell = 4;
	return Value;
}

FCityRoadDefinition FCityRoadDefinition::Highway()
{
	FCityRoadDefinition Value = Avenue();
	Value.Id = TEXT("Highway");
	Value.RoadClass = ECityRoadClass::Highway;
	Value.SpeedUnitsPerStep = 10;
	Value.CapacityPerLane = 50;
	Value.PlacementCostPerCell = 60;
	Value.MonthlyUpkeepPerCell = 8;
	Value.Access = ECityRoadAccess::Car | ECityRoadAccess::Freight;
	return Value;
}

bool FCityRoadNetworkSnapshot::IsValid(FString* OutError) const
{
	if (!Dimensions.IsValid())
	{
		if (OutError)
			*OutError = TEXT("Road network dimensions are invalid");
		return false;
	}
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		if (Nodes[Index].Id != Index || !Dimensions.IsValidCell(Nodes[Index].Cell))
		{
			if (OutError)
				*OutError = TEXT("Road node IDs or coordinates are invalid");
			return false;
		}
	}
	for (int32 Index = 0; Index < Edges.Num(); ++Index)
	{
		const FCityRoadEdge& Edge = Edges[Index];
		if (Edge.Id != Index || !Nodes.IsValidIndex(Edge.StartNodeId) || !Nodes.IsValidIndex(Edge.EndNodeId) ||
		    Edge.Cells.Num() < 2 ||
		    Edge.Cells.ContainsByPredicate([&](const auto& Cell) { return !Dimensions.IsValidCell(Cell); }))
		{
			if (OutError)
				*OutError = TEXT("Road edge has invalid endpoints or cells");
			return false;
		}
	}
	for (int32 Index = 0; Index < Connectors.Num(); ++Index)
	{
		const FCityLaneConnector& Connector = Connectors[Index];
		if (Connector.Id != Index || !Nodes.IsValidIndex(Connector.NodeId) ||
		    !Edges.IsValidIndex(Connector.IncomingEdgeId) || !Edges.IsValidIndex(Connector.OutgoingEdgeId) ||
		    Connector.IncomingEdgeId == Connector.OutgoingEdgeId)
		{
			if (OutError)
				*OutError = TEXT("Road connector has invalid endpoints");
			return false;
		}
	}
	return true;
}

bool FCityRoadNetworkSnapshot::FindRoute(const FCityCellCoord& Start, const FCityCellCoord& End,
                                         const ECityRoadAccess RequiredAccess, TArray<FCityCellCoord>& OutCells) const
{
	OutCells.Reset();
	if (!Dimensions.IsValidCell(Start) || !Dimensions.IsValidCell(End))
		return false;
	const double PreparationStart = RoutingTiming ? FPlatformTime::Seconds() : 0;
	TUniquePtr<FCityRoadRoutingData> Reference;
	const FCityRoadRoutingData* Data = bUsePreparedRouting ? RoutingData.Get() : nullptr;
	if (Data)
	{
		if (RoutingTiming)
			++RoutingTiming->PreparedSearches;
	}
	else
	{
		Reference = MakeUnique<FCityRoadRoutingData>(Edges, RequiredAccess);
		Data = Reference.Get();
		if (RoutingTiming)
			++RoutingTiming->ReferenceBuilds;
	}
	const auto& Links = Data->Links;
	if (RoutingTiming)
		RoutingTiming->Preparation.Add((FPlatformTime::Seconds() - PreparationStart) * 1000);
	FCityFrameTimingScope SearchScope(RoutingTiming ? &RoutingTiming->Search : nullptr);
	auto HasAccess = [&](const FCityCellCoord& Cell)
	{
		const auto* Neighbors = Links.Find(Cell);
		return Neighbors && Neighbors->ContainsByPredicate([&](const auto& Link)
		                                                   { return EnumHasAllFlags(Link.Access, RequiredAccess); });
	};
	if (Start == End && HasAccess(Start))
	{
		OutCells.Add(Start);
		return true;
	}
	if (!HasAccess(Start) || !HasAccess(End))
		return false;

	TMap<FCityCellCoord, int32> Distances;
	TMap<FCityCellCoord, FCityCellCoord> Previous;
	TSet<FCityCellCoord> Visited;
	struct FFrontier
	{
		FCityCellCoord Cell;
		int32 Distance;
	};
	const auto Earlier = [&](const FFrontier& A, const FFrontier& B)
	{
		return A.Distance != B.Distance ? A.Distance < B.Distance
		                                : CellKey(A.Cell, Dimensions) < CellKey(B.Cell, Dimensions);
	};
	TArray<FFrontier> Frontier;
	Distances.Add(Start, 0);
	Frontier.HeapPush({Start, 0}, Earlier);
	bool bReached = false;
	while (!Frontier.IsEmpty())
	{
		FFrontier Next;
		Frontier.HeapPop(Next, Earlier, EAllowShrinking::No);
		const FCityCellCoord Current = Next.Cell;
		const int32 BestDistance = Next.Distance;
		if (Visited.Contains(Current) || BestDistance != Distances.FindChecked(Current))
			continue;
		if (Current == End)
		{
			bReached = true;
			break;
		}
		Visited.Add(Current);
		for (const auto& Link : Links.FindChecked(Current))
		{
			if (!EnumHasAllFlags(Link.Access, RequiredAccess))
				continue;
			const int32 Candidate = BestDistance + Link.Cost;
			const int32* Existing = Distances.Find(Link.Cell);
			if (!Existing || Candidate < *Existing ||
			    (Candidate == *Existing &&
			     CellKey(Current, Dimensions) < CellKey(Previous.FindRef(Link.Cell), Dimensions)))
			{
				Distances.Add(Link.Cell, Candidate);
				Previous.Add(Link.Cell, Current);
				Frontier.HeapPush({Link.Cell, Candidate}, Earlier);
			}
		}
	}
	if (!bReached)
		return false;
	for (FCityCellCoord Cell = End;; Cell = Previous.FindChecked(Cell))
	{
		OutCells.Add(Cell);
		if (Cell == Start)
			break;
	}
	Algo::Reverse(OutCells);
	return true;
}

void FCityRoadNetworkSnapshot::PrepareRouting()
{
	RoutingData = MakeShared<FCityRoadRoutingData>(Edges);
}

void FCityRoadNetworkSnapshot::FindWalkingDistances(const FCityCellCoord& Start,
                                                    TMap<FCityCellCoord, int32>& OutDistances) const
{
	OutDistances.Reset();
	if (!Dimensions.IsValidCell(Start)) return;
	TUniquePtr<FCityRoadRoutingData> Reference;
	const auto* Data = bUsePreparedRouting ? RoutingData.Get() : nullptr;
	if (!Data)
	{
		Reference = MakeUnique<FCityRoadRoutingData>(Edges, ECityRoadAccess::Pedestrian);
		Data = Reference.Get();
	}
	const auto* First = Data->Links.Find(Start);
	if (!First || !First->ContainsByPredicate([](const auto& L) {
		return EnumHasAllFlags(L.Access, ECityRoadAccess::Pedestrian); })) return;
	TArray<FCityCellCoord> Queue;
	Queue.Add(Start); OutDistances.Add(Start, 0);
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const auto Cell = Queue[Head];
		const int32 NextDistance = OutDistances.FindChecked(Cell) + 1;
		const auto* Links = Data->Links.Find(Cell);
		if (!Links) continue;
		for (const auto& Link : *Links)
			if (EnumHasAllFlags(Link.Access, ECityRoadAccess::Pedestrian) && !OutDistances.Contains(Link.Cell))
			{
				OutDistances.Add(Link.Cell, NextDistance);
				Queue.Add(Link.Cell);
			}
	}
}

const FCityRoadEdge* FCityRoadNetworkSnapshot::FindRouteSegment(const FCityCellCoord& Start, const FCityCellCoord& End,
                                                                const ECityRoadAccess Access, int32& OutCellIndex,
                                                                bool& bOutReverse) const
{
	OutCellIndex = 0;
	bOutReverse = false;
	if (bUsePreparedRouting && RoutingData)
	{
		if (RoutingTiming)
			++RoutingTiming->IndexedSegments;
		const auto* Links = RoutingData->Links.Find(Start);
		if (!Links)
			return nullptr;
		for (const auto& Link : *Links)
			if (Link.Cell == End && EnumHasAnyFlags(Link.Access, Access))
			{
				OutCellIndex = Link.CellIndex;
				bOutReverse = Link.bReverse;
				return &Edges[Link.EdgeIndex];
			}
		return nullptr;
	}
	for (const auto& Edge : Edges)
	{
		if (!EnumHasAnyFlags(Edge.Access, Access))
			continue;
		for (int32 Index = 0; Index + 1 < Edge.Cells.Num(); ++Index)
		{
			if (Edge.Cells[Index] == Start && Edge.Cells[Index + 1] == End)
			{
				OutCellIndex = Index;
				return &Edge;
			}
			if (Edge.Cells[Index + 1] == Start && Edge.Cells[Index] == End)
			{
				OutCellIndex = Edge.Cells.Num() - 2 - Index;
				bOutReverse = true;
				return &Edge;
			}
		}
	}
	return nullptr;
}

void FCityRoadNetworkBuilder::Build(const FCityPlacementModel& City, const FCityRoadDefinition& DefaultDefinition,
                                    FCityRoadNetworkSnapshot& OutNetwork)
{
	OutNetwork = FCityRoadNetworkSnapshot();
	OutNetwork.SourceRevision = City.GetRevision();
	OutNetwork.Dimensions = City.GetGrid().GetDimensions();
	const auto EarlierCell = [&](const FCityCellCoord& A, const FCityCellCoord& B)
	{ return CellKey(A, OutNetwork.Dimensions) < CellKey(B, OutNetwork.Dimensions); };
	TArray<FCityCellCoord> Roads = City.GetRoadCells().Array();
	Roads.Sort(EarlierCell);
	TMap<FCityCellCoord, int32> NodeIds;
	for (const FCityCellCoord& Cell : Roads)
	{
		if (IsNodeCell(City, Cell))
		{
			const int32 Id = OutNetwork.Nodes.Num();
			NodeIds.Add(Cell, Id);
			OutNetwork.Nodes.Add({Id, Cell, City.GetRoadNeighborMask(Cell)});
		}
	}
	if (NodeIds.IsEmpty() && !Roads.IsEmpty())
	{
		NodeIds.Add(Roads[0], 0);
		OutNetwork.Nodes.Add({0, Roads[0], City.GetRoadNeighborMask(Roads[0])});
	}

	TSet<uint64> VisitedLinks;
	for (const FCityRoadNode& StartNode : OutNetwork.Nodes)
	{
		TArray<FCityCellCoord> Neighbors;
		RoadNeighbors(City, StartNode.Cell, Neighbors);
		Neighbors.Sort(EarlierCell);
		for (const FCityCellCoord& First : Neighbors)
		{
			if (VisitedLinks.Contains(LinkKey(StartNode.Cell, First, OutNetwork.Dimensions)))
				continue;
			FCityRoadEdge Edge;
			Edge.StartNodeId = StartNode.Id;
			Edge.Cells.Add(StartNode.Cell);
			FCityCellCoord Previous = StartNode.Cell;
			FCityCellCoord Current = First;
			while (true)
			{
				VisitedLinks.Add(LinkKey(Previous, Current, OutNetwork.Dimensions));
				Edge.Cells.Add(Current);
				if (const int32* EndId = NodeIds.Find(Current))
				{
					Edge.EndNodeId = *EndId;
					break;
				}
				TArray<FCityCellCoord> NextCells;
				RoadNeighbors(City, Current, NextCells);
				NextCells.Remove(Previous);
				if (NextCells.IsEmpty())
					break;
				NextCells.Sort(EarlierCell);
				Previous = Current;
				Current = NextCells[0];
			}
			if (Edge.EndNodeId == INDEX_NONE)
				continue;
			FCityRoadDefinition Definition =
			    ResolveDefinition(City.GetRoadDefinitionId(Edge.Cells[1]), DefaultDefinition);
			// A one-cell raised class transition belongs to the narrower approach.
			// This preserves its lanes, speed and walking access at a wide junction.
			if (Edge.Cells.Num() == 2 && Edge.Cells[0].RoadLevel == 1 && Edge.Cells[1].RoadLevel == 1)
			{
				const auto Other = ResolveDefinition(City.GetRoadDefinitionId(Edge.Cells[0]), DefaultDefinition);
				if (Other.PlacementCostPerCell < Definition.PlacementCostPerCell)
					Definition = Other;
			}
			Edge.Id = OutNetwork.Edges.Num();
			Edge.DefinitionId = Definition.Id;
			Edge.RoadClass = Definition.RoadClass;
			Edge.ForwardLanes = Definition.ForwardLanes;
			Edge.ReverseLanes = Definition.ReverseLanes;
			Edge.SpeedUnitsPerStep = Definition.SpeedUnitsPerStep;
			Edge.Capacity = (Definition.ForwardLanes + Definition.ReverseLanes) * Definition.CapacityPerLane;
			Edge.Access = Definition.Access;
			OutNetwork.Edges.Add(MoveTemp(Edge));
		}
	}

	for (const FCityRoadNode& Node : OutNetwork.Nodes)
	{
		TArray<int32> IncidentEdges;
		for (const FCityRoadEdge& Edge : OutNetwork.Edges)
		{
			if (Edge.StartNodeId == Node.Id || Edge.EndNodeId == Node.Id)
				IncidentEdges.Add(Edge.Id);
		}
		IncidentEdges.Sort();
		for (const int32 IncomingId : IncidentEdges)
		{
			const FCityRoadEdge& Incoming = OutNetwork.Edges[IncomingId];
			const int32 IncomingLanes = Incoming.EndNodeId == Node.Id ? Incoming.ForwardLanes : Incoming.ReverseLanes;
			if (IncomingLanes <= 0)
				continue;
			for (const int32 OutgoingId : IncidentEdges)
			{
				if (IncomingId == OutgoingId)
					continue;
				const FCityRoadEdge& Outgoing = OutNetwork.Edges[OutgoingId];
				const int32 OutgoingLanes =
				    Outgoing.StartNodeId == Node.Id ? Outgoing.ForwardLanes : Outgoing.ReverseLanes;
				if (OutgoingLanes <= 0 || !EnumHasAnyFlags(Incoming.Access & Outgoing.Access,
				                                           ECityRoadAccess::Car | ECityRoadAccess::Freight))
					continue;
				FCityLaneConnector Connector;
				Connector.Id = OutNetwork.Connectors.Num();
				Connector.NodeId = Node.Id;
				Connector.IncomingEdgeId = IncomingId;
				Connector.OutgoingEdgeId = OutgoingId;
				Connector.Turn =
				    ClassifyTurn(DirectionAtNode(Incoming, Node.Id, false), DirectionAtNode(Outgoing, Node.Id, true));
				OutNetwork.Connectors.Add(Connector);
			}
		}
	}
	// The original lane-zero turn remains the stable base connector. Expand
	// its declared lanes so all lanes can cross joins/turns and merge down.
	const int32 BaseConnectorCount = OutNetwork.Connectors.Num();
	for (int32 I = 0; I < BaseConnectorCount; ++I)
	{
		const auto Base = OutNetwork.Connectors[I];
		const auto& In = OutNetwork.Edges[Base.IncomingEdgeId];
		const auto& Out = OutNetwork.Edges[Base.OutgoingEdgeId];
		const int32 InCount = In.EndNodeId == Base.NodeId ? In.ForwardLanes : In.ReverseLanes;
		const int32 OutCount = Out.StartNodeId == Base.NodeId ? Out.ForwardLanes : Out.ReverseLanes;
		for (int32 A = 0; A < InCount; ++A)
			for (int32 B = 0; B < OutCount; ++B)
				if (A || B)
				{
					auto Extra = Base; Extra.Id = OutNetwork.Connectors.Num();
					Extra.IncomingLane = A; Extra.OutgoingLane = B;
					OutNetwork.Connectors.Add(Extra);
				}
	}
	OutNetwork.PrepareRouting();
}

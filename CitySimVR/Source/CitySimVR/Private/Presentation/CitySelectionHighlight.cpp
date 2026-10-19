#include "Presentation/CitySelectionHighlight.h"
namespace
{
	bool Owns(const FCityRoadStructure& S, const FCityCellCoord& C)
	{
		if (C.X < FMath::Min(S.Start.X, S.End.X) || C.X > FMath::Max(S.Start.X, S.End.X) ||
		    C.Y < FMath::Min(S.Start.Y, S.End.Y) || C.Y > FMath::Max(S.Start.Y, S.End.Y))
			return false;
		if (C.X == S.Start.X && C.Y == S.Start.Y)
			return C.RoadLevel == S.Start.RoadLevel;
		if (C.X == S.End.X && C.Y == S.End.Y)
			return C.RoadLevel == S.End.RoadLevel;
		return C.RoadLevel == 1;
	}
} // namespace
FCitySelectionHighlightPlan CitySelectionHighlight::Build(const FCityPlacementModel& City, const FCityCellCoord& Cell,
                                                          const bool bDemolish)
{
	FCitySelectionHighlightPlan Plan;
	if (!City.GetGrid().IsValidCell(Cell))
		return Plan;
	const uint64 Target = City.GetGrid().GetOccupant(Cell);
	if (const auto* Entity = City.GetEntities().Find(Target))
	{
		Plan.EntityId = Target;
		for (int32 Y = 0; Y < Entity->FootprintSize.Y; ++Y)
			for (int32 X = 0; X < Entity->FootprintSize.X; ++X)
				Plan.Cells.Add({Entity->Anchor.X + X, Entity->Anchor.Y + Y});
		return Plan;
	}
	if (!City.IsRoad(Cell))
	{
		if (!bDemolish)
			Plan.Cells.Add(Cell);
		return Plan;
	}
	Plan.bRoad = true;
	const auto* Span = bDemolish ? City.FindRoadStructure(Cell) : nullptr;
	if (!Span)
	{
		Plan.Cells.Add(Cell);
		return Plan;
	}
	TArray<FCityCellCoord> Path;
	FCityPlacementModel::BuildOrthogonalRoadPath(Span->Start, Span->End, Path);
	for (int32 I = 0; I < Path.Num(); ++I)
	{
		auto C = Path[I];
		if (I > 0 && I < Path.Num() - 1)
			C.RoadLevel = 1;
		bool Retained = C.RoadLevel == 0;
		for (const auto& Other : City.GetRoadStructures())
			if (&Other != Span && Owns(Other, C))
				Retained = true;
		(Retained ? Plan.RetainedCells : Plan.Cells).Add(C);
	}
	return Plan;
}

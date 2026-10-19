#include "Interaction/CityAreaSelection.h"

FCityAreaSelection CityAreaSelection::MakeInclusiveRectangle(const FCityCellCoord& FirstCorner,
                                                             const FCityCellCoord& SecondCorner)
{
	FCityAreaSelection Result;
	Result.Anchor =
	    FCityCellCoord(FMath::Min(FirstCorner.X, SecondCorner.X), FMath::Min(FirstCorner.Y, SecondCorner.Y));
	Result.Size =
	    FIntPoint(FMath::Abs(FirstCorner.X - SecondCorner.X) + 1, FMath::Abs(FirstCorner.Y - SecondCorner.Y) + 1);
	return Result;
}

void CityAreaSelection::BuildCells(const FCityAreaSelection& Selection, TArray<FCityCellCoord>& OutCells)
{
	OutCells.Reset();
	OutCells.Reserve(Selection.GetCellCount());
	for (int32 Y = Selection.Anchor.Y; Y < Selection.Anchor.Y + Selection.Size.Y; ++Y)
	{
		for (int32 X = Selection.Anchor.X; X < Selection.Anchor.X + Selection.Size.X; ++X)
		{
			OutCells.Emplace(X, Y);
		}
	}
}

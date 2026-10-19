#include "Simulation/CityGrid.h"

FIntPoint FCityCellFootprint::GetRotatedSize(const int32 QuarterTurnsClockwise) const
{
	const int32 NormalizedTurns = ((QuarterTurnsClockwise % 4) + 4) % 4;
	return NormalizedTurns % 2 == 0 ? FIntPoint(Width, Height) : FIntPoint(Height, Width);
}

bool FCityGridDimensions::IsValid() const
{
	return Width == Height &&
	       (Width == 64 || Width == 80 || (Width == CITYSIM_CAPACITY_GRID_SIZE && Width >= 64 && Width <= 192));
}

int32 FCityGridDimensions::GetCellCount() const
{
	return IsValid() ? Width * Height : 0;
}

bool FCityGridDimensions::IsValidCell(const FCityCellCoord& Cell) const
{
	return IsValid() && Cell.X >= 0 && Cell.X < Width && Cell.Y >= 0 && Cell.Y < Height && Cell.RoadLevel >= 0 &&
	       Cell.RoadLevel <= 1;
}

FCityGrid::FCityGrid(const FCityGridDimensions InDimensions) : Dimensions(InDimensions)
{
	checkf(Dimensions.IsValid(), TEXT("Validate city dimensions before allocating occupancy"));
	GroundCellCount = Dimensions.GetCellCount();
	Occupancy.Init(EmptyEntityId, GroundCellCount * 2);
}

bool FCityGrid::IsValidCell(const FCityCellCoord& Cell) const
{
	return Cell.X >= 0 && Cell.X < Dimensions.GetWidth() && Cell.Y >= 0 && Cell.Y < Dimensions.GetHeight() &&
	       Cell.RoadLevel >= 0 && Cell.RoadLevel <= 1;
}

bool FCityGrid::IsFootprintInBounds(const FCityCellCoord& Anchor, const FCityCellFootprint& Footprint,
                                    const int32 QuarterTurnsClockwise) const
{
	const FIntPoint Size = Footprint.GetRotatedSize(QuarterTurnsClockwise);
	return Size.X > 0 && Size.Y > 0 && IsValidCell(Anchor) && Anchor.X <= Dimensions.GetWidth() - Size.X &&
	       Anchor.Y <= Dimensions.GetHeight() - Size.Y;
}

uint64 FCityGrid::GetOccupant(const FCityCellCoord& Cell) const
{
	return IsValidCell(Cell) ? Occupancy[ToIndex(Cell)] : EmptyEntityId;
}

bool FCityGrid::CanOccupy(const FCityCellCoord& Anchor, const FCityCellFootprint& Footprint,
                          const int32 QuarterTurnsClockwise) const
{
	if (!IsFootprintInBounds(Anchor, Footprint, QuarterTurnsClockwise))
	{
		return false;
	}

	const FIntPoint Size = Footprint.GetRotatedSize(QuarterTurnsClockwise);
	for (int32 Y = Anchor.Y; Y < Anchor.Y + Size.Y; ++Y)
	{
		for (int32 X = Anchor.X; X < Anchor.X + Size.X; ++X)
		{
			if (Occupancy[ToIndex(FCityCellCoord(X, Y, Anchor.RoadLevel))] != EmptyEntityId)
			{
				return false;
			}
		}
	}
	return true;
}

bool FCityGrid::TryOccupy(const FCityCellCoord& Anchor, const FCityCellFootprint& Footprint, const uint64 EntityId,
                          const int32 QuarterTurnsClockwise)
{
	if (EntityId == EmptyEntityId || !CanOccupy(Anchor, Footprint, QuarterTurnsClockwise))
	{
		return false;
	}

	const FIntPoint Size = Footprint.GetRotatedSize(QuarterTurnsClockwise);
	for (int32 Y = Anchor.Y; Y < Anchor.Y + Size.Y; ++Y)
	{
		for (int32 X = Anchor.X; X < Anchor.X + Size.X; ++X)
		{
			Occupancy[ToIndex(FCityCellCoord(X, Y, Anchor.RoadLevel))] = EntityId;
		}
	}
	return true;
}

bool FCityGrid::ClearCell(const FCityCellCoord& Cell, const uint64 ExpectedEntityId)
{
	if (!IsValidCell(Cell) || ExpectedEntityId == EmptyEntityId || GetOccupant(Cell) != ExpectedEntityId)
	{
		return false;
	}

	Occupancy[ToIndex(Cell)] = EmptyEntityId;
	return true;
}

void FCityGrid::ClearEntity(const uint64 EntityId)
{
	if (EntityId == EmptyEntityId)
	{
		return;
	}

	for (uint64& Occupant : Occupancy)
	{
		if (Occupant == EntityId)
		{
			Occupant = EmptyEntityId;
		}
	}
}

int32 FCityGrid::ToIndex(const FCityCellCoord& Cell) const
{
	return Cell.RoadLevel * GroundCellCount + Cell.Y * Dimensions.GetWidth() + Cell.X;
}

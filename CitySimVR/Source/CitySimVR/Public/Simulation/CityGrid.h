#pragma once

#include "CoreMinimal.h"
#include "CityGrid.generated.h"

// Dedicated Windows Development capacity builds only. Normal targets remain 64.
#ifndef CITYSIM_CAPACITY_GRID_SIZE
#define CITYSIM_CAPACITY_GRID_SIZE 64
#endif

struct FCityCellCoord;

// Value owned by each city, never a process-wide current size. The 80-cell
// option is an internal prototype until terrain, persistence and UI migrate.
class CITYSIMVR_API FCityGridDimensions
{
public:
	constexpr FCityGridDimensions() = default;
	explicit constexpr FCityGridDimensions(int32 InWidth, int32 InHeight) : Width(InWidth), Height(InHeight) {}
	constexpr int32 GetWidth() const
	{
		return Width;
	}
	constexpr int32 GetHeight() const
	{
		return Height;
	}
	bool IsValid() const;
	int32 GetCellCount() const;
	bool IsValidCell(const FCityCellCoord& Cell) const;
	bool operator==(const FCityGridDimensions& Other) const = default;

private:
	int32 Width = CITYSIM_CAPACITY_GRID_SIZE;
	int32 Height = CITYSIM_CAPACITY_GRID_SIZE;
};

USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityCellCoord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Grid")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Grid")
	int32 Y = 0;

	// Terrain/building coordinates use level0. Explicit raised road interiors use
	// level1, including ramps; their dry endpoint portals remain level0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Grid")
	int32 RoadLevel = 0;

	FCityCellCoord() = default;
	FCityCellCoord(const int32 InX, const int32 InY, const int32 InRoadLevel = 0)
	    : X(InX), Y(InY), RoadLevel(InRoadLevel)
	{
	}

	bool operator==(const FCityCellCoord& Other) const
	{
		return X == Other.X && Y == Other.Y && RoadLevel == Other.RoadLevel;
	}
};

FORCEINLINE uint32 GetTypeHash(const FCityCellCoord& Coord)
{
	const uint32 XY = HashCombine(GetTypeHash(Coord.X), GetTypeHash(Coord.Y));
	return Coord.RoadLevel == 0 ? XY : HashCombine(XY, GetTypeHash(Coord.RoadLevel));
}

USTRUCT(BlueprintType)
struct CITYSIMVR_API FCityCellFootprint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Grid", meta = (ClampMin = "1"))
	int32 Width = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City Grid", meta = (ClampMin = "1"))
	int32 Height = 1;

	FCityCellFootprint() = default;
	FCityCellFootprint(const int32 InWidth, const int32 InHeight) : Width(InWidth), Height(InHeight) {}

	FIntPoint GetRotatedSize(int32 QuarterTurnsClockwise) const;
};

class CITYSIMVR_API FCityGrid
{
public:
	static constexpr int32 Width = CITYSIM_CAPACITY_GRID_SIZE;
	static constexpr int32 Height = CITYSIM_CAPACITY_GRID_SIZE;
	static constexpr int32 CellCount = Width * Height;
	static constexpr uint64 EmptyEntityId = 0;

	explicit FCityGrid(FCityGridDimensions InDimensions = FCityGridDimensions());
	const FCityGridDimensions& GetDimensions() const
	{
		return Dimensions;
	}

	bool IsValidCell(const FCityCellCoord& Cell) const;
	bool IsFootprintInBounds(const FCityCellCoord& Anchor, const FCityCellFootprint& Footprint,
	                         int32 QuarterTurnsClockwise = 0) const;
	uint64 GetOccupant(const FCityCellCoord& Cell) const;
	bool CanOccupy(const FCityCellCoord& Anchor, const FCityCellFootprint& Footprint,
	               int32 QuarterTurnsClockwise = 0) const;
	bool TryOccupy(const FCityCellCoord& Anchor, const FCityCellFootprint& Footprint, uint64 EntityId,
	               int32 QuarterTurnsClockwise = 0);
	bool ClearCell(const FCityCellCoord& Cell, uint64 ExpectedEntityId);
	void ClearEntity(uint64 EntityId);

private:
	int32 ToIndex(const FCityCellCoord& Cell) const;

	FCityGridDimensions Dimensions;
	int32 GroundCellCount = 0;
	TArray<uint64> Occupancy;
};
